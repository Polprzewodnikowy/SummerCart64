#include <trx/game/ui/elements/bar_lara_sprint.h>

#include <trx/config.h>
#include <trx/game/lara/common.h>
#include <trx/game/lara/const.h>
#include <trx/game/ui/elements/bar.h>

bool UI_LaraSprintBar(void)
{
    const ITEM *const lara_item = Lara_GetItem();
    if (lara_item == nullptr) {
        return false;
    }
    const LARA_INFO *const lara = Lara_GetLaraInfo();
    const bool show =
        g_Config.ui.show_bars && lara->sprint_timer < LARA_MAX_SPRINT;
    if (!show) {
        return false;
    }

    UI_Bar((UI_BAR_SETTINGS) {
        .type = UI_BAR_LARA_STAMINA,
        .w = UI_BAR_WIDTH,
        .h = UI_BAR_HEIGHT,
        .value = lara->sprint_timer,
        .max_value = LARA_MAX_SPRINT,
    });
    return true;
}
