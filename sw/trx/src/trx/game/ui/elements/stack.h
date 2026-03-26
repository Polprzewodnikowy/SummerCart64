#pragma once

#include <trx/game/ui/common.h>

#include <stdint.h>

// Stack several widgets vertically or horizontally.

typedef enum {
    UI_STACK_VERTICAL,
    UI_STACK_HORIZONTAL,
} UI_STACK_ORIENTATION;

typedef enum {
    UI_STACK_H_ALIGN_LEFT,
    UI_STACK_H_ALIGN_CENTER,
    UI_STACK_H_ALIGN_RIGHT,
    UI_STACK_H_ALIGN_SPAN,
    UI_STACK_H_ALIGN_DISTRIBUTE,
} UI_STACK_H_ALIGN;

typedef enum {
    UI_STACK_V_ALIGN_TOP,
    UI_STACK_V_ALIGN_CENTER,
    UI_STACK_V_ALIGN_BOTTOM,
    UI_STACK_V_ALIGN_SPAN,
    UI_STACK_V_ALIGN_DISTRIBUTE,
} UI_STACK_V_ALIGN;

typedef struct {
    UI_STACK_ORIENTATION orientation;
    struct {
        UI_STACK_H_ALIGN h;
        UI_STACK_V_ALIGN v;
    } align;
    struct {
        float h;
        float v;
    } spacing;
} UI_STACK_SETTINGS;

void UI_BeginStack(UI_STACK_ORIENTATION orientation);
void UI_BeginStackEx(UI_STACK_SETTINGS settings);
void UI_EndStack(void);
