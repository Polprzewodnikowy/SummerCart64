#include <trx/game/ui/elements/color_swatch.h>

#include <trx/config.h>
#include <trx/core/utils.h>
#include <trx/debug.h>
#include <trx/game/ui/draw.h>
#include <trx/game/ui/helpers.h>

typedef struct {
    float w;
    float h;
    RGBA_8888 color;
} M_DATA;

static void M_Measure(UI_NODE *const node)
{
    const M_DATA *const data = node->data;
    node->measure_w = data->w * g_Config.ui.text_scale;
    node->measure_h = data->h * g_Config.ui.text_scale;
}

static void M_Draw(const UI_NODE *const node)
{
    const M_DATA *const data = node->data;
    const int32_t x = UI_ScaleX(node->x);
    const int32_t y = UI_ScaleY(node->y);
    const int32_t w = UI_ScaleX(node->w);
    const int32_t h = UI_ScaleY(node->h);
    const int32_t border = MAX(1, UI_ScaleX(1.0f));
    UI_ScheduleDrawScreenFlatQuad(x, y, 0, w, h, COLOR_RGBA_8888_BLACK);
    UI_ScheduleDrawScreenFlatQuad(
        x + border, y + border, 0, w - border * 2, h - border * 2, data->color);
    UI_ScheduleDrawTextOutline(
        g_Config.ui.menu_style, x + border, y + border, 0, w - border * 2,
        h - border * 2, TS_HEADING);
}

void UI_ColorSwatch(const UI_COLOR_SWATCH_SETTINGS settings)
{
    ASSERT(settings.w > 0.0f);
    ASSERT(settings.h > 0.0f);

    UI_NODE *const node = UI_AllocNode(
        &(UI_WIDGET_OPS) {
            .measure = M_Measure,
            .layout = UI_LayoutBasic,
            .draw = M_Draw,
        },
        sizeof(M_DATA));
    M_DATA *const data = node->data;
    data->w = settings.w;
    data->h = settings.h;
    data->color = Color_RGBToRGBA(settings.color);
    UI_AddChild(node);
}
