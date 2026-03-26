#include <trx/game/ui/elements/bar_enemy_hp.h>

#include <trx/config.h>
#include <trx/game/creature.h>
#include <trx/game/game.h>
#include <trx/game/lara/common.h>
#include <trx/game/lara/const.h>
#include <trx/game/objects/vars.h>
#include <trx/game/ui/elements/bar.h>

bool UI_EnemyHealthBar(void)
{
    const ITEM *const target = Lara_GetLaraInfo()->target;
    if (target == nullptr) {
        return false;
    }
    const bool is_ally = Creature_IsAlly(target);

    bool show = g_Config.ui.show_bars;
    switch (g_Config.ui.enemy_health_bar.show_mode) {
    case BAR_SHOW_MODE_NEVER:
        show &= false;
        break;
    case BAR_SHOW_MODE_ALWAYS:
        show &= true;
        break;
    case BAR_SHOW_MODE_BOSS_ONLY:
        show &= Object_IsType(target->object_id, g_BossObjects);
        break;
    }
    if (!show) {
        return false;
    }

    UI_Bar((UI_BAR_SETTINGS) {
        .type = is_ally ? UI_BAR_ALLY_HP : UI_BAR_ENEMY_HP,
        .w = UI_BAR_WIDTH,
        .h = UI_BAR_HEIGHT,
        .value = target->hit_points,
        .max_value =
            target->max_hit_points * (Game_IsBonusFlagSet(GBF_NGPLUS) ? 2 : 1),
    });
    return true;
}
