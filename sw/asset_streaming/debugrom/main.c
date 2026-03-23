/**
 * SC64 Asset Streaming - Debug Test ROM
 *
 * Interactive cache debugger that runs on real N64 + SC64 hardware.
 * Displays real-time cache state, lets you load/evict/pin/unpin assets
 * with the controller, and measures timing.
 *
 * Controls:
 *   D-Pad Up/Down   - Select asset ID
 *   A               - Load selected asset (transient)
 *   B               - Load + Pin selected asset (handle)
 *   Z               - Unpin/Release selected asset
 *   L               - Prefetch selected asset
 *   R               - Flush all non-pinned cache slots
 *   Start           - Run full automated test sequence
 *   C-Up/C-Down     - Scroll cache slot view
 *   C-Left          - Show timing stats
 *   C-Right         - Show pin event log
 *
 * Build:
 *   Requires mips64-elf-gcc toolchain (same as SC64 bootloader).
 *   See debugrom/Makefile.
 *
 * Archive:
 *   Pack test assets with: make -C debugrom archive
 *   Write to SD card at sector 0x10000.
 */

#include "display.h"
#include "io.h"
#include "joybus.h"
#include "sc64.h"
#include "exception.h"

#include "../src/asset_manager.h"
#include "../src/asset_debug.h"
#include "../src/asset_cache.h"

#include <string.h>
#include <stdbool.h>

/* -------------------------------------------------------------------
 * Configuration
 * ------------------------------------------------------------------- */

#define ARCHIVE_SECTOR      (0x10000)   /* 32 MB into SD card */
#define CACHE_BASE          ((uint8_t *)0xB1000000)  /* SDRAM after 16 MB */
#define CACHE_SIZE          (4 * 1024 * 1024)   /* 4 MB for testing */
#define SLOT_SIZE           (256 * 1024)         /* 256 KB per slot = 16 slots */
#define STAGING_SIZE        (512 * 1024)         /* 512 KB staging */

#define NUM_TEST_ASSETS     (24)    /* How many test assets in the archive */
#define INPUT_DEBOUNCE_MS   (150)   /* Button repeat delay */

/* Timer: CP0 Count runs at CPU_FREQ/2 = 46.875 MHz */
#define TICKS_PER_US        (47)    /* ~46.875, close enough */
#define TICKS_PER_MS        (46875)

/* -------------------------------------------------------------------
 * State
 * ------------------------------------------------------------------- */

static uint8_t staging[STAGING_SIZE] __attribute__((aligned(16)));
static asset_manager_t mgr;

static int selected_asset = 1;
static int scroll_offset = 0;
static int view_mode = 0;          /* 0=cache, 1=timing, 2=events */

/* Timing measurements */
#define MAX_TIMINGS 64
static struct {
    uint32_t asset_id;
    uint32_t ticks;
    bool     cache_hit;
} timings[MAX_TIMINGS];
static int timing_count = 0;

/* Handle storage for pinned assets */
#define MAX_HANDLES 16
static asset_handle_t handles[MAX_HANDLES];
static int handle_count = 0;

/* Auto-test state */
static bool auto_test_running = false;
static int auto_test_step = 0;

/* -------------------------------------------------------------------
 * Debug log -> display
 * ------------------------------------------------------------------- */

static char log_lines[8][80];
static int log_line_count = 0;

static int dbg_log(const char *fmt, ...) {
    if (log_line_count >= 8) {
        /* Shift lines up */
        for (int i = 0; i < 7; i++) {
            memcpy(log_lines[i], log_lines[i + 1], 80);
        }
        log_line_count = 7;
    }
    va_list args;
    va_start(args, fmt);
    vsnprintf(log_lines[log_line_count], 80, fmt, args);
    va_end(args);
    /* Strip newline */
    char *nl = strchr(log_lines[log_line_count], '\n');
    if (nl) *nl = '\0';
    log_line_count++;
    return 0;
}

/* -------------------------------------------------------------------
 * Timing helpers
 * ------------------------------------------------------------------- */

static inline uint32_t timer_now(void) {
    return c0_count();
}

static inline uint32_t timer_us(uint32_t start, uint32_t end) {
    return (end - start) / TICKS_PER_US;
}

static void record_timing(uint32_t asset_id, uint32_t ticks, bool hit) {
    if (timing_count < MAX_TIMINGS) {
        timings[timing_count].asset_id = asset_id;
        timings[timing_count].ticks = ticks;
        timings[timing_count].cache_hit = hit;
        timing_count++;
    }
}

/* -------------------------------------------------------------------
 * Timed asset operations
 * ------------------------------------------------------------------- */

static void *timed_load(uint32_t id) {
    uint32_t t0 = timer_now();
    bool was_cached = asset_is_cached(&mgr, id);
    void *data = asset_load(&mgr, id);
    uint32_t t1 = timer_now();

    record_timing(id, t1 - t0, was_cached);
    return data;
}

static asset_handle_t timed_acquire(uint32_t id) {
    uint32_t t0 = timer_now();
    bool was_cached = asset_is_cached(&mgr, id);
    asset_handle_t h = asset_acquire(&mgr, id);
    uint32_t t1 = timer_now();

    record_timing(id, t1 - t0, was_cached);
    return h;
}

/* -------------------------------------------------------------------
 * Drawing: Cache slot visualization
 * ------------------------------------------------------------------- */

static void draw_header(void) {
    display_printf("SC64 Asset Streaming Debug ROM\n");
    display_printf("==============================\n");
}

static const char *state_str(slot_state_t s) {
    switch (s) {
        case SLOT_EMPTY:   return "EMPTY  ";
        case SLOT_LOADING: return "LOADING";
        case SLOT_READY:   return "READY  ";
        default:           return "???    ";
    }
}

static void draw_cache_view(void) {
    display_printf("Cache Slots (%u/%u used, %d%% hit rate)\n",
                   mgr.cache.config.slot_count - mgr.cache.pin_count,
                   mgr.cache.config.slot_count,
                   cache_hit_rate(&mgr.cache));
    display_printf("Slot  State    Asset  Pin  LRU    Size\n");
    display_printf("----  -------  -----  ---  -----  ------\n");

    uint32_t max_visible = 10;
    uint32_t start = (uint32_t)scroll_offset;
    if (start >= mgr.cache.config.slot_count) start = 0;

    for (uint32_t i = start; i < mgr.cache.config.slot_count && i < start + max_visible; i++) {
        cache_slot_t *s = &mgr.cache.slots[i];
        char pin_ch = s->pin_count > 0 ? 'P' : ' ';
        if (s->state == SLOT_EMPTY) {
            display_printf(" %2u   %s     -    -      -       -\n",
                           i, state_str(s->state));
        } else {
            display_printf(" %2u   %s  %4u  %c%u  %5u  %5uK\n",
                           i, state_str(s->state),
                           s->asset_id,
                           pin_ch, s->pin_count,
                           s->lru_counter,
                           s->data_size / 1024);
        }
    }
}

static void draw_timing_view(void) {
    display_printf("Load Timings (last %d)\n", timing_count);
    display_printf("Asset  Hit/Miss  Time (us)\n");
    display_printf("-----  --------  ---------\n");

    int start = timing_count > 12 ? timing_count - 12 : 0;
    for (int i = start; i < timing_count; i++) {
        uint32_t us = timings[i].ticks / TICKS_PER_US;
        display_printf(" %4u  %s   %7u\n",
                       timings[i].asset_id,
                       timings[i].cache_hit ? "HIT " : "MISS",
                       us);
    }
}

static void draw_event_view(void) {
    display_printf("Debug Event Log\n");
    display_printf("---------------\n");

    for (int i = 0; i < log_line_count && i < 12; i++) {
        display_printf("%s\n", log_lines[i]);
    }
}

static void draw_controls(void) {
    display_printf("\n");
    display_printf("Selected: Asset %d  ", selected_asset);
    display_printf("[%s]\n",
                   asset_is_cached(&mgr, selected_asset) ?
                   (asset_is_pinned(&mgr, selected_asset) ? "CACHED+PINNED" : "CACHED") :
                   "NOT LOADED");
    display_printf("Up/Dn=Select A=Load B=Pin Z=Unpin\n");
    display_printf("L=Prefetch R=Flush Start=AutoTest\n");
    display_printf("C-L=Timing C-R=Events C-U/D=Scroll\n");
}

/* -------------------------------------------------------------------
 * Auto-test sequence
 * ------------------------------------------------------------------- */

static void auto_test_tick(void) {
    switch (auto_test_step) {
        case 0:
            dbg_log("=== Auto Test: Cold Loads ===");
            for (int i = 1; i <= 8; i++) {
                timed_load(i);
            }
            auto_test_step++;
            break;

        case 1:
            dbg_log("=== Auto Test: Hot Loads ===");
            for (int i = 1; i <= 8; i++) {
                timed_load(i);
            }
            auto_test_step++;
            break;

        case 2:
            dbg_log("=== Auto Test: Pin + Evict ===");
            {
                /* Pin asset 1, then load enough to force eviction */
                asset_handle_t h = timed_acquire(1);
                if (asset_handle_valid(h)) {
                    handles[handle_count++] = h;
                }
                for (int i = 2; i <= NUM_TEST_ASSETS && i <= 20; i++) {
                    timed_load(i);
                }
                /* Verify pinned asset survived */
                if (asset_is_cached(&mgr, 1)) {
                    dbg_log("Pin survived eviction: OK");
                } else {
                    dbg_log("Pin survived eviction: FAILED!");
                }
            }
            auto_test_step++;
            break;

        case 3:
            dbg_log("=== Auto Test: Flush + Reload ===");
            asset_flush(&mgr);
            for (int i = 1; i <= 4; i++) {
                timed_load(i);
            }
            auto_test_step++;
            break;

        case 4:
            dbg_log("=== Auto Test: Complete ===");
            /* Release any handles from the test */
            for (int i = 0; i < handle_count; i++) {
                asset_release(&mgr, handles[i]);
            }
            handle_count = 0;
            auto_test_running = false;
            auto_test_step = 0;
            break;
    }
}

/* -------------------------------------------------------------------
 * Input handling
 * ------------------------------------------------------------------- */

static uint32_t last_input_time = 0;

static bool debounced(void) {
    uint32_t now = timer_now();
    if ((now - last_input_time) > (INPUT_DEBOUNCE_MS * TICKS_PER_MS)) {
        last_input_time = now;
        return true;
    }
    return false;
}

static void handle_input(void) {
    joybus_controller_state_t pad;
    if (!joybus_get_controller_state(0, &pad)) return;

    if (pad.up && debounced()) {
        selected_asset++;
        if (selected_asset > NUM_TEST_ASSETS) selected_asset = 1;
    }
    if (pad.down && debounced()) {
        selected_asset--;
        if (selected_asset < 1) selected_asset = NUM_TEST_ASSETS;
    }

    if (pad.a && debounced()) {
        dbg_log("Load asset %d (transient)", selected_asset);
        timed_load(selected_asset);
    }

    if (pad.b && debounced()) {
        dbg_log("Acquire asset %d (pinned)", selected_asset);
        asset_handle_t h = timed_acquire(selected_asset);
        if (asset_handle_valid(h) && handle_count < MAX_HANDLES) {
            handles[handle_count++] = h;
        }
    }

    if (pad.z && debounced()) {
        /* Release the most recent handle for this asset */
        for (int i = handle_count - 1; i >= 0; i--) {
            if (handles[i].asset_id == (uint32_t)selected_asset) {
                dbg_log("Release asset %d", selected_asset);
                asset_release(&mgr, handles[i]);
                /* Remove from handle list */
                for (int j = i; j < handle_count - 1; j++) {
                    handles[j] = handles[j + 1];
                }
                handle_count--;
                break;
            }
        }
    }

    if (pad.l && debounced()) {
        dbg_log("Prefetch asset %d", selected_asset);
        uint32_t t0 = timer_now();
        asset_prefetch(&mgr, selected_asset);
        uint32_t t1 = timer_now();
        record_timing(selected_asset, t1 - t0, false);
    }

    if (pad.r && debounced()) {
        dbg_log("Flush cache (non-pinned)");
        asset_flush(&mgr);
    }

    if (pad.start && debounced()) {
        dbg_log("Starting auto test...");
        auto_test_running = true;
        auto_test_step = 0;
        timing_count = 0;
    }

    /* View switching */
    if (pad.c_left && debounced())  view_mode = 1;  /* Timing */
    if (pad.c_right && debounced()) view_mode = 2;  /* Events */
    if (pad.c_up && debounced())    { if (view_mode == 0) scroll_offset = (scroll_offset > 0) ? scroll_offset - 1 : 0; else view_mode = 0; }
    if (pad.c_down && debounced())  { if (view_mode == 0) scroll_offset++; else view_mode = 0; }
}

/* -------------------------------------------------------------------
 * Main
 * ------------------------------------------------------------------- */

void main(void) {
    /* Initialize display */
    display_init(NULL);

    /* Unlock SC64 registers */
    sc64_unlock();

    /* Show init screen */
    display_printf("SC64 Asset Streaming Debug ROM\n");
    display_printf("Initializing...\n\n");

    /* Set up debug logging */
    asset_debug_set_log(dbg_log);

    /* Initialize asset manager */
    asset_error_t err = asset_init(&mgr, &(asset_init_config_t){
        .archive_base_sector = ARCHIVE_SECTOR,
        .cache_base = CACHE_BASE,
        .cache_size = CACHE_SIZE,
        .slot_size = SLOT_SIZE,
        .staging_buffer = staging,
        .staging_size = STAGING_SIZE,
        .backend = NULL,    /* Real SC64 hardware */
    });

    if (err != ASSET_OK) {
        display_printf("ERROR: asset_init failed!\n");
        display_printf("  %s\n", asset_error_string(err));
        display_printf("\nMake sure test archive is on SD card\n");
        display_printf("at sector offset 0x%X.\n", ARCHIVE_SECTOR);
        display_printf("\nBuild with:\n");
        display_printf("  make -C debugrom archive\n");
        display_printf("  # Write assets.bin to SD\n");

        while (1);  /* Halt */
    }

    display_printf("Asset manager initialized OK!\n");
    display_printf("  %u assets in archive\n", mgr.index_count);
    display_printf("  %u cache slots (%u KB each)\n",
                   mgr.cache.config.slot_count,
                   mgr.cache.config.slot_size / 1024);
    display_printf("\nReady. Press any button.\n");

    /* Wait for button press */
    joybus_controller_state_t pad;
    do {
        delay_ms(16);
        joybus_get_controller_state(0, &pad);
    } while (!pad.a && !pad.b && !pad.start);

    /* Main loop */
    while (1) {
        /* Clear and redraw */
        display_init(NULL);
        draw_header();

        switch (view_mode) {
            case 0: draw_cache_view(); break;
            case 1: draw_timing_view(); break;
            case 2: draw_event_view(); break;
        }

        draw_controls();

        /* Handle input */
        handle_input();

        /* Run auto-test if active */
        if (auto_test_running) {
            auto_test_tick();
        }

        /* Frame pacing (~30fps) */
        delay_ms(33);
    }
}
