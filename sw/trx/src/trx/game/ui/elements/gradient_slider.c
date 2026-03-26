#include <trx/game/ui/elements/gradient_slider.h>

#include <trx/config.h>
#include <trx/core/colors.h>
#include <trx/core/utils.h>
#include <trx/debug.h>
#include <trx/game/ui/draw.h>
#include <trx/game/ui/helpers.h>
#include <trx/game/ui/text.h>

#include <math.h>
#include <string.h>

#define M_MARKER_BORDER 1.0f
#define M_MARKER_WIDTH 4.0f

typedef struct {
    float width;
    float value;
    int32_t stop_count;
} M_DATA;

static void M_DrawMarker(const float x, const float y, const float h)
{
    const float inner_w_s = UI_ScaleX(M_MARKER_WIDTH);
    const float inner_h_s = UI_ScaleY(h);
    const int32_t border_s = UI_ScaleX(M_MARKER_BORDER);

    // Compute pixel‑snapped corners so both sides are balanced
    const int32_t inner_x1 = floorf(x - inner_w_s * 0.5f + 0.5f);
    const int32_t inner_y1 = floorf(y - inner_h_s * 0.5f + 0.5f);
    const int32_t inner_x2 = floorf(x + inner_w_s * 0.5f + 0.5f);
    const int32_t inner_y2 = floorf(y + inner_h_s * 0.5f + 0.5f);

    const int32_t outer_x1 = inner_x1 - border_s;
    const int32_t outer_y1 = inner_y1 - border_s;
    const int32_t outer_x2 = inner_x2 + border_s;
    const int32_t outer_y2 = inner_y2 + border_s;

    // Derive final snapped widths/heights
    const int32_t inner_w = inner_x2 - inner_x1;
    const int32_t inner_h = inner_y2 - inner_y1;
    const int32_t outer_w = outer_x2 - outer_x1;
    const int32_t outer_h = outer_y2 - outer_y1;

    UI_ScheduleDrawScreenFlatQuad(
        outer_x1, outer_y1, 0, outer_w, outer_h, COLOR_RGBA_8888_BLACK);
    UI_ScheduleDrawScreenFlatQuad(
        inner_x1, inner_y1, 0, inner_w, inner_h, COLOR_RGBA_8888_WHITE);

    UI_ScheduleDrawTextOutline(
        g_Config.ui.menu_style, outer_x1, outer_y1, 0, outer_w - 1, outer_h - 1,
        TS_REQUESTED);
}

static void M_Draw(const UI_NODE *const node)
{
    const M_DATA *const data = node->data;
    const RGB_888 *const stops = (const RGB_888 *)(data + 1);

    const float x = UI_ScaleX(node->x);
    const float y = UI_ScaleY(node->y);
    const float w = UI_ScaleX(node->w);
    const float h = UI_ScaleY(node->h);
    const float border = UI_ScaleX(1.0f);
    const float inner_x = x + border;
    const float inner_y = y + border;
    const float inner_w = w - border * 2;
    const float inner_h = h - border * 2;

    UI_ScheduleDrawScreenFlatQuad(x, y, 0, w, h, COLOR_RGBA_8888_BLACK);

    if (data->stop_count == 1) {
        UI_ScheduleDrawScreenFlatQuad(
            inner_x, inner_y, 0, inner_w, inner_h, Color_RGBToRGBA(stops[0]));
    } else {
        for (int32_t i = 0; i < data->stop_count - 1; i++) {
            const int32_t sx = inner_x + i * inner_w / (data->stop_count - 1);
            const int32_t sw =
                inner_x + (i + 1) * inner_w / (data->stop_count - 1) - sx;
            UI_ScheduleDrawScreenGradientQuad(
                sx, inner_y, 0, MAX(1, sw), inner_h, Color_RGBToRGBA(stops[i]),
                Color_RGBToRGBA(stops[i + 1]), Color_RGBToRGBA(stops[i]),
                Color_RGBToRGBA(stops[i + 1]));
        }
    }

    float marker_x = inner_x + data->value * inner_w;
    float marker_y = inner_y + inner_h * 0.5f;
    M_DrawMarker(marker_x, marker_y, node->h);
}

static void M_Measure(UI_NODE *const node)
{
    const M_DATA *const data = node->data;
    node->measure_w = data->width;
    node->measure_h = UI_TEXT_HEIGHT * 0.5f * g_Config.ui.text_scale;
}

void UI_GradientSlider(const UI_GRADIENT_SLIDER_SETTINGS settings)
{
    ASSERT(settings.stop_count > 0);
    ASSERT(settings.stops != nullptr);

    const size_t extra_size =
        sizeof(M_DATA) + sizeof(RGB_888) * settings.stop_count;
    UI_NODE *const node = UI_AllocNode(
        &(UI_WIDGET_OPS) {
            .measure = M_Measure,
            .layout = UI_LayoutBasic,
            .draw = M_Draw,
        },
        extra_size);
    M_DATA *const data = node->data;
    data->width = settings.width;
    data->value = settings.value;
    CLAMP(data->value, 0.0f, 1.0f);
    data->stop_count = settings.stop_count;
    RGB_888 *const stops = (RGB_888 *)(data + 1);
    memcpy(stops, settings.stops, sizeof(RGB_888) * settings.stop_count);
    UI_AddChild(node);
}
