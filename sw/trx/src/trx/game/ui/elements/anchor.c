#include <trx/game/ui/elements/anchor.h>

#include <trx/game/ui/helpers.h>
#include <trx/game/ui/text.h>

typedef struct {
    float x;
    float y;
} M_DATA;

static void M_Measure(UI_NODE *const node)
{
    node->measure_w = UI_GetCanvasWidth();
    node->measure_h = UI_GetCanvasHeight() - UI_TEXT_HEIGHT;
}

static void M_Layout(
    UI_NODE *const node, const float x, const float y, const float w,
    const float h)
{
    UI_LayoutBasic(node, x, y, w, h);
    const M_DATA *const data = node->data;
    UI_NODE *child = node->first_child;
    while (child != nullptr) {
        const float cw = child->measure_w;
        const float ch = child->measure_h;
        const float cx = x + (w - cw) * data->x;
        const float cy = y + (h - ch) * data->y;
        child->ops.layout(child, cx, cy, cw, ch);
        child = child->next_sibling;
    }
}

void UI_BeginAnchor(const float x, const float y)
{
    UI_NODE *const node = UI_AllocNode(
        &(UI_WIDGET_OPS) {
            .measure = UI_MeasureWrapper,
            .layout = M_Layout,
            .draw = UI_DrawWrapper,
        },
        sizeof(M_DATA));
    M_DATA *const data = node->data;
    data->x = x;
    data->y = y;
    UI_AddChild(node);
    UI_PushCurrent(node);
}

void UI_EndAnchor(void)
{
    UI_PopCurrent();
}
