#include <trx/game/ui/elements/bar.h>

#include <trx/config.h>
#include <trx/core/json.h>
#include <trx/core/json/util/file.h>
#include <trx/core/memory.h>
#include <trx/core/strings.h>
#include <trx/core/utils.h>
#include <trx/game/output.h>
#include <trx/game/shell.h>
#include <trx/game/ui/draw.h>
#include <trx/game/ui/helpers.h>
#include <trx/game/ui/scaler.h>
#include <trx/version.h>

#include <math.h>

typedef struct {
    int32_t x, y, w, h;
} M_RECT_32;

typedef struct {
    UI_BAR_SETTINGS settings;
    const UI_BAR_THEME *theme;
    float scale;
} M_DATA;

static void M_Measure(UI_NODE *const node)
{
    M_DATA *const data = node->data;
    const float scale = UI_Scaler_GetScale(
                            data->settings.preview ? UI_SCALER_TARGET_TEXT
                                                   : UI_SCALER_TARGET_BAR)
        * data->scale;
    node->measure_w = data->settings.w * scale;
    node->measure_h = data->settings.h * scale;
}

static void M_DrawBackground(
    const UI_BAR_THEME *const theme, const M_RECT_32 rect)
{
    UI_ScheduleDrawScreenFlatQuad(
        rect.x, rect.y, 0, rect.w, rect.h, (RGBA_8888) { 0, 0, 0, 255 });
}

static void M_DrawBorderPC(
    const UI_BAR_THEME *const theme, const M_RECT_32 rect, const float border)
{
    UI_ScheduleDrawScreenFlatQuad(
        rect.x, rect.y, 0, rect.w, rect.h, theme->border_light);
    UI_ScheduleDrawScreenFlatQuad(
        rect.x + border, rect.y + border, 0, rect.w - border, rect.h - border,
        theme->border_dark);
}

static void M_DrawBorderPS1(
    const UI_BAR_THEME *const theme, const M_RECT_32 rect, const float border)
{
#if 0
    Output_DrawScreenGradientQuad(
        rect.x - border, rect.y + border, 0, rect.w + border * 2, rect.h - border * 2, theme->border_bl, theme->border_br, theme->border_br, theme->border_bl);
#endif
    Output_DrawScreenGradientQuad(
        rect.x, rect.y, 0, rect.w, rect.h, theme->border_tl, theme->border_tr,
        theme->border_br, theme->border_bl);
}

static void M_DrawFillPC(
    const UI_BAR_THEME *const theme, const UI_BAR_SETTINGS *const settings,
    const M_RECT_32 rect, const float percent)
{
    if (g_Config.ui.enable_smooth_bars) {
        for (int32_t i = 0; i < UI_BAR_COLOR_STEPS - 1; i++) {
            const RGBA_8888 c1 = theme->ramp[i];
            const RGBA_8888 c2 = theme->ramp[i + 1];
            const int32_t lsy = rect.y + i * rect.h / (UI_BAR_COLOR_STEPS - 1);
            const int32_t lsh =
                rect.y + (i + 1) * rect.h / (UI_BAR_COLOR_STEPS - 1) - lsy;
            UI_ScheduleDrawScreenGradientQuad(
                rect.x, lsy, 0, rect.w, lsh, c1, c1, c2, c2);
        }
    } else {
        for (int32_t i = 0; i < UI_BAR_COLOR_STEPS; i++) {
            const RGBA_8888 c = theme->ramp[i];
            const int32_t lsy = rect.y + i * rect.h / UI_BAR_COLOR_STEPS;
            const int32_t lsh =
                rect.y + (i + 1) * rect.h / UI_BAR_COLOR_STEPS - lsy;
            UI_ScheduleDrawScreenFlatQuad(rect.x, lsy, 0, rect.w, lsh, c);
        }
    }
}

static void M_DrawFillPS1(
    const UI_BAR_THEME *const theme, const UI_BAR_SETTINGS *const settings,
    const M_RECT_32 rect, const float percent)
{
    const UI_BAR_TYPE type = settings->type;
    if (g_Config.ui.enable_smooth_bars) {
        for (int32_t i = 0; i < UI_BAR_COLOR_STEPS - 1; i++) {
            const RGBA_8888 ctl = theme->ramp_left[i];
            const RGBA_8888 ctr = theme->ramp_right[i];
            const RGBA_8888 cbl = theme->ramp_left[i + 1];
            const RGBA_8888 cbr = theme->ramp_right[i + 1];
            const RGBA_8888 ctrm = Color_Mix(ctl, ctr, percent);
            const RGBA_8888 cbrm = Color_Mix(cbl, cbr, percent);
            const int32_t lsy = rect.y + i * rect.h / (UI_BAR_COLOR_STEPS - 1);
            const int32_t lsh =
                rect.y + (i + 1) * rect.h / (UI_BAR_COLOR_STEPS - 1) - lsy;
            UI_ScheduleDrawScreenGradientQuad(
                rect.x, lsy, 0, rect.w, lsh, ctl, ctrm, cbl, cbrm);
        }
    } else {
        for (int32_t i = 0; i < UI_BAR_COLOR_STEPS; i++) {
            const RGBA_8888 cl = theme->ramp_left[i];
            const RGBA_8888 cr = theme->ramp_right[i];
            const RGBA_8888 crm = Color_Mix(cl, cr, percent);
            const int32_t lsy = rect.y + i * rect.h / UI_BAR_COLOR_STEPS;
            const int32_t lsh =
                rect.y + (i + 1) * rect.h / UI_BAR_COLOR_STEPS - lsy;
            UI_ScheduleDrawScreenGradientQuad(
                rect.x, lsy, 0, rect.w, lsh, cl, crm, cl, crm);
        }
    }
}

static void M_Draw(const UI_NODE *const node)
{
    M_DATA *const data = node->data;
    const UI_BAR_SETTINGS *const settings = &data->settings;

    float percent = settings->value / (float)MAX(1, settings->max_value);
    CLAMP(percent, 0.0f, 1.0f);
    percent = (int32_t)(percent * 100) / 100.0f;

    // Convert everything to screen coordinates
    const int32_t x = UI_ScaleX(node->x);
    const int32_t y = UI_ScaleY(node->y);
    const int32_t w = UI_ScaleX(node->w);
    const int32_t h = UI_ScaleY(node->h);
    const int32_t border = h / (float)(UI_BAR_COLOR_STEPS + 4);
    const int32_t padding = h / (float)(UI_BAR_COLOR_STEPS + 4);
    const M_RECT_32 outer_rect = {
        .x = x,
        .y = y,
        .w = w,
        .h = h,
    }, inner_rect = {
        .x = outer_rect.x + border,
        .y = outer_rect.y + border,
        .w = outer_rect.w - border * 2,
        .h = outer_rect.h - border * 2,
    }, bar_rect = {
        .x = inner_rect.x + padding,
        .y = inner_rect.y + padding,
        .w = (inner_rect.w - padding * 2) * percent,
        .h = inner_rect.h - padding * 2,
    };

    switch (data->theme->kind) {
    case UI_BAR_THEME_PC_KIND:
        M_DrawBorderPC(data->theme, outer_rect, border);
        M_DrawBackground(data->theme, inner_rect);
        if (percent > 0.0f) {
            M_DrawFillPC(data->theme, settings, bar_rect, percent);
        }
        break;
    case UI_BAR_THEME_PS1_KIND:
        M_DrawBorderPS1(data->theme, outer_rect, border);
        M_DrawBackground(data->theme, inner_rect);
        if (percent > 0.0f) {
            M_DrawFillPS1(data->theme, settings, bar_rect, percent);
        }
        break;
    }
}

void UI_Bar(const UI_BAR_SETTINGS settings)
{
    UI_NODE *const node = UI_AllocNode(
        &(UI_WIDGET_OPS) {
            .measure = M_Measure,
            .layout = UI_LayoutBasic,
            .draw = M_Draw,
        },
        sizeof(M_DATA));
    M_DATA *const data = node->data;
    data->settings = settings;
    data->theme = UI_Settings_GetBarTheme(settings.type);
    data->scale = data->settings.preview ? 1.0f : data->theme->basic_scale;
    UI_AddChild(node);
}
