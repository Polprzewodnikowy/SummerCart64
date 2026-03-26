#include <trx/game/ui/elements/progress_button.h>

#include <trx/core/memory.h>
#include <trx/core/strings.h>
#include <trx/game/const.h>
#include <trx/game/ui/elements/hide.h>
#include <trx/game/ui/elements/label.h>
#include <trx/game/ui/elements/pad.h>
#include <trx/game/ui/elements/sleek_bar.h>
#include <trx/game/ui/elements/stack.h>

#define M_HOLD_TIMER_DEBUFF (LOGIC_FPS / 3)
#define M_HOLD_TIMER_MAX LOGIC_FPS

struct UI_PROGRESS_BUTTON_STATE {
    GAME_STRING_ID text;
    INPUT_BACKEND backend;
    INPUT_ROLE role;
    UI_PROGRESS_BUTTON_CALLBACK func;
    void *func_arg;
    int32_t hold_timer;
};

UI_PROGRESS_BUTTON_STATE *UI_ProgressButton_Init(
    INPUT_BACKEND backend, INPUT_ROLE role, GAME_STRING_ID text,
    UI_PROGRESS_BUTTON_CALLBACK func, void *func_arg)
{
    UI_PROGRESS_BUTTON_STATE *const s =
        Memory_Alloc(sizeof(UI_PROGRESS_BUTTON_STATE));
    s->backend = backend;
    s->role = role;
    s->text = text;
    s->func = func;
    s->func_arg = func_arg;
    s->hold_timer = 0;
    return s;
}

void UI_ProgressButton_Control(UI_PROGRESS_BUTTON_STATE *const s)
{
    if (!Input_IsPressedEx(s->backend, INPUT_LAYOUT_DEFAULT, s->role)) {
        s->hold_timer = 0;
        return;
    }
    if (s->hold_timer != -1) {
        s->hold_timer++;
        if (s->hold_timer - M_HOLD_TIMER_DEBUFF > M_HOLD_TIMER_MAX) {
            s->func(s->func_arg);
            s->hold_timer = -1; // Debounce the key
        }
    }
}

void UI_ProgressButton_Free(UI_PROGRESS_BUTTON_STATE *s)
{
    Memory_Free(s);
}

void UI_ProgressButton(UI_PROGRESS_BUTTON_STATE *const s)
{
    const char *const key_name =
        Input_GetKeyName(s->backend, INPUT_LAYOUT_DEFAULT, s->role);
    if (key_name == nullptr) {
        return;
    }
    const char *const value_label =
        String_FormatStatic(GS("general/misc/hold_fmt"), key_name);

    const float pad[2] = { 6.0f, 3.0f };
    const float spacing = 2.0f;
    const float progress =
        (s->hold_timer - M_HOLD_TIMER_DEBUFF) / (float)M_HOLD_TIMER_MAX;

    UI_BeginPad(pad[0], pad[1]);
    UI_BeginStackEx((UI_STACK_SETTINGS) {
        .orientation = UI_STACK_VERTICAL,
        .align = {
            .h = UI_STACK_H_ALIGN_SPAN,
            .v = UI_STACK_V_ALIGN_TOP,
        },
        .spacing = {
            .h = 0.0f,
            .v = spacing,
        },
    });
    UI_LabelFmt("%s: %s", GameString_Get(s->text), value_label);
    UI_BeginHide(progress < 0.0f);
    UI_SleekBar(progress);
    UI_EndHide();
    UI_EndStack();
    UI_EndPad();
}
