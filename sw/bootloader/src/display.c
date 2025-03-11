#include <stdio.h>
#include "display.h"
#include "font.h"
#include "init.h"
#include "io.h"


#define SCREEN_WIDTH            (640)
#define SCREEN_HEIGHT           (240)
#define BORDER_WIDTH            (64)
#define BORDER_HEIGHT           (24)

#define BACKGROUND_COLOR        (0x00000000UL)
#define TEXT_COLOR              (0xFFFFFFFFUL)
#define LINE_SPACING            (2)

#define VI_CR                   (VI_CR_PIXEL_ADVANCE_1 | VI_CR_PIXEL_ADVANCE_0 | VI_CR_ANTIALIAS_1 | VI_CR_ANTIALIAS_0 | VI_CR_TYPE_32)


static io32_t display_framebuffer[SCREEN_WIDTH * SCREEN_HEIGHT] __attribute__((section(".framebuffer, \"aw\", %nobits#")));
static int char_x;
static int char_y;
static bool vi_configured = false;
static const vi_regs_t vi_config[] = {{
    .CR = VI_CR,
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
    .CR = VI_CR,
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
}, {
    .CR = VI_CR,
    .H_WIDTH = SCREEN_WIDTH,
    .V_INTR = 0x000003FF,
    .CURR_LINE = 0x00000000,
    .TIMING = 0x04651E39,
    .V_SYNC = 0x0000020D,
    .H_SYNC = 0x00040C11,
    .H_SYNC_LEAP = 0x0C190C1A,
    .H_LIMITS = 0x006C02EC,
    .V_LIMITS = 0x002501FF,
    .COLOR_BURST = 0x000E0204,
    .H_SCALE = 0x00000400,
    .V_SCALE = 0x00000400,
}};     


static void display_decompress_background (uint32_t *background) {
    uint32_t *framebuffer = (uint32_t *) (display_framebuffer);

    int pixel_count = (int) ((*background++) / 3);
    int pixels_painted = 0;

    while (pixels_painted < pixel_count) {
        uint32_t pixel = *background++;

        int pixel_repeat = (((pixel >> 24) & 0xFF) + 1);
        uint32_t pixel_value = (((pixel << 8) & 0xFFFFFF00) | 0xFF);

        for (int i = 0; i < pixel_repeat; i++) {
            cpu_io_write(framebuffer++, pixel_value);
        }

        pixels_painted += pixel_repeat;
    }
}

static void display_clear_background (void) {
    for (int i = 0; i < (SCREEN_WIDTH * SCREEN_HEIGHT); i++) {
        cpu_io_write(&display_framebuffer[i], BACKGROUND_COLOR);
    }
}

static void display_draw_character (char c) {
    if (c == '\n') {
        char_x = BORDER_WIDTH;
        char_y += FONT_HEIGHT + LINE_SPACING;
        return;
    }

    if (c == '\b') {
        char_x -= FONT_WIDTH;
        for (int i = 0; i < (FONT_WIDTH * FONT_HEIGHT); i++) {
            int c_x = char_x + (i % FONT_WIDTH);
            int c_y = char_y + (i / FONT_WIDTH);

            if ((c_x >= (SCREEN_WIDTH - BORDER_WIDTH)) || (c_y >= (SCREEN_HEIGHT - BORDER_HEIGHT))) {
                continue;
            }

            int screen_offset = c_x + (c_y * SCREEN_WIDTH);
            cpu_io_write(&display_framebuffer[screen_offset], BACKGROUND_COLOR);
        }
        return;
    }

    if ((char_x + FONT_WIDTH) > (SCREEN_WIDTH - BORDER_WIDTH)) {
        char_x -= FONT_WIDTH;
        c = '\x7F';
    }

    if ((c < ' ') || (c > '~')) {
        c = '\x7F';
    }

    for (int i = 0; i < (FONT_WIDTH * FONT_HEIGHT); i++) {
        int c_x = char_x + (i % FONT_WIDTH);
        int c_y = char_y + (i / FONT_WIDTH);

        if ((c_x >= (SCREEN_WIDTH - BORDER_WIDTH)) || (c_y >= (SCREEN_HEIGHT - BORDER_HEIGHT))) {
            continue;
        }

        if (font_data[c - ' '][i / 8] & (1 << (i % 8))) {
            int screen_offset = c_x + (c_y * SCREEN_WIDTH);
            cpu_io_write(&display_framebuffer[screen_offset], TEXT_COLOR);
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
    char_x = BORDER_WIDTH;
    char_y = BORDER_HEIGHT;

    if (background != NULL) {
        display_decompress_background(background);
    } else {
        display_clear_background();
    }

    if (!vi_configured) {
        vi_configured = true;

        const vi_regs_t *cfg = &vi_config[__tv_type];

        cpu_io_write(&VI->MADDR, (uint32_t) (display_framebuffer));
        cpu_io_write(&VI->H_WIDTH, cfg->H_WIDTH);
        cpu_io_write(&VI->V_INTR, cfg->V_INTR);
        cpu_io_write(&VI->CURR_LINE, cfg->CURR_LINE);
        cpu_io_write(&VI->TIMING, cfg->TIMING);
        cpu_io_write(&VI->V_SYNC, cfg->V_SYNC);
        cpu_io_write(&VI->H_SYNC, cfg->H_SYNC);
        cpu_io_write(&VI->H_SYNC_LEAP, cfg->H_SYNC_LEAP);
        cpu_io_write(&VI->H_LIMITS, cfg->H_LIMITS);
        cpu_io_write(&VI->V_LIMITS, cfg->V_LIMITS);
        cpu_io_write(&VI->COLOR_BURST, cfg->COLOR_BURST);
        cpu_io_write(&VI->H_SCALE, cfg->H_SCALE);
        cpu_io_write(&VI->V_SCALE, cfg->V_SCALE);
        cpu_io_write(&VI->CR, cfg->CR);
    }
}

bool display_ready (void) {
    return vi_configured;
}

void display_vprintf (const char *fmt, va_list args) {
    char line[1024];

    vsniprintf(line, sizeof(line), fmt, args);
    display_draw_string(line);
}

void display_printf (const char* fmt, ...) {
    va_list args;

    va_start(args, fmt);
    display_vprintf(fmt, args);
    va_end(args);
}
