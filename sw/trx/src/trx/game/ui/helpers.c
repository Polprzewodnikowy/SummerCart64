#include <trx/game/ui/helpers.h>

#include <trx/core/utils.h>

void UI_MeasureWrapper(UI_NODE *const node)
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

void UI_LayoutBasic(
    UI_NODE *const node, const float x, const float y, const float w,
    const float h)
{
    node->x = x;
    node->y = y;
    node->w = w;
    node->h = h;
}

void UI_LayoutWrapper(
    UI_NODE *const node, const float x, const float y, const float w,
    const float h)
{
    UI_LayoutBasic(node, x, y, w, h);
    UI_NODE *child = node->first_child;
    while (child != nullptr) {
        if (child->ops.layout != nullptr) {
            child->ops.layout(child, x, y, w, h);
        }
        child = child->next_sibling;
    }
}

void UI_DrawWrapper(const UI_NODE *const node)
{
    const UI_NODE *child = node->first_child;
    while (child != nullptr) {
        if (child->ops.draw != nullptr) {
            child->ops.draw(child);
        }
        child = child->next_sibling;
    }
}
