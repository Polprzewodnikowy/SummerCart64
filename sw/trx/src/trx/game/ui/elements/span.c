#include <trx/game/ui/elements/span.h>

#include <trx/core/utils.h>
#include <trx/game/ui/helpers.h>

static void M_Measure(UI_NODE *const node)
{
    node->measure_w = 0.0f;
    node->measure_h = 0.0f;
    const UI_NODE *child = node->first_child;
    while (child != nullptr) {
        node->measure_w = MAX(node->measure_w, child->measure_w);
        node->measure_h = MAX(node->measure_h, child->measure_h);
        child = child->next_sibling;
    }
}

static void M_Layout(
    UI_NODE *const node, const float x, const float y, const float w,
    const float h)
{
    UI_LayoutBasic(node, x, y, w, h);
    UI_NODE *child = node->first_child;
    while (child != nullptr) {
        child->ops.layout(child, x, y, node->measure_w, node->measure_h);
        child = child->next_sibling;
    }
}

void UI_BeginSpan(void)
{
    UI_NODE *const node = UI_AllocNode(
        &(UI_WIDGET_OPS) {
            .measure = UI_MeasureWrapper,
            .layout = M_Layout,
            .draw = UI_DrawWrapper,
        },
        0);
    UI_AddChild(node);
    UI_PushCurrent(node);
}

void UI_EndSpan(void)
{
    UI_PopCurrent();
}
