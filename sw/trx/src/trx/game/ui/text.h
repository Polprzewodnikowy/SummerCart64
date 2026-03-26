#pragma once

#include <stddef.h>
#include <stdint.h>

#define UI_TEXT_HEIGHT 15
#define UI_TEXT_BASE_SCALE 0x10000

typedef struct {
    float scale;
    int32_t z;
} UI_TEXT_SETTINGS;

// Initialize and shutdown UI text rendering cache.
void UI_InitText(void);
void UI_ShutdownText(void);

// Observe level load to establish glyph widths.
void UI_LoadText(void);

// Draw the given text at screen coordinates (x, y) with specified settings.
void UI_Text_Draw(
    const char *text, float x, float y, UI_TEXT_SETTINGS settings);

// Measure the width and height of the given text with specified settings.
void UI_Text_Measure(
    const char *text, float *out_w, float *out_h, UI_TEXT_SETTINGS settings);

// Wrap a text into multiple lines to fit a specific width in pixels.
char *UI_Text_WordWrap(
    const char *text, const float scale, const float max_width);

// Filter out any characters not present in the glyph map.
// Returns a newly-allocated string containing only known glyphs.
// Caller must free the result with Memory_Free*().
char *UI_Text_FilterGlyphs(const char *text);
