#include <trx/game/ui/elements/scrollable_stack.h>

#include <trx/config.h>
#include <trx/core/utils.h>
#include <trx/game/input.h>
#include <trx/game/ui/helpers.h>

typedef struct {
    UI_SCROLLABLE *scroll;
    UI_SCROLLABLE_STACK_SETTINGS settings;
} M_DATA;

static int32_t M_CountChildren(const UI_NODE *const node)
{
    int32_t count = 0;
    const UI_NODE *child = node->first_child;
    while (child != nullptr) {
        count++;
        child = child->next_sibling;
    }
    return count;
}

static void M_ClampScroll(UI_SCROLLABLE *const s)
{
    CLAMPG(s->first_item, s->max_items - s->vis_items);
    CLAMPL(s->first_item, 0);
}

static void M_Measure(UI_NODE *const node)
{
    M_DATA *const data = node->data;
    UI_SCROLLABLE *const s = data->scroll;
    s->max_items = M_CountChildren(node);
    CLAMPL(s->vis_items, 0);
    M_ClampScroll(s);

    node->measure_w = 0.0f;
    node->measure_h = 0.0f;

    const int32_t first = s->first_item;
    const int32_t last = MIN(first + s->vis_items, s->max_items);
    const float scale = g_Config.ui.text_scale;

    int32_t visible_count = 0;
    int32_t i = 0;
    const UI_NODE *child = node->first_child;
    while (child != nullptr) {
        if (i >= first && i < last) {
            if (data->settings.orientation == UI_STACK_VERTICAL) {
                node->measure_w = MAX(node->measure_w, child->measure_w);
                node->measure_h += child->measure_h;
            } else {
                node->measure_h = MAX(node->measure_h, child->measure_h);
                node->measure_w += child->measure_w;
            }
            visible_count++;
        }
        i++;
        child = child->next_sibling;
    }

    const int32_t gaps = (visible_count > 1) ? (visible_count - 1) : 0;
    if (data->settings.orientation == UI_STACK_VERTICAL) {
        node->measure_h += (float)gaps * data->settings.spacing * scale;
    } else {
        node->measure_w += (float)gaps * data->settings.spacing * scale;
    }
}

static void M_Layout(
    UI_NODE *const node, const float x, const float y, const float w,
    const float h)
{
    UI_LayoutBasic(node, x, y, w, h);

    M_DATA *const data = node->data;
    const UI_SCROLLABLE *const s = data->scroll;
    const int32_t first = s->first_item;
    const int32_t last = MIN(first + s->vis_items, s->max_items);
    const float scale = g_Config.ui.text_scale;

    float cx = x;
    float cy = y;

    int32_t i = 0;
    UI_NODE *child = node->first_child;
    while (child != nullptr) {
        if (i >= first && i < last) {
            if (data->settings.orientation == UI_STACK_VERTICAL) {
                child->ops.layout(child, x, cy, w, child->measure_h);
                cy += child->measure_h;
                if (i + 1 < last) {
                    cy += data->settings.spacing * scale;
                }
            } else {
                child->ops.layout(child, cx, y, child->measure_w, h);
                cx += child->measure_w;
                if (i + 1 < last) {
                    cx += data->settings.spacing * scale;
                }
            }
        }
        i++;
        child = child->next_sibling;
    }
}

static void M_Draw(const UI_NODE *const node)
{
    const M_DATA *const data = node->data;
    const UI_SCROLLABLE *const s = data->scroll;
    const int32_t first = s->first_item;
    const int32_t last = MIN(first + s->vis_items, s->max_items);

    int32_t i = 0;
    const UI_NODE *child = node->first_child;
    while (child != nullptr) {
        if (i >= first && i < last && child->ops.draw != nullptr) {
            child->ops.draw(child);
        }
        i++;
        child = child->next_sibling;
    }
}

bool UI_ScrollableStack_Control(
    UI_SCROLLABLE *const s, const UI_STACK_ORIENTATION orientation)
{
    if (orientation == UI_STACK_VERTICAL) {
        if (g_InputDB.menu_down) {
            return UI_Scrollable_ScrollDown(s, g_Config.ui.enable_wraparound);
        } else if (g_InputDB.menu_up) {
            return UI_Scrollable_ScrollUp(s, g_Config.ui.enable_wraparound);
        }
    } else {
        if (g_InputDB.menu_right || g_InputDB.step_right) {
            return UI_Scrollable_ScrollDown(s, g_Config.ui.enable_wraparound);
        } else if (g_InputDB.menu_left || g_InputDB.step_left) {
            return UI_Scrollable_ScrollUp(s, g_Config.ui.enable_wraparound);
        }
    }
    return false;
}

void UI_BeginScrollableStack(
    UI_SCROLLABLE *const s, const UI_SCROLLABLE_STACK_SETTINGS settings)
{
    UI_NODE *const node = UI_AllocNode(
        &(UI_WIDGET_OPS) {
            .measure = M_Measure,
            .layout = M_Layout,
            .draw = M_Draw,
        },
        sizeof(M_DATA));
    if (node == nullptr) {
        return;
    }
    M_DATA *const data = node->data;
    data->scroll = s;
    data->settings = settings;
    UI_AddChild(node);
    UI_PushCurrent(node);
}

void UI_EndScrollableStack(void)
{
    UI_PopCurrent();
}
