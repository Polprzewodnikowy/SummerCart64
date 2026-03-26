#include <trx/game/ui/elements/row_arrows.h>

#include <trx/game/ui/elements/hide.h>
#include <trx/game/ui/elements/label.h>
#include <trx/game/ui/elements/stack.h>
#include <trx/game/ui/helpers.h>

void UI_BeginRowArrows(
    const bool left_arrow, const bool right_arrow, const int32_t spacing)
{
    UI_NODE *const node = UI_AllocNode(
        &(UI_WIDGET_OPS) {
            .measure = UI_MeasureWrapper,
            .layout = UI_LayoutWrapper,
            .draw = UI_DrawWrapper,
        },
        sizeof(bool));
    *(bool *)node->data = right_arrow;
    UI_BeginStackEx((UI_STACK_SETTINGS) {
        .orientation = UI_STACK_HORIZONTAL,
        .align = {
            .h = UI_STACK_H_ALIGN_DISTRIBUTE,
            .v = UI_STACK_V_ALIGN_CENTER,
        },
        .spacing = { .h = spacing },
    });
    UI_BeginHide(!left_arrow);
    UI_Label("\\{button left}");
    UI_EndHide();

    UI_AddChild(node);
    UI_PushCurrent(node);
}

void UI_EndRowArrows(void)
{
    const UI_NODE *const node = UI_GetCurrent();
    const bool right_arrow = *(bool *)(intptr_t)node->data;
    UI_PopCurrent();
    UI_BeginHide(!right_arrow);
    UI_Label("\\{button right}");
    UI_EndHide();
    UI_EndStack();
}
