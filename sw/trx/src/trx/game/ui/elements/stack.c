#include <trx/game/ui/elements/stack.h>

#include <trx/config.h>
#include <trx/core/utils.h>
#include <trx/debug.h>
#include <trx/game/ui/helpers.h>

#include <math.h>
#include <stdint.h>

typedef struct {
    UI_STACK_SETTINGS settings;
} M_DATA;

static float M_CalcChildW(const UI_NODE *const node, const UI_NODE *const child)
{
    M_DATA *const data = node->data;
    if (data->settings.align.h == UI_STACK_H_ALIGN_SPAN) {
        return MAX(child->measure_w, node->w);
    }
    return child->measure_w;
}

static float M_CalcChildH(const UI_NODE *const node, const UI_NODE *const child)
{
    M_DATA *const data = node->data;
    if (data->settings.align.v == UI_STACK_V_ALIGN_SPAN) {
        return MAX(child->measure_h, node->h);
    }
    return child->measure_h;
}

static float M_CalcStartX(const UI_NODE *const node, const UI_NODE *const child)
{
    M_DATA *const data = node->data;
    switch (data->settings.align.h) {
    case UI_STACK_H_ALIGN_SPAN:
    case UI_STACK_H_ALIGN_LEFT:
        return node->x;
    case UI_STACK_H_ALIGN_CENTER:
        return node->x + (node->w - child->measure_w) * 0.5f;
    case UI_STACK_H_ALIGN_RIGHT:
        return node->x + node->w - child->measure_w;
    case UI_STACK_H_ALIGN_DISTRIBUTE:
        ASSERT_FAIL();
    }
    return 0.0f;
}

static float M_CalcStartY(const UI_NODE *const node, const UI_NODE *const child)
{
    M_DATA *const data = node->data;
    switch (data->settings.align.v) {
    case UI_STACK_V_ALIGN_SPAN:
    case UI_STACK_V_ALIGN_TOP:
        return node->y;
    case UI_STACK_V_ALIGN_CENTER:
        return node->y + (node->h - child->measure_h) * 0.5f;
    case UI_STACK_V_ALIGN_BOTTOM:
        return node->y + node->h - child->measure_h;
    case UI_STACK_V_ALIGN_DISTRIBUTE:
        ASSERT_FAIL();
    }
    return 0.0f;
}

static void M_Measure(UI_NODE *const node)
{
    node->measure_w = 0.0f;
    node->measure_h = 0.0f;
    UI_NODE *child = node->first_child;
    M_DATA *const data = node->data;
    const float scale = g_Config.ui.text_scale;
    while (child != nullptr) {
        if (data->settings.orientation == UI_STACK_VERTICAL) {
            node->measure_w = MAX(node->measure_w, child->measure_w);
            node->measure_h += child->measure_h;
            if (child->next_sibling != nullptr) {
                node->measure_h += data->settings.spacing.v * scale;
            }
        } else {
            node->measure_h = MAX(node->measure_h, child->measure_h);
            node->measure_w += child->measure_w;
            if (child->next_sibling != nullptr) {
                node->measure_w += data->settings.spacing.h * scale;
            }
        }
        child = child->next_sibling;
    }
}

static void M_Layout(
    UI_NODE *const node, const float x, const float y, const float w,
    const float h)
{
    UI_LayoutBasic(node, x, y, w, h);
    M_DATA *const data = node->data;

    // Count children and compute the total size they occupy on the main axis
    // including the base spacing from the settings.
    int32_t child_count = 0;
    float total_child_main_size = 0.0f;
    UI_NODE *child = node->first_child;
    while (child != nullptr) {
        switch (data->settings.orientation) {
        case UI_STACK_HORIZONTAL:
            total_child_main_size += child->measure_w;
            break;
        case UI_STACK_VERTICAL:
            total_child_main_size += child->measure_h;
            break;
        }
        child_count++;
        child = child->next_sibling;
    }

    // If there is at least one gap between children, compute the normal
    // (configured) total spacing on the main axis. If only 1 child or 0
    // children, there's no gap to distribute leftover space into.
    const int32_t gaps = (child_count > 1) ? (child_count - 1) : 0;
    const float scale = g_Config.ui.text_scale;
    float base_spacing = 0.0f;
    switch (data->settings.orientation) {
    case UI_STACK_HORIZONTAL:
        base_spacing = data->settings.spacing.h * gaps * scale;
        break;
    case UI_STACK_VERTICAL:
        base_spacing = data->settings.spacing.v * gaps * scale;
        break;
    }

    // The space that the children + base spacing absolutely need
    const float needed_size = total_child_main_size + base_spacing;

    // The leftover that we can distribute among the (child_count - 1) internal
    // gaps.
    float leftover = 0.0f;
    float extra_per_gap = 0.0f;
    switch (data->settings.orientation) {
    case UI_STACK_HORIZONTAL:
        leftover = w - needed_size;
        break;
    case UI_STACK_VERTICAL:
        leftover = h - needed_size;
        break;
    }

    if ((data->settings.orientation == UI_STACK_HORIZONTAL
         && data->settings.align.h == UI_STACK_H_ALIGN_DISTRIBUTE)
        || (data->settings.orientation == UI_STACK_VERTICAL
            && data->settings.align.v == UI_STACK_V_ALIGN_DISTRIBUTE)) {
        if (gaps > 0 && leftover > 0.0f) {
            extra_per_gap = leftover / (float)gaps;
        }
    }

    // Now we actually lay out the children
    float cx = x;
    float cy = y;
    child = node->first_child;
    while (child != nullptr) {
        const float cw = M_CalcChildW(node, child);
        const float ch = M_CalcChildH(node, child);

        switch (data->settings.orientation) {
        case UI_STACK_HORIZONTAL:
            // For horizontal: vertical alignment is determined by M_CalcStartY
            cy = M_CalcStartY(node, child);

            // Lay out the child
            child->ops.layout(child, cx, cy, cw, ch);

            // Advance cx for the next child
            cx += cw;
            // Add normal spacing + any extra leftover that we are distributing
            if (child->next_sibling != nullptr) {
                cx += data->settings.spacing.h * scale + extra_per_gap;
            }
            break;

        case UI_STACK_VERTICAL:
            cx = M_CalcStartX(node, child);

            child->ops.layout(child, cx, cy, cw, ch);

            cy += ch;
            if (child->next_sibling != nullptr) {
                cy += data->settings.spacing.v * scale + extra_per_gap;
            }
            break;
        }

        child = child->next_sibling;
    }
}

UI_NODE *UI_CreateStack(const UI_STACK_SETTINGS settings)
{
    UI_NODE *const node = UI_AllocNode(
        &(UI_WIDGET_OPS) {
            .measure = M_Measure,
            .layout = M_Layout,
            .draw = UI_DrawWrapper,
        },
        sizeof(M_DATA));
    if (node == nullptr) {
        return nullptr;
    }
    M_DATA *const data = node->data;
    data->settings = settings;
    return node;
}

void UI_BeginStack(const UI_STACK_ORIENTATION orientation)
{
    UI_BeginStackEx((UI_STACK_SETTINGS) {
        .orientation = orientation,
        .align = {
            .h = UI_STACK_H_ALIGN_LEFT,
            .v = UI_STACK_V_ALIGN_TOP,
        },
        .spacing = {
            .h = 0.0f,
            .v = 0.0f,
        },
    });
}

void UI_BeginStackEx(const UI_STACK_SETTINGS settings)
{
    UI_NODE *const child = UI_CreateStack(settings);
    UI_AddChild(child);
    UI_PushCurrent(child);
}

void UI_EndStack(void)
{
    UI_PopCurrent();
}
