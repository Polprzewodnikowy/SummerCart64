#include <trx/game/ui/elements/sleek_bar.h>

#include <trx/config.h>
#include <trx/core/utils.h>
#include <trx/game/ui/draw.h>
#include <trx/game/ui/helpers.h>
#include <trx/version.h>

typedef struct {
    float progress;
} M_DATA;

typedef struct {
    float x, y, w, h;
    RGBA_8888 color;
} M_RECT;

static RGBA_8888 m_BackgroundColor = { 0x06, 0x06, 0x06, 0xFF };
static RGBA_8888 m_FillColors[TR_VERSION_COUNT] = {
    { 0xA1, 0x83, 0x3C, 0xFF },
    { 0x5A, 0xB5, 0x5A, 0xFF },
    { 0x1C, 0x6A, 0xC4, 0xFF },
};

static void M_Measure(UI_NODE *const node)
{
    node->measure_w = 0.0f;
    node->measure_h = 4.0f * g_Config.ui.text_scale;
}

static void M_Draw(const UI_NODE *const node)
{
    const M_DATA *const data = node->data;
    const float border = 1.0f;

    const M_RECT out = {
        .x = UI_ScaleX(node->x),
        .y = UI_ScaleY(node->y),
        .w = UI_ScaleX(node->w),
        .h = UI_ScaleY(node->h),
        .color = m_BackgroundColor,
    };
    const M_RECT in = {
        .x = UI_ScaleX(node->x + border),
        .y = UI_ScaleY(node->y + border),
        .w = UI_ScaleX(node->w - border * 2.0f) * data->progress,
        .h = UI_ScaleY(node->h - border * 2.0f),
        .color = m_FillColors[g_TRVersion - 1],
    };

    UI_ScheduleDrawScreenFlatQuad(out.x, out.y, 0, out.w, out.h, out.color);
    UI_ScheduleDrawScreenFlatQuad(in.x, in.y, 0, in.w, in.h, in.color);
}

void UI_SleekBar(float progress)
{
    UI_NODE *const node = UI_AllocNode(
        &(UI_WIDGET_OPS) {
            .measure = M_Measure,
            .layout = UI_LayoutBasic,
            .draw = M_Draw,
        },
        sizeof(M_DATA));
    M_DATA *const data = node->data;
    CLAMP(progress, 0.0f, 1.0f);
    data->progress = progress;
    UI_AddChild(node);
}
