#include <trx/game/ui/dialogs/color_editor.h>

#include <trx/config.h>
#include <trx/core/memory.h>
#include <trx/core/utils.h>
#include <trx/debug.h>
#include <trx/game/input.h>
#include <trx/game/ui.h>
#include <trx/game/ui/elements/color_swatch.h>
#include <trx/game/ui/elements/gradient_slider.h>
#include <trx/game/ui/helpers.h>
#include <trx/version.h>

#define M_COLOR_EDITOR_PADDING 8.0f
#define M_COLOR_EDITOR_TITLE_MARGIN 5.0f
#define M_MAX_STOPS 7
#define M_OKLCH_MAX_CHROMA 0.4f

typedef enum {
    M_COLOR_ROW_HUE,
    M_COLOR_ROW_CHROMA,
    M_COLOR_ROW_LIGHTNESS,
    M_COLOR_ROW_COUNT,
} M_COLOR_ROW;

typedef struct {
    float h;
    float c;
    float l;
    bool use_state_h;
    bool use_state_c;
    bool use_state_l;
} M_STOP_DEF;

typedef struct {
    GAME_STRING_ID label_id;
    int32_t stop_count;
    M_STOP_DEF stops[M_MAX_STOPS];
} M_ROW_DEF;

struct UI_COLOR_EDITOR_DIALOG_STATE {
    bool show;
    const UI_SETTINGS_OPTION *option;
    M_COLOR_ROW component_idx;
    float h;
    float c;
    float l;
    RGB_888 color;
    RGB_888 cached_stops[M_COLOR_ROW_COUNT][M_MAX_STOPS];
};

static M_ROW_DEF m_RowDefs[M_COLOR_ROW_COUNT];

__attribute__((constructor)) static void M_Init(void)
{
    m_RowDefs[M_COLOR_ROW_HUE] = (M_ROW_DEF) {
        .label_id = GS_ID("general/settings/common/hue"),
        .stop_count = 7,
        .stops = {
            { .h = 0.0f, .use_state_c = true, .use_state_l = true },
            { .h = 60.0f, .use_state_c = true, .use_state_l = true },
            { .h = 120.0f, .use_state_c = true, .use_state_l = true },
            { .h = 180.0f, .use_state_c = true, .use_state_l = true },
            { .h = 240.0f, .use_state_c = true, .use_state_l = true },
            { .h = 300.0f, .use_state_c = true, .use_state_l = true },
            { .h = 360.0f, .use_state_c = true, .use_state_l = true },
        },
    };

    m_RowDefs[M_COLOR_ROW_CHROMA] = (M_ROW_DEF) {
        .label_id = GS_ID("general/settings/common/chroma"),
        .stop_count = 2,
        .stops = {
            { .c = 0.0f, .use_state_h = true, .use_state_l = true },
            { .c = M_OKLCH_MAX_CHROMA, .use_state_h = true, .use_state_l = true },
        },
    };

    m_RowDefs[M_COLOR_ROW_LIGHTNESS] = (M_ROW_DEF) {
        .label_id = GS_ID("general/settings/common/lightness"),
        .stop_count = 3,
        .stops = {
            { .l = 0.0f, .use_state_h = true, .use_state_c = true },
            { .l = 0.5f, .use_state_h = true, .use_state_c = true },
            { .l = 1.0f, .use_state_h = true, .use_state_c = true },
        },
    };
}

static float M_GetSliderValue(
    const UI_COLOR_EDITOR_DIALOG_STATE *const s, const M_COLOR_ROW row)
{
    switch (row) {
    case M_COLOR_ROW_HUE:
        return s->h / 360.0f;
    case M_COLOR_ROW_CHROMA:
        return s->c / M_OKLCH_MAX_CHROMA;
    case M_COLOR_ROW_LIGHTNESS:
        return s->l;
    case M_COLOR_ROW_COUNT:
        break;
    }

    return 0.0f;
}

static RGB_888 M_GetStopColor(
    const UI_COLOR_EDITOR_DIALOG_STATE *const s, const M_STOP_DEF *const stop)
{
    const float h = stop->use_state_h ? s->h : stop->h;
    const float c = stop->use_state_c ? s->c : stop->c;
    const float l = stop->use_state_l ? s->l : stop->l;
    return Color_OKLCHToRGB(l, c, h);
}

static void M_GetGradientStops(
    const UI_COLOR_EDITOR_DIALOG_STATE *const s, const M_COLOR_ROW row,
    RGB_888 out_stops[M_MAX_STOPS])
{
    const M_ROW_DEF *const row_def = &m_RowDefs[row];
    for (int32_t i = 0; i < row_def->stop_count; i++) {
        out_stops[i] = M_GetStopColor(s, &row_def->stops[i]);
    }
}

static void M_RebuildCache(UI_COLOR_EDITOR_DIALOG_STATE *const s)
{
    s->color = Color_OKLCHToRGB(s->l, s->c, s->h);
    for (M_COLOR_ROW row = M_COLOR_ROW_HUE; row < M_COLOR_ROW_COUNT; row++) {
        M_GetGradientStops(s, row, s->cached_stops[row]);
    }
}

static void M_SetLocalColorFromRGB(
    UI_COLOR_EDITOR_DIALOG_STATE *const s, const RGB_888 rgb)
{
    Color_RGBToOKLCH(rgb, &s->l, &s->c, &s->h);
    CLAMP(s->c, 0.0f, M_OKLCH_MAX_CHROMA);
    M_RebuildCache(s);
}

static void M_EmitLocalColorAsRGB(UI_COLOR_EDITOR_DIALOG_STATE *const s)
{
    M_RebuildCache(s);
    *(RGB_888 *)s->option->target = s->color;
    Config_Update();
}

static void M_ColorEditorRow(
    UI_COLOR_EDITOR_DIALOG_STATE *const s, const M_COLOR_ROW row)
{
    const bool is_selected = s->component_idx == row;
    const M_ROW_DEF *const row_def = &m_RowDefs[row];

    if (is_selected) {
        UI_BeginFrame(UI_FRAME_SELECTED_OPTION);
    }

    UI_BeginPad(g_TRVersion == 1 ? 1.0f : 0.0f, g_TRVersion == 1 ? 1.0f : 0.0f);
    UI_BeginStackEx((UI_STACK_SETTINGS) {
        .orientation = UI_STACK_VERTICAL,
        .align = { .h = UI_STACK_H_ALIGN_SPAN },
        .spacing = { .v = 1.0f },
    });

    UI_BeginStackEx((UI_STACK_SETTINGS) {
        .orientation = UI_STACK_HORIZONTAL,
        .align = { .h = UI_STACK_H_ALIGN_DISTRIBUTE },
    });
    UI_Label(GameString_Get(row_def->label_id));
    UI_BeginRowArrows(is_selected, is_selected, UI_ROW_ARROWS_MEDIUM);
    UI_GradientSlider((UI_GRADIENT_SLIDER_SETTINGS) {
        .width = 100.0f,
        .value = M_GetSliderValue(s, row),
        .stop_count = row_def->stop_count,
        .stops = s->cached_stops[row],
    });
    UI_EndRowArrows();
    UI_EndStack();

    UI_EndStack();
    UI_EndPad();

    if (is_selected) {
        UI_EndFrame();
    }
}

UI_COLOR_EDITOR_DIALOG_STATE *UI_ColorEditorDialog_Init(void)
{
    UI_COLOR_EDITOR_DIALOG_STATE *const s = Memory_Alloc(sizeof(*s));
    return s;
}

void UI_ColorEditorDialog_Free(UI_COLOR_EDITOR_DIALOG_STATE *const s)
{
    Memory_Free(s);
}

void UI_ColorEditorDialog_Open(
    UI_COLOR_EDITOR_DIALOG_STATE *const s,
    const UI_SETTINGS_OPTION *const option)
{
    ASSERT(s != nullptr);
    ASSERT(option != nullptr);
    ASSERT(Config_GetOption(option->target)->type == COT_RGB888);
    const RGB_888 *const color = option->target;
    s->show = true;
    s->option = option;
    s->component_idx = 0;
    M_SetLocalColorFromRGB(s, *color);
}

void UI_ColorEditorDialog_Close(UI_COLOR_EDITOR_DIALOG_STATE *const s)
{
    if (s == nullptr) {
        return;
    }
    s->show = false;
    s->option = nullptr;
    s->component_idx = 0;
    s->h = 0.0f;
    s->c = 0.0f;
    s->l = 0.0f;
}

bool UI_ColorEditorDialog_IsOpen(const UI_COLOR_EDITOR_DIALOG_STATE *const s)
{
    return s != nullptr && s->show;
}

void UI_ColorEditorDialog_Control(UI_COLOR_EDITOR_DIALOG_STATE *const s)
{
    if (s == nullptr || !s->show) {
        return;
    }
    const UI_SETTINGS_OPTION *const option = s->option;
    if (option == nullptr) {
        UI_ColorEditorDialog_Close(s);
        return;
    }
    if (g_InputDB.menu_back || g_InputDB.look) {
        UI_ColorEditorDialog_Close(s);
        return;
    }
    if (g_InputDB.menu_up) {
        int32_t next_idx = (int32_t)s->component_idx - 1;
        if (next_idx < 0) {
            next_idx = M_COLOR_ROW_COUNT - 1;
        }
        s->component_idx = (M_COLOR_ROW)next_idx;
    } else if (g_InputDB.menu_down) {
        int32_t next_idx = (int32_t)s->component_idx + 1;
        if (next_idx >= M_COLOR_ROW_COUNT) {
            next_idx = 0;
        }
        s->component_idx = (M_COLOR_ROW)next_idx;
    } else if (g_InputDB.menu_left || g_InputDB.menu_right) {
        int32_t delta = g_Input.slow ? option->delta_slow : option->delta_fast;
        if (delta == 0) {
            delta = 1;
        }
        if (g_InputDB.menu_left) {
            delta = -delta;
        }
        if (s->component_idx == M_COLOR_ROW_HUE) {
            s->h += delta;
            while (s->h < 0.0f) {
                s->h += 360.0f;
            }
            while (s->h > 360.0f) {
                s->h -= 360.0f;
            }
        } else if (s->component_idx == M_COLOR_ROW_CHROMA) {
            s->c += (delta / 100.0f) * M_OKLCH_MAX_CHROMA;
            CLAMP(s->c, 0.0f, M_OKLCH_MAX_CHROMA);
        } else {
            s->l += delta / 100.0f;
            CLAMP(s->l, 0.0f, 1.0f);
        }
        M_EmitLocalColorAsRGB(s);
    } else if (g_InputDB.unbind_key) {
        Config_RestoreOptionDefault(option->target);
        M_SetLocalColorFromRGB(s, *(RGB_888 *)option->target);
        Config_Update();
    }
}

void UI_ColorEditorDialog(UI_COLOR_EDITOR_DIALOG_STATE *const s)
{
    if (s == nullptr || !s->show || s->option == nullptr) {
        return;
    }

    UI_BeginModal(0.5f, 0.5f);
    UI_BeginFrame(UI_FRAME_DIALOG_BACKGROUND_HEAVY);
    UI_BeginPad(M_COLOR_EDITOR_PADDING, M_COLOR_EDITOR_PADDING);
    UI_BeginStackEx((UI_STACK_SETTINGS) {
        .orientation = UI_STACK_VERTICAL,
        .spacing = { .v = 5.0f },
        .align = { .h = UI_STACK_H_ALIGN_SPAN },
    });
    UI_BeginAnchor(0.5f, 0.5f);
    const char *const title =
        Config_GetOptionTitle(Config_GetOption(s->option->target));
    UI_Label(title != nullptr ? title : "");
    UI_EndAnchor();
    UI_Spacer(M_COLOR_EDITOR_TITLE_MARGIN, M_COLOR_EDITOR_TITLE_MARGIN);

    UI_BeginStackEx((UI_STACK_SETTINGS) {
        .orientation = UI_STACK_VERTICAL,
        .align = { .h = UI_STACK_H_ALIGN_SPAN },
        .spacing = { .v = 4.0f },
    });

    UI_BeginStackEx((UI_STACK_SETTINGS) {
        .orientation = UI_STACK_HORIZONTAL,
        .align = { .h = UI_STACK_H_ALIGN_DISTRIBUTE },
        .spacing = { .h = 10.0f },
    });
    UI_LabelFmt("#%02X%02X%02X", s->color.r, s->color.g, s->color.b);
    UI_BeginRowArrows(false, false, UI_ROW_ARROWS_MEDIUM);
    UI_ColorSwatch((UI_COLOR_SWATCH_SETTINGS) {
        .color = s->color,
        .w = 48.0f,
        .h = 12.0f,
    });
    UI_EndRowArrows();
    UI_EndStack();

    M_ColorEditorRow(s, M_COLOR_ROW_HUE);
    M_ColorEditorRow(s, M_COLOR_ROW_CHROMA);
    M_ColorEditorRow(s, M_COLOR_ROW_LIGHTNESS);

    UI_EndStack();

    UI_EndStack();
    UI_EndPad();
    UI_EndFrame();
    UI_EndModal();
}
