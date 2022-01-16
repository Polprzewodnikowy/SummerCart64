#include <stdarg.h>
#include <stdio.h>
#include "exception.h"
#include "font.h"
#include "io.h"
#include "sc64.h"
#include "version.h"
#include "vr4300.h"


typedef union {
    uint64_t u64;
    struct {
        uint32_t u32_h;
        uint32_t u32;
    };
} uint64_32_t;

typedef struct {
    uint64_32_t zr;
    uint64_32_t at;
    uint64_32_t v0;
    uint64_32_t v1;
    uint64_32_t a0;
    uint64_32_t a1;
    uint64_32_t a2;
    uint64_32_t a3;
    uint64_32_t t0;
    uint64_32_t t1;
    uint64_32_t t2;
    uint64_32_t t3;
    uint64_32_t t4;
    uint64_32_t t5;
    uint64_32_t t6;
    uint64_32_t t7;
    uint64_32_t s0;
    uint64_32_t s1;
    uint64_32_t s2;
    uint64_32_t s3;
    uint64_32_t s4;
    uint64_32_t s5;
    uint64_32_t s6;
    uint64_32_t s7;
    uint64_32_t t8;
    uint64_32_t t9;
    uint64_32_t k0;
    uint64_32_t k1;
    uint64_32_t gp;
    uint64_32_t sp;
    uint64_32_t fp;
    uint64_32_t ra;
    uint32_t sr;
    uint32_t cr;
    uint64_32_t epc;
} exception_t;


#define EXCEPTION_INTERRUPT     (0)
#define EXCEPTION_SYSCALL       (8)

#define INTERRUPT_MASK_TIMER    (1 << 7)

#define SYSCALL_CODE_MASK       (0x03FFFFC0UL)
#define SYSCALL_CODE_BIT        (6)

#define SCREEN_WIDTH            (640)
#define SCREEN_HEIGHT           (240)
#define BORDER_WIDTH            (32)
#define BORDER_HEIGHT           (16)

#define BACKGROUND_COLOR        (0xFFFFFFFFUL)
#define FOREGROUND_COLOR        (0x000000FFUL)
#define BORDER_COLOR            (0x2F2F2FFFUL)

#define LINE_HEIGHT             (10)
#define START_X_OFFSET          (19)


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

static io32_t *exception_framebuffer = (io32_t *) (0x0026A000UL);


static void exception_init_screen (void) {
    const vi_regs_t *cfg = &vi_config[OS_INFO->tv_type];

    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            uint32_t color;
            if (
                (x < (BORDER_WIDTH - FONT_WIDTH)) ||
                (x > (SCREEN_WIDTH - BORDER_WIDTH + FONT_WIDTH)) ||
                (y < (BORDER_HEIGHT - FONT_HEIGHT)) ||
                (y > (SCREEN_HEIGHT - BORDER_HEIGHT + FONT_HEIGHT))
            ) {
                color = BORDER_COLOR;
            } else {
                color = BACKGROUND_COLOR;
            }
            io_write(&exception_framebuffer[x + (y * SCREEN_WIDTH)], color);
        }
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

        if ((c_x >= (SCREEN_WIDTH - BORDER_WIDTH)) || (c_y >= (SCREEN_HEIGHT - BORDER_HEIGHT))) {
            break;
        }

        if (font_data[c - ' '][i / 8] & (1 << (i % 8))) {
            int screen_offset = c_x + (c_y * SCREEN_WIDTH);
            io_write(&exception_framebuffer[screen_offset], FOREGROUND_COLOR);
        }
    }
}

static void exception_print_string (const char *s) {
    static int x = BORDER_WIDTH + (START_X_OFFSET * FONT_WIDTH);
    static int y = BORDER_HEIGHT;

    while (*s != '\0') {
        if (*s == '\n') {
            x = BORDER_WIDTH;
            y += LINE_HEIGHT;
            s++;
        } else {
            if (x + FONT_WIDTH > (SCREEN_WIDTH - BORDER_WIDTH)) {
                x = BORDER_WIDTH;
                y += LINE_HEIGHT;
            }
            exception_draw_character(x, y, *s++);
            x += FONT_WIDTH;
        }
    }
}

static void exception_vprint (const char *fmt, va_list args) {
    char line[256];

    vsniprintf(line, sizeof(line), fmt, args);
    exception_print_string(line);
    sc64_uart_print_string(line);
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
    uint32_t sc64_version = pi_io_read(&SC64->VERSION);
    uint32_t *instruction_address = (((uint32_t *) (e->epc.u32)) + ((e->cr & C0_CR_BD) ? 1 : 0));

    exception_init_screen();

    exception_print("-----  SummerCart64 n64boot  -----\n");
    exception_print("branch: %s | tag: %s\n", version->git_branch, version->git_tag);
    exception_print("sha: %s\n", version->git_sha);
    exception_print("msg: %s\n\n", version->git_message);
    exception_print("%s at pc: 0x%08lX\n", exception_get_description(exception_code), e->epc.u32);
    exception_print("sr: 0x%08lX  cr: 0x%08lX  hw: 0x%08lX  [%4s]\n", e->sr, e->cr, sc64_version, (char *) (&sc64_version));
    exception_print("zr: 0x%08lX  at: 0x%08lX  v0: 0x%08lX  v1: 0x%08lX\n", e->zr.u32, e->at.u32, e->v0.u32, e->v1.u32);
    exception_print("a0: 0x%08lX  a1: 0x%08lX  a2: 0x%08lX  a3: 0x%08lX\n", e->a0.u32, e->a1.u32, e->a2.u32, e->a3.u32);
    exception_print("t0: 0x%08lX  t1: 0x%08lX  t2: 0x%08lX  t3: 0x%08lX\n", e->t0.u32, e->t1.u32, e->t2.u32, e->t3.u32);
    exception_print("t4: 0x%08lX  t5: 0x%08lX  t6: 0x%08lX  t7: 0x%08lX\n", e->t4.u32, e->t5.u32, e->t6.u32, e->t7.u32);
    exception_print("s0: 0x%08lX  s1: 0x%08lX  s2: 0x%08lX  s3: 0x%08lX\n", e->s0.u32, e->s1.u32, e->s2.u32, e->s3.u32);
    exception_print("s4: 0x%08lX  s5: 0x%08lX  s6: 0x%08lX  s7: 0x%08lX\n", e->s4.u32, e->s5.u32, e->s6.u32, e->s7.u32);
    exception_print("t8: 0x%08lX  t9: 0x%08lX  k0: 0x%08lX  k1: 0x%08lX\n", e->t8.u32, e->t9.u32, e->k0.u32, e->k1.u32);
    exception_print("gp: 0x%08lX  sp: 0x%08lX  fp: 0x%08lX  ra: 0x%08lX\n\n", e->gp.u32, e->sp.u32, e->fp.u32, e->ra.u32);

    if (exception_code == EXCEPTION_INTERRUPT) {
        if (interrupt_mask & INTERRUPT_MASK_TIMER) {
            exception_print("Bootloader did not finish within 1 second limit\n\n");
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
