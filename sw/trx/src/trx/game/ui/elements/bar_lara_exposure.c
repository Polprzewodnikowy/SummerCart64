#include <trx/game/ui/elements/bar_lara_exposure.h>

#include <trx/config.h>
#include <trx/game/lara/common.h>
#include <trx/game/lara/const.h>
#include <trx/game/ui/elements/bar.h>

bool UI_LaraExposureBar(const bool blink_state)
{
    const ITEM *const lara_item = Lara_GetItem();
    if (lara_item == nullptr) {
        return false;
    }
    const LARA_INFO *const lara = Lara_GetLaraInfo();
    const bool is_blinking = g_Config.ui.enable_bar_flashing
        && lara->exposure_timer <= LARA_MAX_EXPOSURE * UI_BAR_BLINK_THRESHOLD;

    const bool show =
        g_Config.ui.show_bars && lara->exposure_timer < LARA_MAX_EXPOSURE;
    if (!show) {
        return false;
    }

    int32_t value = is_blinking && blink_state ? 0 : lara->exposure_timer;
    CLAMPL(value, 0);
    UI_Bar((UI_BAR_SETTINGS) {
        .type = UI_BAR_LARA_EXPOSURE,
        .w = UI_BAR_WIDTH,
        .h = UI_BAR_HEIGHT,
        .value = value,
        .max_value = LARA_MAX_EXPOSURE,
    });
    return true;
}
