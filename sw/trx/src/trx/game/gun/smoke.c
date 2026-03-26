#include <trx/game/gun/smoke.h>

#include <trx/game/gun/vars.h>
#include <trx/game/lara.h>
#include <trx/game/lara/common.h>
#include <trx/game/lara/misc.h>
#include <trx/game/rooms.h>
#include <trx/game/sparks.h>
#include <trx/version.h>

static XYZ_32 M_GetHandAbsPosition(const LARA_MESH hand, XYZ_32 offset)
{
    Lara_GetMeshPos(hand, &offset);
    return offset;
}

static XYZ_32 M_GetMuzzleOffset(
    const LARA_GUN_TYPE weapon_type, const bool is_right_hand)
{
    return is_right_hand ? g_Weapons[weapon_type].muzzle_pos
                         : g_Weapons[weapon_type].muzzle_pos_alt;
}

void Gun_Smoke_OnFire(const LARA_GUN_TYPE weapon_type, const bool is_right_hand)
{
    if (g_TRVersion != 3) {
        return;
    }

    LARA_INFO *const lara = Lara_GetLaraInfo();
    const int32_t count = g_Weapons[weapon_type].smoke_count;
    if (count == 0) {
        return;
    }

    lara->tr3_smoke_weapon = weapon_type;
    if (is_right_hand) {
        lara->tr3_smoke_count_r = count;
    } else {
        lara->tr3_smoke_count_l = count;
    }

    const LARA_MESH hand = is_right_hand ? LM_HAND_R : LM_HAND_L;
    const XYZ_32 muzzle_pos = weapon_type == LGT_SHOTGUN && is_right_hand
        ? M_GetHandAbsPosition(hand, (XYZ_32) { .x = 0, .y = 228, .z = 32 })
        : M_GetHandAbsPosition(
              hand, M_GetMuzzleOffset(weapon_type, is_right_hand));

    GAME_VECTOR pos = { .pos = muzzle_pos,
                        .room_num = Lara_GetItem()->room_num };
    Room_GetSector(pos.pos, &pos.room_num);

    if (weapon_type == LGT_SHOTGUN && is_right_hand) {
        const XYZ_32 muzzle_tip_pos =
            M_GetHandAbsPosition(hand, (XYZ_32) { .x = 0, .y = 1508, .z = 32 });
        const XYZ_32 vel = {
            .x = muzzle_tip_pos.x - muzzle_pos.x,
            .y = muzzle_tip_pos.y - muzzle_pos.y,
            .z = muzzle_tip_pos.z - muzzle_pos.z,
        };

        for (int32_t i = 0; i < 7; i++) {
            Sparks_TriggerGunSmokeDirected(pos, vel, true, weapon_type, count);
        }

        const XYZ_32 vel_sparks = {
            .x = (muzzle_tip_pos.x - muzzle_pos.x) << 1,
            .y = (muzzle_tip_pos.y - muzzle_pos.y) << 1,
            .z = (muzzle_tip_pos.z - muzzle_pos.z) << 1,
        };

        for (int32_t i = 0; i < 12; i++) {
            Sparks_TriggerShotgunSparks(muzzle_pos, vel_sparks);
        }
    } else {
        Sparks_TriggerGunSmoke(pos, true, weapon_type, count);
    }
}

void Gun_Smoke_Control(void)
{
    if (g_TRVersion != 3) {
        return;
    }

    LARA_INFO *const lara = Lara_GetLaraInfo();
    if (lara->tr3_smoke_count_l == 0 && lara->tr3_smoke_count_r == 0) {
        return;
    }

    const LARA_GUN_TYPE weapon_type = lara->tr3_smoke_weapon;

    if (lara->tr3_smoke_count_l > 0) {
        const XYZ_32 muzzle_pos = M_GetHandAbsPosition(
            LM_HAND_L, M_GetMuzzleOffset(weapon_type, false));
        GAME_VECTOR pos = { .pos = muzzle_pos,
                            .room_num = Lara_GetItem()->room_num };
        Room_GetSector(pos.pos, &pos.room_num);
        Sparks_TriggerGunSmoke(
            pos, false, weapon_type, lara->tr3_smoke_count_l);
        lara->tr3_smoke_count_l--;
    }

    if (lara->tr3_smoke_count_r > 0) {
        const XYZ_32 muzzle_pos = M_GetHandAbsPosition(
            LM_HAND_R, M_GetMuzzleOffset(weapon_type, true));
        GAME_VECTOR pos = { .pos = muzzle_pos,
                            .room_num = Lara_GetItem()->room_num };
        Room_GetSector(pos.pos, &pos.room_num);
        Sparks_TriggerGunSmoke(
            pos, false, weapon_type, lara->tr3_smoke_count_r);
        lara->tr3_smoke_count_r--;
    }
}
