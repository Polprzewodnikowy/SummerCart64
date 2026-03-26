/*
 * TRX-N64: Platform abstraction — N64 hardware implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "n64_platform.h"
#include "io.h"
#include "joybus.h"
#include <string.h>

/* -------------------------------------------------------------------
 * Video initialization
 * ------------------------------------------------------------------- */

void n64_video_init(n64_video_mode_t mode) {
    volatile uint32_t *vi = (volatile uint32_t *)0xA4400000;

    if (mode == N64_VIDEO_NTSC) {
        vi[0x00/4] = 0x00003203;    /* STATUS: 16-bit, AA+resamp */
        /* ORIGIN set per-frame */
        vi[0x08/4] = 320;           /* WIDTH */
        vi[0x0C/4] = 512;           /* V_INTR */
        vi[0x10/4] = 0;             /* V_CURRENT (reset) */
        vi[0x14/4] = 0x03E52239;    /* BURST */
        vi[0x18/4] = 0x0000020D;    /* V_SYNC (525 lines NTSC) */
        vi[0x1C/4] = 0x00000C15;    /* H_SYNC */
        vi[0x20/4] = 0x0C150C15;    /* H_SYNC_LEAP */
        vi[0x24/4] = 0x006C02EC;    /* H_VIDEO */
        vi[0x28/4] = 0x002501FF;    /* V_VIDEO */
        vi[0x2C/4] = 0x000E0204;    /* V_BURST */
        vi[0x30/4] = 0x00000200;    /* X_SCALE (1.0 = 0x200) */
        vi[0x34/4] = 0x00000400;    /* Y_SCALE (1.0 = 0x400) */
    } else {
        /* PAL timing */
        vi[0x00/4] = 0x00003203;
        vi[0x08/4] = 320;
        vi[0x0C/4] = 512;
        vi[0x18/4] = 0x00000271;    /* V_SYNC (625 lines PAL) */
        vi[0x1C/4] = 0x00150C69;
        vi[0x20/4] = 0x0C6F0C6E;
        vi[0x24/4] = 0x00800300;
        vi[0x28/4] = 0x005F0239;
        vi[0x2C/4] = 0x0009026B;
        vi[0x30/4] = 0x00000200;
        vi[0x34/4] = 0x00000400;
    }
}

void n64_video_set_framebuffer(uint16_t *fb) {
    volatile uint32_t *vi = (volatile uint32_t *)0xA4400000;
    vi[0x04/4] = (uint32_t)(uintptr_t)fb & 0x1FFFFFFF;
}

void n64_video_wait_vsync(void) {
    volatile uint32_t *vi = (volatile uint32_t *)0xA4400000;
    /* Wait for V_CURRENT to reach bottom of frame */
    while (vi[0x10/4] < 480) {}
    while (vi[0x10/4] >= 480) {}
}

/* -------------------------------------------------------------------
 * Controller input
 * ------------------------------------------------------------------- */

#define DEADZONE 12  /* Analog stick dead zone */

void n64_input_init(void) {
    joybus_controller_info_t info;
    joybus_get_controller_info(0, &info, true);
}

void n64_input_poll(n64_input_state_t *state) {
    joybus_controller_state_t pad;

    state->buttons_prev = state->buttons;
    state->buttons = 0;

    if (!joybus_get_controller_state(0, &pad)) return;

    /* Map N64 buttons → TR1 inputs */
    if (pad.a)      state->buttons |= TR_INPUT_ACTION;
    if (pad.b)      state->buttons |= TR_INPUT_JUMP;
    if (pad.z)      state->buttons |= TR_INPUT_DRAW;
    if (pad.start)  state->buttons |= TR_INPUT_OPTION | TR_INPUT_PAUSE;
    if (pad.r)      state->buttons |= TR_INPUT_LOOK;

    /* C-buttons for look mode */
    if (pad.c_up)    state->buttons |= TR_INPUT_LOOK_UP;
    if (pad.c_down)  state->buttons |= TR_INPUT_LOOK_DOWN;
    if (pad.c_left)  state->buttons |= TR_INPUT_WALK;
    if (pad.c_right) state->buttons |= TR_INPUT_LOOK_RIGHT;

    /* Roll = C-Up + C-Down simultaneously */
    if (pad.c_up && pad.c_down) state->buttons |= TR_INPUT_ROLL;

    /* D-pad → direct movement */
    if (pad.up)    state->buttons |= TR_INPUT_UP;
    if (pad.down)  state->buttons |= TR_INPUT_DOWN;
    if (pad.left)  state->buttons |= TR_INPUT_LEFT;
    if (pad.right) state->buttons |= TR_INPUT_RIGHT;

    /* L + Left/Right → sidestep */
    if (pad.l && pad.left)  state->buttons |= TR_INPUT_STEP_LEFT;
    if (pad.l && pad.right) state->buttons |= TR_INPUT_STEP_RIGHT;

    /* Analog stick → movement with deadzone */
    state->analog_x = pad.x;
    state->analog_y = pad.y;

    if (pad.x > DEADZONE)       state->buttons |= TR_INPUT_RIGHT;
    else if (pad.x < -DEADZONE) state->buttons |= TR_INPUT_LEFT;
    if (pad.y > DEADZONE)       state->buttons |= TR_INPUT_UP;
    else if (pad.y < -DEADZONE) state->buttons |= TR_INPUT_DOWN;

    /* Normalized analog (-1.0 to 1.0) with deadzone */
    int ax = (pad.x > DEADZONE) ? pad.x - DEADZONE :
             (pad.x < -DEADZONE) ? pad.x + DEADZONE : 0;
    int ay = (pad.y > DEADZONE) ? pad.y - DEADZONE :
             (pad.y < -DEADZONE) ? pad.y + DEADZONE : 0;
    state->move_x = (float)ax / (127.0f - DEADZONE);
    state->move_y = (float)ay / (127.0f - DEADZONE);
    if (state->move_x > 1.0f) state->move_x = 1.0f;
    if (state->move_x < -1.0f) state->move_x = -1.0f;
    if (state->move_y > 1.0f) state->move_y = 1.0f;
    if (state->move_y < -1.0f) state->move_y = -1.0f;
}

/* -------------------------------------------------------------------
 * Timing
 * ------------------------------------------------------------------- */

uint32_t n64_timer_ticks(void) {
    return c0_count();
}

uint32_t n64_timer_ms(void) {
    return c0_count() / N64_TICKS_PER_MS;
}

void n64_timer_wait_ms(uint32_t ms) {
    delay_ms(ms);
}

/* -------------------------------------------------------------------
 * Memory pool (bump allocator)
 * ------------------------------------------------------------------- */

static uint8_t game_heap[N64_GAME_HEAP_SIZE]
    __attribute__((aligned(16), section(".bss")));
static uint32_t heap_offset = 0;

void n64_mem_init(void) {
    heap_offset = 0;
}

void *n64_mem_alloc(uint32_t size) {
    /* Align to 16 bytes */
    size = (size + 15) & ~15;
    if (heap_offset + size > N64_GAME_HEAP_SIZE) {
        return (void *)0;  /* Out of memory */
    }
    void *ptr = &game_heap[heap_offset];
    heap_offset += size;
    return ptr;
}

void n64_mem_reset(void) {
    heap_offset = 0;
}

uint32_t n64_mem_available(void) {
    return N64_GAME_HEAP_SIZE - heap_offset;
}

/* -------------------------------------------------------------------
 * File I/O bridge → asset streaming
 * ------------------------------------------------------------------- */

/* Forward-declared: the asset manager lives in the main module */
extern void *trx_asset_load(uint32_t asset_id, uint32_t *out_size);

#define MAX_OPEN_FILES 4
static n64_file_t open_files[MAX_OPEN_FILES];

n64_file_t *n64_file_open(uint32_t asset_id) {
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (open_files[i].data == (void *)0) {
            uint32_t size = 0;
            const uint8_t *data = trx_asset_load(asset_id, &size);
            if (!data) return (void *)0;

            open_files[i].asset_id = asset_id;
            open_files[i].data = data;
            open_files[i].size = size;
            open_files[i].pos = 0;
            return &open_files[i];
        }
    }
    return (void *)0;  /* Too many open files */
}

int32_t n64_file_read(n64_file_t *f, void *buf, uint32_t size) {
    if (!f || !f->data) return -1;
    uint32_t remaining = f->size - f->pos;
    if (size > remaining) size = remaining;
    memcpy(buf, f->data + f->pos, size);
    f->pos += size;
    return (int32_t)size;
}

int32_t n64_file_seek(n64_file_t *f, int32_t offset, int whence) {
    if (!f) return -1;
    int32_t new_pos;
    switch (whence) {
        case 0: new_pos = offset; break;                    /* SEEK_SET */
        case 1: new_pos = (int32_t)f->pos + offset; break;  /* SEEK_CUR */
        case 2: new_pos = (int32_t)f->size + offset; break; /* SEEK_END */
        default: return -1;
    }
    if (new_pos < 0) new_pos = 0;
    if ((uint32_t)new_pos > f->size) new_pos = f->size;
    f->pos = (uint32_t)new_pos;
    return 0;
}

uint32_t n64_file_tell(n64_file_t *f) {
    return f ? f->pos : 0;
}

void n64_file_close(n64_file_t *f) {
    if (f) {
        memset(f, 0, sizeof(*f));
    }
}

/* -------------------------------------------------------------------
 * Audio stubs (milestone 2+)
 * ------------------------------------------------------------------- */

void n64_audio_init(void) {}
void n64_audio_play_sample(uint32_t sample_id, int16_t volume, int16_t pan) {
    (void)sample_id; (void)volume; (void)pan;
}
void n64_audio_play_music(uint32_t track_id) { (void)track_id; }
void n64_audio_stop_music(void) {}
void n64_audio_shutdown(void) {}
