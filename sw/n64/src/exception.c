#include <stdarg.h>
#include <stdio.h>
#include "boot.h"
#include "exception.h"
#include "sc64.h"
#include "sys.h"


extern io32_t __ipl3_font __attribute__((section(".data")));


#define SCREEN_WIDTH        (640)
#define SCREEN_HEIGHT       (240)

#define BACKGROUND_COLOR    (0xFFFFFFFFUL)
#define FOREGROUND_COLOR    (0x000000FFUL)

#define CHARACTER_WIDTH     (13)
#define CHARACTER_HEIGHT    (14)
#define CHARACTER_SIZE      (23)

#define LINE_HEIGHT         (22)


static const vi_regs_t vi_config[] = {{
    .CR = VI_CR_TYPE_32,
    .MADDR = 0x00200000UL,
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
    .MADDR = 0x00200000UL,
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

static const uint8_t exception_font_mapping[96] = {
    48, 36, 37, 38, 48, 48, 48, 39, 48, 48, 40, 41, 42, 43, 44, 45,
    26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 46, 48, 48, 47, 48, 48,
    49,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 48, 48, 48, 48, 48,
    48,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 48, 48, 48, 48, 48,
};

static io32_t *exception_framebuffer;
static uint32_t exception_font_buffer[288];


static void exception_init_screen (void) {
    const vi_regs_t *cfg = &vi_config[OS_INFO->tv_type];

    io32_t *src = &__ipl3_font;
    uint32_t *dst = exception_font_buffer;

    bool sdram_switched = sc64_get_config(CFG_ID_SDRAM_SWITCH);

    if (sdram_switched) {
        sc64_set_config(CFG_ID_SDRAM_SWITCH, false);
    }

    for (int i = 0; i < sizeof(exception_font_buffer); i += 4) {
        *dst++ = pi_io_read(src++);
    }

    if (sdram_switched) {
        sc64_set_config(CFG_ID_SDRAM_SWITCH, true);
    }

    exception_framebuffer = (io32_t *) (cfg->MADDR);
    
    for (int i = 0; i < (SCREEN_WIDTH * SCREEN_HEIGHT); i++) {
        io_write(exception_framebuffer + i, BACKGROUND_COLOR);
    }

    io_write(&VI->MADDR, cfg->MADDR);
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
    if ((c <= 32) || (c >= 127)) {
        return;
    }

    uint8_t index = exception_font_mapping[c - 32];
    uint8_t *character = ((uint8_t *) (exception_font_buffer)) + (index * CHARACTER_SIZE);

    for (int i = 0; i < (CHARACTER_WIDTH * CHARACTER_HEIGHT); i++) {
        int c_x = x + (i % CHARACTER_WIDTH);
        int c_y = y + (i / CHARACTER_WIDTH);

        if ((c_x >= SCREEN_WIDTH) || (c_y >= SCREEN_HEIGHT)) {
            break;
        }

        if (character[i / 8] & (1 << (7 - (i % 8)))) {
            int screen_offset = c_x + (c_y * SCREEN_WIDTH);
            io_write(&exception_framebuffer[screen_offset], FOREGROUND_COLOR);
        }
    }
}

static void exception_print_string (int x, int y, const char *s) {
    while (*s != '\0') {
        exception_draw_character(x, y, *s++);
        x += CHARACTER_WIDTH;
    }
}

static void exception_print (int *x, int *y, const char* fmt, ...) {
    char line[64];
    va_list args;

    va_start(args, fmt);

    vsniprintf(line, sizeof(line), fmt, args);
    exception_print_string(*x, *y, line);
    *y += LINE_HEIGHT;

    va_end(args);
}

static const char *exception_get_description (uint8_t exception_code) {
    switch (exception_code) {
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


typedef struct {
    uint64_t gpr[32];
    uint32_t sr;
    uint32_t cr;
    uint64_t epc;
} exception_t;


void exception_fatal_handler (exception_t *e) {
    uint8_t exception_code = (uint8_t) ((e->cr & EXCEPTION_CODE_MASK) >> EXCEPTION_CODE_BIT);
    const char *exception_description = exception_get_description(exception_code);

    exception_init_screen();

    uint32_t gpr32[32];

    for (int i = 0; i < 32; i++) {
        gpr32[i] = (uint32_t) (e->gpr[i]);
    }

    int x = 12;
    int y = 8;

    exception_print(&x, &y, "%s", exception_description);
    exception_print(&x, &y, "pc %08lX sr %08lX cr %08lX", (uint32_t) (e->epc), e->sr, e->cr);
    exception_print(&x, &y, "zr %08lX at %08lX v0 %08lX v1 %08lX", gpr32[0], gpr32[1], gpr32[2], gpr32[3]);
    exception_print(&x, &y, "a0 %08lX a1 %08lX a2 %08lX a3 %08lX", gpr32[4], gpr32[5], gpr32[6], gpr32[7]);
    exception_print(&x, &y, "t0 %08lX t1 %08lX t2 %08lX t3 %08lX", gpr32[8], gpr32[9], gpr32[10], gpr32[11]);
    exception_print(&x, &y, "t4 %08lX t5 %08lX t6 %08lX t7 %08lX", gpr32[12], gpr32[13], gpr32[14], gpr32[15]);
    exception_print(&x, &y, "s0 %08lX s1 %08lX s2 %08lX s3 %08lX", gpr32[16], gpr32[17], gpr32[18], gpr32[19]);
    exception_print(&x, &y, "s4 %08lX s5 %08lX s6 %08lX s7 %08lX", gpr32[20], gpr32[21], gpr32[22], gpr32[23]);
    exception_print(&x, &y, "t8 %08lX t9 %08lX k0 %08lX k1 %08lX", gpr32[24], gpr32[25], gpr32[26], gpr32[27]);
    exception_print(&x, &y, "gp %08lX sp %08lX fp %08lX ra %08lX", gpr32[28], gpr32[29], gpr32[30], gpr32[31]);

    LOG_E("%s\r\n", exception_description);
    LOG_E("pc: 0x%08lX, sr: 0x%08lX, cr: 0x%08lX\r\n", (uint32_t) (e->epc), e->sr, e->cr);
    LOG_E("zr: 0x%08lX, at: 0x%08lX, v0: 0x%08lX, v1: 0x%08lX\r\n", gpr32[0], gpr32[1], gpr32[2], gpr32[3]);
    LOG_E("a0: 0x%08lX, a1: 0x%08lX, a2: 0x%08lX, a3: 0x%08lX\r\n", gpr32[4], gpr32[5], gpr32[6], gpr32[7]);
    LOG_E("t0: 0x%08lX, t1: 0x%08lX, t2: 0x%08lX, t3: 0x%08lX\r\n", gpr32[8], gpr32[9], gpr32[10], gpr32[11]);
    LOG_E("t4: 0x%08lX, t5: 0x%08lX, t6: 0x%08lX, t7: 0x%08lX\r\n", gpr32[12], gpr32[13], gpr32[14], gpr32[15]);
    LOG_E("s0: 0x%08lX, s1: 0x%08lX, s2: 0x%08lX, s3: 0x%08lX\r\n", gpr32[16], gpr32[17], gpr32[18], gpr32[19]);
    LOG_E("s4: 0x%08lX, s5: 0x%08lX, s6: 0x%08lX, s7: 0x%08lX\r\n", gpr32[20], gpr32[21], gpr32[22], gpr32[23]);
    LOG_E("t8: 0x%08lX, t9: 0x%08lX, k0: 0x%08lX, k1: 0x%08lX\r\n", gpr32[24], gpr32[25], gpr32[26], gpr32[27]);
    LOG_E("gp: 0x%08lX, sp: 0x%08lX, fp: 0x%08lX, ra: 0x%08lX\r\n", gpr32[28], gpr32[29], gpr32[30], gpr32[31]);
    LOG_FLUSH();

    while (1);
}


void exception_interrupt_handler (uint32_t interrupt) {
    LOG_I("Unimplemented interrupt, mask: 0x%08lX\r\n", interrupt);
    LOG_FLUSH();

    while (1);
}
