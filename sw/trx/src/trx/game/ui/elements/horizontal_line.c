#include <trx/game/ui/elements/horizontal_line.h>

#include <trx/config.h>
#include <trx/game/output.h>
#include <trx/game/ui/draw.h>
#include <trx/game/ui/helpers.h>

static void M_Draw(const UI_NODE *node)
{
    // UI_DrawWrapper(node);
    UI_ScheduleDrawHorizontalLine(
        g_Config.ui.menu_style, UI_ScaleX(node->x),
        UI_ScaleX(node->x + node->w), UI_ScaleY(node->y + node->h / 2.0f), 0);
}

static void M_Measure(UI_NODE *const node)
{
    UI_MeasureWrapper(node);
    node->measure_h = 2 * g_Config.ui.text_scale;
}

void UI_HorizontalLine(void)
{
    UI_NODE *const node = UI_AllocNode(
        &(UI_WIDGET_OPS) {
            .measure = M_Measure,
            .layout = UI_LayoutBasic,
            .draw = M_Draw,
        },
        0);
    UI_AddChild(node);
}
