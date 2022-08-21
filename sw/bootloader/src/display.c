#include <stdio.h>
#include "display.h"
#include "font.h"
#include "io.h"


#define SCREEN_WIDTH            (640)
#define SCREEN_HEIGHT           (240)
#define BORDER_WIDTH            (32)
#define BORDER_HEIGHT           (16)

#define BACKGROUND_COLOR        (0x00000000UL)
#define TEXT_COLOR              (0xFFFFFFFFUL)
#define LINE_HEIGHT             (10)


static io32_t display_framebuffer[SCREEN_WIDTH * SCREEN_HEIGHT] __attribute__((aligned(64)));
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


static void display_draw_character (char c) {
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
            io_write(&display_framebuffer[screen_offset], TEXT_COLOR);
        }
    }

    char_x += FONT_WIDTH;
}

static void display_draw_string (const char *s) {
    while (*s != '\0') {
        display_draw_character(*s++);
    }
}


void display_init (uint32_t *background) {
    const vi_regs_t *cfg = &vi_config[OS_INFO->tv_type];

    char_x = BORDER_WIDTH;
    char_y = BORDER_HEIGHT;

    if (background == NULL) {
        for (int i = 0; i < (SCREEN_WIDTH * SCREEN_HEIGHT); i += 1) {
            io_write(&display_framebuffer[i], BACKGROUND_COLOR);
        }
    } else {
        for (int i = 0; i < (SCREEN_WIDTH * SCREEN_HEIGHT); i += 2) {
            io_write(&display_framebuffer[i], *background);
            io_write(&display_framebuffer[i + 1], *background);
            background++;
        }
    }

    io_write(&VI->MADDR, (uint32_t) (display_framebuffer));
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

void display_vprintf (const char *fmt, va_list args) {
    char line[256];

    vsniprintf(line, sizeof(line), fmt, args);
    display_draw_string(line);
}

void display_printf (const char* fmt, ...) {
    va_list args;

    va_start(args, fmt);
    display_vprintf(fmt, args);
    va_end(args);
}
