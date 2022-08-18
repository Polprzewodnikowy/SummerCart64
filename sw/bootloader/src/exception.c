#include <stdarg.h>
#include <stdio.h>
#include "exception_regs.h"
#include "exception.h"
#include "font.h"
#include "io.h"
#include "sc64.h"
#include "version.h"
#include "vr4300.h"
#include "../assets/assets.h"


#define EXCEPTION_INTERRUPT     (0)
#define EXCEPTION_SYSCALL       (8)

#define INTERRUPT_MASK_TIMER    (1 << 7)

#define SYSCALL_CODE_MASK       (0x03FFFFC0UL)
#define SYSCALL_CODE_BIT        (6)

#define SCREEN_WIDTH            (640)
#define SCREEN_HEIGHT           (240)
#define BORDER_WIDTH            (32)
#define BORDER_HEIGHT           (16)

#define TEXT_COLOR              (0xFFFFFFFFUL)
#define LINE_HEIGHT             (10)


static io32_t exception_framebuffer[SCREEN_WIDTH * SCREEN_HEIGHT] __attribute__((aligned(64)));
static int char_x;
static int char_y;
static const vi_regs_t vi_config[] = {{
    .CR = (
        VI_CR_PIXEL_ADVANCE_1 |
        VI_CR_PIXEL_ADVANCE_0 |
        VI_CR_ANTIALIAS_1 |
        VI_CR_ANTIALIAS_0 |
        VI_CR_TYPE_32
    ),
    .H_WIDTH = SCREEN_WIDTH,
    .V_INTR = 0x000003FF,
    .CURR_LINE = 0x00000000,
    .TIMING = 0x0404233A,
    .V_SYNC = 0x00000271,
    .H_SYNC = 0x00150C69,
    .H_SYNC_LEAP = 0x0C6F0C6E,
    .H_LIMITS = 0x00800300,
    .V_LIMITS = 0x005D023D,
    .COLOR_BURST = 0x00090268,
    .H_SCALE = 0x00000400,
    .V_SCALE = 0x00000400,
}, {
    .CR = (
        VI_CR_PIXEL_ADVANCE_1 |
        VI_CR_PIXEL_ADVANCE_0 |
        VI_CR_ANTIALIAS_1 |
        VI_CR_ANTIALIAS_0 |
        VI_CR_TYPE_32
    ),
    .H_WIDTH = SCREEN_WIDTH,
    .V_INTR = 0x000003FF,
    .CURR_LINE = 0x00000000,
    .TIMING = 0x03E52239,
    .V_SYNC = 0x0000020D,
    .H_SYNC = 0x00000C15,
    .H_SYNC_LEAP = 0x0C150C15,
    .H_LIMITS = 0x006C02EC,
    .V_LIMITS = 0x00230203,
    .COLOR_BURST = 0x000E0204,
    .H_SCALE = 0x00000400,
    .V_SCALE = 0x00000400,
}};


static void exception_init_screen (void) {
    const vi_regs_t *cfg = &vi_config[OS_INFO->tv_type];
    uint32_t *background_data = (uint32_t *) (&assets_exception_background);

    char_x = BORDER_WIDTH;
    char_y = BORDER_HEIGHT;

    for (int i = 0; i < (SCREEN_WIDTH * SCREEN_HEIGHT); i += 2) {
        io_write(&exception_framebuffer[i], *background_data);
        io_write(&exception_framebuffer[i + 1], *background_data);
        background_data++;
    }

    io_write(&VI->MADDR, (uint32_t) (exception_framebuffer));
    io_write(&VI->H_WIDTH, cfg->H_WIDTH);
    io_write(&VI->V_INTR, cfg->V_INTR);
    io_write(&VI->CURR_LINE, cfg->CURR_LINE);
    io_write(&VI->TIMING, cfg->TIMING);
    io_write(&VI->V_SYNC, cfg->V_SYNC);
    io_write(&VI->H_SYNC, cfg->H_SYNC);
    io_write(&VI->H_SYNC_LEAP, cfg->H_SYNC_LEAP);
    io_write(&VI->H_LIMITS, cfg->H_LIMITS);
    io_write(&VI->V_LIMITS, cfg->V_LIMITS);
    io_write(&VI->COLOR_BURST, cfg->COLOR_BURST);
    io_write(&VI->H_SCALE, cfg->H_SCALE);
    io_write(&VI->V_SCALE, cfg->V_SCALE);
    io_write(&VI->CR, cfg->CR);
}

static void exception_draw_character (char c) {
    if (c == '\n') {
        char_x = BORDER_WIDTH;
        char_y += LINE_HEIGHT;
        return;
    }

    if ((char_x + FONT_WIDTH) > (SCREEN_WIDTH - BORDER_WIDTH)) {
        char_x = BORDER_WIDTH;
        char_y += LINE_HEIGHT;
    }

    if ((c < ' ') || (c > '~')) {
        c = '\x7F';
    }

    for (int i = 0; i < (FONT_WIDTH * FONT_HEIGHT); i++) {
        int c_x = char_x + (i % FONT_WIDTH);
        int c_y = char_y + (i / FONT_WIDTH);

        if ((c_x >= (SCREEN_WIDTH - BORDER_WIDTH)) || (c_y >= (SCREEN_HEIGHT - BORDER_HEIGHT))) {
            break;
        }

        if (font_data[c - ' '][i / 8] & (1 << (i % 8))) {
            int screen_offset = c_x + (c_y * SCREEN_WIDTH);
            io_write(&exception_framebuffer[screen_offset], TEXT_COLOR);
        }
    }

    char_x += FONT_WIDTH;
}

static void exception_print_string (const char *s) {
    while (*s != '\0') {
        exception_draw_character(*s++);
    }
}

static void exception_vprint (const char *fmt, va_list args) {
    char line[256];

    vsniprintf(line, sizeof(line), fmt, args);
    exception_print_string(line);
}

static void exception_print (const char* fmt, ...) {
    va_list args;

    va_start(args, fmt);
    exception_vprint(fmt, args);
    va_end(args);
}

static const char *exception_get_description (uint8_t exception_code) {
    switch (exception_code) {
        case 0:  return "Interrupt";
        case 1:  return "TLB Modification exception";
        case 2:  return "TLB Miss exception (load or instruction fetch)";
        case 3:  return "TLB Miss exception (store)";
        case 4:  return "Address Error exception (load or instruction fetch)";
        case 5:  return "Address Error exception (store)";
        case 6:  return "Bus Error exception (instruction fetch)";
        case 7:  return "Bus Error exception (data reference: load or store)";
        case 8:  return "Syscall exception";
        case 9:  return "Breakpoint exception";
        case 10: return "Reserved Instruction exception";
        case 11: return "Coprocessor Unusable exception";
        case 12: return "Arithmetic Overflow exception";
        case 13: return "Trap exception";
        case 15: return "Floating-Point exception";
        case 23: return "Watch exception";
    }

    return "Unknown exception";
}


void exception_fatal_handler (uint32_t exception_code, uint32_t interrupt_mask, exception_t *e) {
    version_t *version = version_get();
    uint32_t sc64_version = pi_io_read(&SC64_REGS->VERSION);
    uint32_t *instruction_address = (((uint32_t *) (e->epc.u32)) + ((e->cr & C0_CR_BD) ? 1 : 0));

    exception_init_screen();

    exception_print("branch: %s\n", version->git_branch);
    exception_print("tag: %s\n", version->git_tag);
    exception_print("sha: %s\n\n", version->git_sha);
    exception_print("%s\n\n", exception_get_description(exception_code));
    exception_print("pc: 0x%08lX  sr: 0x%08lX  cr: 0x%08lX  hw: 0x%08lX  [%.4s]\n", e->epc.u32, e->sr, e->cr, sc64_version, (char *) (&sc64_version));
    exception_print("zr: 0x%08lX  at: 0x%08lX  v0: 0x%08lX  v1: 0x%08lX\n", e->zr.u32, e->at.u32, e->v0.u32, e->v1.u32);
    exception_print("a0: 0x%08lX  a1: 0x%08lX  a2: 0x%08lX  a3: 0x%08lX\n", e->a0.u32, e->a1.u32, e->a2.u32, e->a3.u32);
    exception_print("t0: 0x%08lX  t1: 0x%08lX  t2: 0x%08lX  t3: 0x%08lX\n", e->t0.u32, e->t1.u32, e->t2.u32, e->t3.u32);
    exception_print("t4: 0x%08lX  t5: 0x%08lX  t6: 0x%08lX  t7: 0x%08lX\n", e->t4.u32, e->t5.u32, e->t6.u32, e->t7.u32);
    exception_print("s0: 0x%08lX  s1: 0x%08lX  s2: 0x%08lX  s3: 0x%08lX\n", e->s0.u32, e->s1.u32, e->s2.u32, e->s3.u32);
    exception_print("s4: 0x%08lX  s5: 0x%08lX  s6: 0x%08lX  s7: 0x%08lX\n", e->s4.u32, e->s5.u32, e->s6.u32, e->s7.u32);
    exception_print("t8: 0x%08lX  t9: 0x%08lX  k0: 0x%08lX  k1: 0x%08lX\n", e->t8.u32, e->t9.u32, e->k0.u32, e->k1.u32);
    exception_print("gp: 0x%08lX  sp: 0x%08lX  s8: 0x%08lX  ra: 0x%08lX\n\n", e->gp.u32, e->sp.u32, e->s8.u32, e->ra.u32);

    if (exception_code == EXCEPTION_INTERRUPT) {
        if (interrupt_mask & INTERRUPT_MASK_TIMER) {
            exception_disable_watchdog();
            exception_print("Still loading after 3 second limit...\n\n");
            return;
        }
    } else if (exception_code == EXCEPTION_SYSCALL) {
        uint32_t code = (((*instruction_address) & SYSCALL_CODE_MASK) >> SYSCALL_CODE_BIT);

        if (code == TRIGGER_CODE_ERROR) {
            const char *fmt = (const char *) (e->a0.u32);
            va_list args = *((va_list *) (e->sp.u32));
            exception_vprint(fmt, args);
            exception_print("\n");
        }
    }

    while (1);
}
