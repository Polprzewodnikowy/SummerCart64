#pragma once

#include <trx/game/ui/common.h>
#include <trx/game/ui/text.h>

// Basic text widget.

typedef UI_TEXT_SETTINGS UI_LABEL_SETTINGS;

void UI_Label(const char *text);
void UI_LabelFmt(const char *fmt, ...);
void UI_LabelEx(const char *text, UI_LABEL_SETTINGS settings);

void UI_Label_Measure(const char *text, float *out_w, float *out_h);
float UI_Label_MeasureW(const char *text);
void UI_Label_MeasureEx(
    const char *text, float *out_w, float *out_h, UI_LABEL_SETTINGS settings);
