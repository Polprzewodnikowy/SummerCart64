#include <trx/game/ui/elements/spacer.h>

#include <trx/config.h>
#include <trx/game/ui/helpers.h>

static void M_Measure(UI_NODE *const node)
{
    // already done in the constructor
}

void UI_Spacer(const float w, const float h)
{
    UI_NODE *const node = UI_AllocNode(
        &(UI_WIDGET_OPS) {
            .measure = M_Measure,
            .layout = UI_LayoutBasic,
            .draw = UI_DrawWrapper,
        },
        0);
    node->measure_w = w * g_Config.ui.text_scale;
    node->measure_h = h * g_Config.ui.text_scale;
    UI_AddChild(node);
}
