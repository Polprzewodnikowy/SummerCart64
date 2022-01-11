#include <stdarg.h>
#include <stdio.h>
#include "exception.h"
#include "font.h"
#include "sys.h"


typedef struct {
    uint64_t gpr[32];
    uint32_t sr;
    uint32_t cr;
    uint64_t epc;
} exception_t;


#define EXCEPTION_INTERRUPT     (0)
#define EXCEPTION_SYSCALL       (8)

#define INTERRUPT_MASK_TIMER    (1 << 7)

#define SYSCALL_CODE_MASK       (0x03FF0000UL)
#define SYSCALL_CODE_BIT        (16)

#define SCREEN_WIDTH            (640)
#define SCREEN_HEIGHT           (240)

#define BACKGROUND_COLOR        (0x000000FFUL)
#define FOREGROUND_COLOR        (0xFFFFFFFFUL)

#define LINE_HEIGHT             (12)


static const vi_regs_t vi_config[] = {{
    .CR = VI_CR_TYPE_32,
    .H_WIDTH = SCREEN_WIDTH,
    .V_INTR = 512,
    .CURR_LINE = 0,
    .TIMING = 0x0404233A,
    .V_SYNC = 625,
    .H_SYNC = 0x00150C69,
    .H_SYNC_LEAP = 0x0C6F0C6E,
    .H_LIMITS = 0x00800300,
    .V_LIMITS = 0x005F0239,
    .COLOR_BURST = 0x00090268,
    .H_SCALE = ((0x100 * SCREEN_WIDTH) / 160),
    .V_SCALE = ((0x100 * SCREEN_HEIGHT) / 60),
}, {
    .CR = VI_CR_TYPE_32,
    .H_WIDTH = SCREEN_WIDTH,
    .V_INTR = 512,
    .CURR_LINE = 0,
    .TIMING = 0x03E52239,
    .V_SYNC = 525,
    .H_SYNC = 0x00000C15,
    .H_SYNC_LEAP = 0x0C150C15,
    .H_LIMITS = 0x006C02EC,
    .V_LIMITS = 0x002501FF,
    .COLOR_BURST = 0x000E0204,
    .H_SCALE = ((0x100 * SCREEN_WIDTH) / 160),
    .V_SCALE = ((0x100 * SCREEN_HEIGHT) / 60),
}};

static io32_t *exception_framebuffer = (io32_t *) (0x00200000UL);


static void exception_init_screen (void) {
    const vi_regs_t *cfg = &vi_config[OS_INFO->tv_type];

    for (int i = 0; i < (SCREEN_WIDTH * SCREEN_HEIGHT); i++) {
        io_write(&exception_framebuffer[i], BACKGROUND_COLOR);
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

static void exception_draw_character (int x, int y, char c) {
    if ((c < ' ') || (c > '~')) {
        c = 127;
    }

    for (int i = 0; i < (FONT_WIDTH * FONT_HEIGHT); i++) {
        int c_x = x + (i % FONT_WIDTH);
        int c_y = y + (i / FONT_WIDTH);

        if ((c_x >= SCREEN_WIDTH) || (c_y >= SCREEN_HEIGHT)) {
            break;
        }

        if (font_data[c - ' '][i / 8] & (1 << (i % 8))) {
            int screen_offset = c_x + (c_y * SCREEN_WIDTH);
            io_write(&exception_framebuffer[screen_offset], FOREGROUND_COLOR);
        }
    }
}

static void exception_print_string (int x, int y, const char *s) {
    while (*s != '\0') {
        exception_draw_character(x, y, *s++);
        x += FONT_WIDTH;
    }
}

static void exception_print (int *x, int *y, const char* fmt, ...) {
    char line[128];
    va_list args;

    va_start(args, fmt);

    vsniprintf(line, sizeof(line), fmt, args);
    exception_print_string(*x, *y, line);
    *y += LINE_HEIGHT;

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
    uint32_t sc64_version = pi_io_read(&SC64->VERSION);
    uint32_t *instruction_address = (uint32_t *) ((uint32_t) (e->epc));
    uint32_t gpr32[32];
    int x = 12;
    int y = 8;

    if (e->cr & C0_CR_BD) {
        instruction_address += 1;
    }

    for (int i = 0; i < 32; i++) {
        gpr32[i] = (uint32_t) (e->gpr[i]);
    }

    exception_init_screen();

    exception_print(&x, &y, "SC64 VERSION: 0x%08lX (%4s)", sc64_version, (char *) (&sc64_version));
    exception_print(&x, &y, "%s at pc: 0x%08lX", exception_get_description(exception_code), (uint32_t) (e->epc));
    exception_print(&x, &y, "sr: 0x%08lX, cr: 0x%08lX", e->sr, e->cr);
    exception_print(&x, &y, "zr: 0x%08lX, at: 0x%08lX, v0: 0x%08lX, v1: 0x%08lX", gpr32[0], gpr32[1], gpr32[2], gpr32[3]);
    exception_print(&x, &y, "a0: 0x%08lX, a1: 0x%08lX, a2: 0x%08lX, a3: 0x%08lX", gpr32[4], gpr32[5], gpr32[6], gpr32[7]);
    exception_print(&x, &y, "t0: 0x%08lX, t1: 0x%08lX, t2: 0x%08lX, t3: 0x%08lX", gpr32[8], gpr32[9], gpr32[10], gpr32[11]);
    exception_print(&x, &y, "t4: 0x%08lX, t5: 0x%08lX, t6: 0x%08lX, t7: 0x%08lX", gpr32[12], gpr32[13], gpr32[14], gpr32[15]);
    exception_print(&x, &y, "s0: 0x%08lX, s1: 0x%08lX, s2: 0x%08lX, s3: 0x%08lX", gpr32[16], gpr32[17], gpr32[18], gpr32[19]);
    exception_print(&x, &y, "s4: 0x%08lX, s5: 0x%08lX, s6: 0x%08lX, s7: 0x%08lX", gpr32[20], gpr32[21], gpr32[22], gpr32[23]);
    exception_print(&x, &y, "t8: 0x%08lX, t9: 0x%08lX, k0: 0x%08lX, k1: 0x%08lX", gpr32[24], gpr32[25], gpr32[26], gpr32[27]);
    exception_print(&x, &y, "gp: 0x%08lX, sp: 0x%08lX, fp: 0x%08lX, ra: 0x%08lX", gpr32[28], gpr32[29], gpr32[30], gpr32[31]);

    if (exception_code == EXCEPTION_INTERRUPT) {
        if (interrupt_mask & INTERRUPT_MASK_TIMER) {
            exception_print(&x, &y, "Bootloader did not finish within 10 seconds limit");
        }
    } else if (exception_code == EXCEPTION_SYSCALL) {
        uint32_t code = (((*instruction_address) & SYSCALL_CODE_MASK) >> SYSCALL_CODE_BIT);

        if (code == TRIGGER_CODE_ERROR) {
            const char *message = (const char *) (gpr32[4]);
            exception_print(&x, &y, "%s", message);
        } else if (code == TRIGGER_CODE_ASSERT) {
            const char *file = (const char *) (gpr32[4]);
            int line = (int) (gpr32[5]);
            const char *func = (const char *) (gpr32[6]);
            const char *failedexpr = (const char *) (gpr32[7]);

            exception_print(&x, &y, "assertion \"%s\" failed:", failedexpr);
            exception_print(&x, &y, "  file \"%s\", line %d, %s%s", file, line, func ? "function: " : "", func);
        }
    }

    while (1);
}
