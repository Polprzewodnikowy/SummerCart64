#include <trx/game/ui/elements/button_label.h>

#include <trx/core/memory.h>
#include <trx/core/strings.h>
#include <trx/game/ui/elements/label.h>

void UI_ButtonLabel(INPUT_ROLE input_role, const char *const label)
{
    UI_ButtonLabelEx(Input_GetRoleName(input_role), label);
}

void UI_ButtonLabelEx(const char *const input_label, const char *const label)
{
    UI_LabelFmt("\\{button empty} %s: %s", input_label, label);
}
