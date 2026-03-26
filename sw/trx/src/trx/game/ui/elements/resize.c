#include <trx/game/ui/elements/resize.h>

#include <trx/config.h>
#include <trx/game/ui/helpers.h>

typedef struct {
    float x;
    float y;
} M_DATA;

static void M_Measure(UI_NODE *const node)
{
    UI_MeasureWrapper(node);
    const M_DATA *const data = node->data;
    if (data->x >= 0.0f) {
        node->measure_w = data->x;
    }
    if (data->y >= 0.0f) {
        node->measure_h = data->y;
    }
}

static void M_Draw(const UI_NODE *const node)
{
    if (node->measure_w <= 0.0f || node->measure_h <= 0.0f) {
        return;
    }
    UI_DrawWrapper(node);
}

void UI_BeginResize(const float x, const float y)
{
    UI_NODE *const node = UI_AllocNode(
        &(UI_WIDGET_OPS) {
            .measure = M_Measure,
            .layout = UI_LayoutWrapper,
            .draw = M_Draw,
        },
        sizeof(M_DATA));
    M_DATA *const data = node->data;
    data->x = x * g_Config.ui.text_scale;
    data->y = y * g_Config.ui.text_scale;
    UI_AddChild(node);
    UI_PushCurrent(node);
}

void UI_EndResize(void)
{
    UI_PopCurrent();
}
