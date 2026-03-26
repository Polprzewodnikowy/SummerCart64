#include <trx/game/lara/breath.h>

#include <trx/core/utils.h>
#include <trx/game/collision.h>
#include <trx/game/lara.h>
#include <trx/game/level/settings.h>
#include <trx/game/output/state.h>
#include <trx/game/random.h>
#include <trx/game/rooms.h>
#include <trx/game/sparks.h>
#include <trx/version.h>

static bool M_CanBreatheVisible(const ITEM *const lara_item)
{
    if (lara_item == nullptr) {
        return false;
    }

    if (g_TRVersion != 3) {
        return false;
    }

    if (!Level_HasColdWater()) {
        return false;
    }

    const LARA_INFO *const lara = Lara_GetLaraInfo();
    if (lara->extra_anim) {
        return false;
    }

    if (lara->water_status == LWS_CHEAT || lara->water_status == LWS_UNDERWATER
        || lara_item->hit_points < 0) {
        return false;
    }

    if (lara_item->current_anim_state == LS(LS_STOP)) {
        return Item_GetRelativeFrame(lara_item) >= 30;
    }

    if (lara_item->current_anim_state == LS(LS_CROUCH_IDLE)) {
        return Item_GetRelativeFrame(lara_item) >= 30;
    }

    const int32_t time = (int32_t)Output_GetTimeInGame() % 64;
    return time >= 32 && time <= 48;
}

void Lara_Breath_Control(const ITEM *const lara_item)
{
    if (!M_CanBreatheVisible(lara_item)) {
        return;
    }

    XYZ_32 pos = { .x = 0, .y = -4, .z = 64 };
    Collide_GetJointAbsPosition(lara_item, &pos, LM_HEAD);

    XYZ_32 end = {
        .x = (Random_GetControl() & 7) - 4,
        .y = (Random_GetControl() & 7) - 8,
        .z = (Random_GetControl() & 0x7F) + 64,
    };
    Collide_GetJointAbsPosition(lara_item, &end, LM_HEAD);

    const XYZ_32 vel = {
        .x = end.x - pos.x,
        .y = end.y - pos.y,
        .z = end.z - pos.z,
    };

    Sparks_TriggerBreath(pos, vel, lara_item->room_num);
}
