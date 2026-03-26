#include <trx/config.h>
#include <trx/game/lara.h>
#include <trx/game/random.h>
#include <trx/game/sound.h>
#include <trx/game/spawn.h>
#include <trx/version.h>

#define M_FALL_SPEED_LIMIT (g_TRVersion == 1 ? 0 : GRAVITY)
#define M_DAMAGE 15

static void M_Collision(
    const int16_t item_num, ITEM *const lara_item, COLL_INFO *const coll)
{
    ITEM *const item = Item_Get(item_num);
    if (lara_item->hit_points < 0) {
        return;
    }

    if (!Lara_TestBoundsCollide(item, coll->radius)) {
        return;
    }
    if (!Collide_TestCollision(item, lara_item)) {
        return;
    }

    int32_t blood_spawn_count = Random_GetControl() / 0x6000;
    if (lara_item->gravity) {
        if (lara_item->fall_speed > M_FALL_SPEED_LIMIT
            && !g_Config.debug.enable_invulnerability) {
            lara_item->hit_points = -1;
            blood_spawn_count = 20;
        }
    } else if (lara_item->speed < 30) {
        return;
    }

    lara_item->hit_points -= M_DAMAGE;
    for (int32_t i = 0; i < blood_spawn_count; i++) {
        const XYZ_32 pos = {
            .x = lara_item->pos.x + (Random_GetControl() - 0x4000) / 256,
            .z = lara_item->pos.z + (Random_GetControl() - 0x4000) / 256,
            .y = lara_item->pos.y - Random_GetControl() / 64,
        };
        Spawn_Blood(
            pos.x, pos.y, pos.z, 20, Random_GetControl(), item->room_num);
    }

    if (lara_item->hit_points <= 0) {
        Item_SwitchToAnim(lara_item, LA(LA_SPIKE_DEATH), 0);
        lara_item->current_anim_state = LS(LS_DEATH);
        lara_item->goal_anim_state = LS(LS_DEATH);
        lara_item->pos.y = item->pos.y;
        lara_item->gravity = false;
    }
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    if (!Item_IsTriggerActive(item)) {
        return;
    }

    const int32_t level_num = GF_BadGetLevelNum();
    if (level_num != 5 && level_num != 7) {
        return;
    }

    if (Item_GetRelativeFrame(item) == 0) {
        if (level_num == 5) {
            Sound_Effect(SFX_SHIVA_SWORD_2, &item->pos, SPM_ALWAYS);
        } else {
            Sound_Effect(SFX_LARA_GET_OUT, &item->pos, SPM_ALWAYS);
        }
    }

    Item_Animate(item);
}

static void M_Setup(OBJECT *const obj)
{
    obj->collision_func = M_Collision;
    if (g_TRVersion == 3) {
        obj->control_func = M_Control;
    }
}

REGISTER_OBJECT(O_SPIKES, M_Setup)
