#include <trx/game/ui/elements/bar_lara_air.h>

#include <trx/config.h>
#include <trx/game/lara.h>
#include <trx/game/rooms.h>
#include <trx/game/ui/elements/bar.h>

bool UI_LaraAirBar(const bool blink_state)
{
    const ITEM *const lara_item = Lara_GetItem();
    if (lara_item == nullptr) {
        return false;
    }
    const LARA_INFO *const lara = Lara_GetLaraInfo();
    const bool is_blinking = g_Config.ui.enable_bar_flashing
        && lara->air <= LARA_MAX_AIR * UI_BAR_BLINK_THRESHOLD;
    const ROOM *const room = Room_Get(lara_item->room_num);
    const bool show = g_Config.ui.show_bars
        && (lara->water_status == LWS_UNDERWATER
            || lara->water_status == LWS_SURFACE
            || (room->flags.swamp && lara->air < LARA_MAX_AIR)
            || (lara->water_status == LWS_ABOVE_WATER
                && Lara_Vehicle_IsOnType(O_UPV)));
    if (!show) {
        return false;
    }

    UI_Bar((UI_BAR_SETTINGS) {
        .type = UI_BAR_LARA_AIR,
        .w = UI_BAR_WIDTH,
        .h = UI_BAR_HEIGHT,
        .value = is_blinking && blink_state ? 0 : lara->air,
        .max_value = LARA_MAX_AIR,
    });
    return true;
}
