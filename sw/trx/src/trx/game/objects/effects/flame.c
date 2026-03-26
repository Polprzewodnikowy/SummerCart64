#include <trx/game/objects/effects/flame.h>

#include <trx/config.h>
#include <trx/core/utils.h>
#include <trx/game/effects.h>
#include <trx/game/lara.h>
#include <trx/game/output.h>
#include <trx/game/random.h>
#include <trx/game/rooms.h>
#include <trx/game/sound.h>
#include <trx/game/sparks.h>
#include <trx/version.h>

#define M_LIGHT_INTENSITY 11
#define M_LIGHT_FALLOFF 10
#define M_DAMAGE_PROXIMITY 600
#define M_IGNITE_PROXIMITY (g_TRVersion == 1 ? 300 : 450)
#define M_TOO_NEAR_DAMAGE (g_TRVersion == 1 ? 3 : 5)
#define M_ON_FIRE_DAMAGE (g_TRVersion == 1 ? 5 : 7)

static const uint8_t m_TR3_XZOffsets[16][2] = {
    { 9, 9 },   { 24, 9 },  { 40, 9 },  { 55, 9 },  { 9, 24 },  { 24, 24 },
    { 40, 24 }, { 55, 24 }, { 9, 40 },  { 24, 40 }, { 40, 40 }, { 55, 40 },
    { 9, 55 },  { 24, 55 }, { 40, 55 }, { 55, 55 },
};

static void M_TR3_SideFlameDetection(
    const EFFECT *const effect, const int32_t length)
{
    ITEM *const lara_item = Lara_GetItem();
    const int32_t dx = lara_item->pos.x - effect->pos.x;
    const int32_t dz = lara_item->pos.z - effect->pos.z;

    const int32_t max_dist = 20 * WALL_L;
    if (dx < -max_dist || dx > max_dist || dz < -max_dist || dz > max_dist) {
        return;
    }

    const int32_t half_w = STEP_L;
    const int32_t offset = WALL_L / 2;
    int32_t origin_x = effect->pos.x;
    int32_t origin_z = effect->pos.z;
    int32_t xs = 0, xe = 0, zs = 0, ze = 0;

    switch (effect->rot.y) {
    case 0:
        origin_z += offset;
        xs = -half_w;
        xe = half_w;
        zs = -length;
        ze = 0;
        break;

    case DEG_90:
        origin_x += offset;
        xs = -length;
        xe = 0;
        zs = -half_w;
        ze = half_w;
        break;

    case -DEG_90:
        origin_x -= offset;
        xs = 0;
        xe = length;
        zs = -half_w;
        ze = half_w;
        break;

    case -DEG_180:
        origin_z -= offset;
        xs = -half_w;
        xe = half_w;
        zs = 0;
        ze = length;
        break;

    default:
        return;
    }

    const int32_t min_x = origin_x + xs;
    const int32_t max_x = origin_x + xe;
    const int32_t min_z = origin_z + zs;
    const int32_t max_z = origin_z + ze;

    const BOUNDS_16 *const b = Item_GetBoundsAccurate(lara_item);
    const int32_t lara_min_y = lara_item->pos.y + b->min.y;
    const int32_t lara_max_y = lara_item->pos.y + b->max.y;
    const int32_t fire_min_y = effect->pos.y - 384;
    const int32_t fire_max_y = effect->pos.y + 128;

    if (lara_item->pos.x < min_x || lara_item->pos.x > max_x
        || lara_item->pos.z < min_z || lara_item->pos.z > max_z
        || lara_min_y > fire_max_y || lara_max_y < fire_min_y) {
        return;
    }

    if (effect->flag1 >= 18) {
        Lara_CatchFire();
    } else {
        Lara_TakeDamage(M_TOO_NEAR_DAMAGE, true);
    }
}

static inline XYZ_32 M_OffsetPos(
    const XYZ_32 base_pos, const int32_t dx, const int32_t dy, const int32_t dz)
{
    return (XYZ_32) {
        .x = base_pos.x + dx,
        .y = base_pos.y + dy,
        .z = base_pos.z + dz,
    };
}

static void M_TR3_ControlBig(
    EFFECT *const effect, const int32_t time4, const int32_t rnd)
{
    if ((time4 & 0xC) == 0) {
        Sparks_TriggerFireFlame(effect->pos, -1, 0);
        Sparks_TriggerFireSmoke(effect->pos, -1, 0);
    }
    Sparks_TriggerStaticFlame(effect->pos, (Random_GetControl() & 0xF) + 96);
}

static void M_TR3_ControlSmall(
    EFFECT *const effect, const int32_t time4, const int32_t rnd)
{
    if (effect->counter >= 0) {
        const int32_t angle = ((effect->rot.y >> 4) & 4095) << 1;
        const int32_t s = (288 * Math_Sin(angle << 3)) >> W2V_SHIFT;
        const int32_t c = (288 * Math_Cos(angle << 3)) >> W2V_SHIFT;

        Sparks_TriggerStaticFlame(
            M_OffsetPos(effect->pos, s, -192, c),
            (Random_GetControl() & 15) + 32);

        if ((time4 & 0x18) != 0) {
            return;
        }

        Sparks_TriggerFireFlame(M_OffsetPos(effect->pos, s, -224, c), -1, 1);

        if ((time4 & 0x18) == 0) {
            Sparks_TriggerFireSmoke(M_OffsetPos(effect->pos, s, 0, c), -1, 1);
        }
        return;
    }

    const ITEM *const lara_item = Lara_GetItem();
    LARA_INFO *const lara = Lara_GetLaraInfo();

    // Lara is on fire; attach flames to joints and apply damage unless
    // water extinguishes it.
    if (lara->water_status == LWS_CHEAT) {
        effect->counter = 0;
        Effect_Kill(Effect_GetIndex(effect));
        lara->burn = 0;
        return;
    }

    for (int i = 0; i < LM_NUMBER_OF; i++) {
        if ((time4 & 0xC) == 0) {
            effect->pos.x = 0;
            effect->pos.y = 0;
            effect->pos.z = 0;
            Collide_GetJointAbsPosition(lara_item, &effect->pos, i);
#if 0
            // TODO: implement this for Willard
            Sparks_TriggerFireFlame(effect->pos, -1, 255 - lara->burn_green);
#else
            Sparks_TriggerFireFlame(effect->pos, -1, 255);
#endif
        }
    }

    RGB_888 color;
#if 0
    // TODO: implement this for Willard
    if (lara->burn_green) {
        color.r = (rnd >> 2) & 0x3F;
        color.g = (rnd & 0x3F) + 192;
        color.b = ((rnd >> 4) & 0x1F) + 96;
    } else
#endif
    {
        color.r = (rnd & 0x3F) + 192;
        color.g = ((rnd >> 4) & 0x1F) + 96;
        color.b = 0;
    }

    Output_AddDynamicLightRGB(lara_item->pos, 13, color);

    if (lara_item->room_num != effect->room_num) {
        Effect_UpdateRoom(Effect_GetIndex(effect), lara_item->room_num);
    }

    const int32_t wh = Room_GetWaterHeight(effect->pos, effect->room_num);
    if (wh == NO_HEIGHT || effect->pos.y <= wh
        || (Room_Get(effect->room_num)->flags.swamp
            && (GF_BadGetLevelNum() == 4 || GF_BadGetLevelNum() == 18
                || GF_BadGetLevelNum() == 19))) {
        Sound_Effect(SFX_LOOP_FOR_SMALL_FIRES, &effect->pos, SPM_NORMAL);
        Lara_TakeDamage(M_ON_FIRE_DAMAGE, true);
    } else {
        effect->counter = 0;
        Effect_Kill(Effect_GetIndex(effect));
        lara->burn = false;
    }
}

static void M_TR3_ControlJet(
    EFFECT *const effect, const int32_t time4, const int32_t rnd)
{
    // Jets cycle between two spawn positions, chosen from a 16-cell grid.
    if (effect->flag1 != 0) {
        effect->flag1--;
    } else {
        effect->flag1 = (Random_GetControl() & 3) + 8;
        int32_t new_index = Random_GetControl() & 0x3F;
        if (effect->flag2 == new_index) {
            new_index = (new_index + 13) & 0x3F;
        }
        effect->flag2 = (int16_t)new_index;
    }

    const int32_t idx0 = effect->flag2 & 7;
    const int32_t idx1 = (effect->flag2 >> 3) + 8;
    const int32_t x0 = (m_TR3_XZOffsets[idx0][0] << 4) - 512;
    const int32_t z0 = (m_TR3_XZOffsets[idx0][1] << 4) - 512;
    const int32_t x1 = (m_TR3_XZOffsets[idx1][0] << 4) - 512;
    const int32_t z1 = (m_TR3_XZOffsets[idx1][1] << 4) - 512;
    if ((time4 & 4) == 0) {
        Sparks_TriggerFireFlame(M_OffsetPos(effect->pos, x0, 0, z0), -1, 2);
    } else {
        Sparks_TriggerFireFlame(M_OffsetPos(effect->pos, x1, 0, z1), -1, 2);
    }
}

static void M_TR3_ControlSide(
    EFFECT *const effect, const int32_t time4, const int32_t rnd)
{
    // Side flames: alternate between direction and length phases.
    const int32_t dist = (rnd & 0xFF) + 512;
    const int32_t angle = (effect->rot.y >> 3) & 0x1FFE;
    const int32_t s = (dist * Math_Sin(angle << 3)) >> W2V_SHIFT;
    const int32_t c = (dist * Math_Cos(angle << 3)) >> W2V_SHIFT;

    if (effect->flag2 != 0) {
        if ((time4 & 4) != 0) {
            Sparks_TriggerSideFlame(
                M_OffsetPos(effect->pos, s, 0, c),
                ((angle - 4096) & 0x1FFF) << 3,
                (!(Random_GetControl() & 7)) ? 1 : 0, 1);
        }

        effect->flag2--;
    } else {
        if (effect->flag1 != 0) {
            if ((time4 & 4) != 0) {
                int32_t size = 9;
                if (effect->flag1 > 112) {
                    size = (129 - effect->flag1) >> 1;
                } else if (effect->flag1 < 18) {
                    size = (effect->flag1 >> 1) + 1;
                }

                Sparks_TriggerSideFlame(
                    M_OffsetPos(effect->pos, s, 0, c),
                    ((angle + 4096) & 0x1FFE) << 3, size, 0);
            }

            effect->flag1 -= 2;
        } else {
            effect->flag1 = 128;

            // TODO: do not hardcode this
            if (GF_BadGetLevelNum() == 7) {
                effect->flag2 = 120;
            } else {
                effect->flag2 = 60;
            }
        }
    }
}

static void M_TR3_Control(const int16_t effect_num)
{
    EFFECT *const effect = Effect_Get(effect_num);
    const int32_t rnd = Random_GetControl();

    const int32_t time4 = Output_GetTimeInGame() * 4;

    // Animation & particle logic.
    if (effect->frame_num == FLAME_BIG) {
        M_TR3_ControlBig(effect, time4, rnd);
    } else if (effect->frame_num == FLAME_SMALL) {
        M_TR3_ControlSmall(effect, time4, rnd);
    } else if (effect->frame_num == FLAME_JET) {
        M_TR3_ControlJet(effect, time4, rnd);
    } else {
        M_TR3_ControlSide(effect, time4, rnd);
    }

    // Light & proximity damage logic (applies to most flame types).
    int32_t x = effect->pos.x + ((rnd & 0xF) << 5);
    int32_t y = effect->pos.y + ((rnd & 0xF0) << 1);
    int32_t z = effect->pos.z + ((rnd >> 3) & 0x1E0);
    RGB_888 color = {
        .r = (rnd & 0x3F) + 192,
        .g = ((rnd >> 4) & 0x1F) + 96,
        .b = 0,
    };

    int32_t dist = 0;
    if (effect->frame_num == FLAME_SIDE) {
        if (effect->flag2 != 0) {
            dist = 0;
        } else if (effect->flag1 < 18) {
            dist = 2048;
        } else if (effect->flag1 < 64) {
            dist = 2048;
        } else {
            dist = (128 - effect->flag1) << 5;
        }

        const int32_t angle = (((effect->rot.y >> 3) & 0xFFFE) - 4096) & 0x1FFE;
        const int32_t s = (dist * Math_Sin(angle << 3)) >> W2V_SHIFT;
        const int32_t c = (dist * Math_Cos(angle << 3)) >> W2V_SHIFT;
        Output_AddDynamicLightRGB(
            (XYZ_32) { x + s, y, z + c }, (effect->flag2 != 0) ? 6 : 13, color);
    } else {
        Output_AddDynamicLightRGB(
            (XYZ_32) { x, y, z }, 16 - (effect->frame_num << 2), color);
    }

    Sound_Effect(SFX_LOOP_FOR_SMALL_FIRES, &effect->pos, SPM_NORMAL);

    const ITEM *const lara_item = Lara_GetItem();
    LARA_INFO *const lara = Lara_GetLaraInfo();
    if (effect->counter != 0) {
        effect->counter--;
    } else if (effect->frame_num == FLAME_SIDE) {
        if (!lara->burn && dist) {
            M_TR3_SideFlameDetection(effect, dist);
        }
    } else if (effect->frame_num != FLAME_SMALL) {
        if (Lara_IsNearItem(&effect->pos, M_DAMAGE_PROXIMITY)) {
            x = lara_item->pos.x - effect->pos.x;
            z = lara_item->pos.z - effect->pos.z;
            dist = SQUARE(x) + SQUARE(z);
            Lara_TakeDamage(M_TOO_NEAR_DAMAGE, true);

            if (dist < 202500) {
                effect->counter = 100;
                Lara_CatchFire();
            }
        }
    }
}

static void M_TR12_DoEffects(const EFFECT *const effect)
{
    if (!Object_Get(O_FLAME)->loaded) {
        return;
    }

    Sound_Effect(SFX_LOOP_FOR_SMALL_FIRES, &effect->pos, SPM_NORMAL);
    if (!g_Config.visuals.enable_fire_lighting) {
        return;
    }

    const int32_t random = Random_GetControl();
    const XYZ_32 light_pos = {
        .x = effect->pos.x + (random & 0x140) - 0xA0,
        .y = effect->pos.y - STEP_L - (random & 0x50),
        .z = effect->pos.z + (random & 0x140) - 0xA0,
    };

    if (random > 0x4000) {
        Output_AddDynamicLight(light_pos, M_LIGHT_INTENSITY, M_LIGHT_FALLOFF);
    } else if (random > 0x2000) {
        Output_AddDynamicLight(
            light_pos, M_LIGHT_INTENSITY - (random & 2), M_LIGHT_FALLOFF);
    } else {
        Output_AddDynamicLight(
            light_pos, M_LIGHT_INTENSITY, M_LIGHT_FALLOFF / 2);
    }
}

static void M_TR12_Control(const int16_t effect_num)
{
    EFFECT *const effect = Effect_Get(effect_num);
    const ITEM *const lara_item = Lara_GetItem();
    LARA_INFO *const lara_info = Lara_GetLaraInfo();

    effect->frame_num--;
    if (effect->frame_num <= Object_Get(O_FLAME)->mesh_count) {
        effect->frame_num = 0;
    }

    if (effect->counter >= 0) {
        M_TR12_DoEffects(effect);
        if (effect->counter != 0) {
            effect->counter--;
        } else if (Lara_IsNearItem(&effect->pos, M_DAMAGE_PROXIMITY)) {
            Lara_TakeDamage(M_TOO_NEAR_DAMAGE, true);
            const int32_t dx = lara_item->pos.x - effect->pos.x;
            const int32_t dz = lara_item->pos.z - effect->pos.z;
            const int32_t dist = SQUARE(dx) + SQUARE(dz);
            if (dist < SQUARE(M_IGNITE_PROXIMITY)) {
                effect->counter = 100;
                Lara_CatchFire();
            }
        }
    } else {
        effect->pos.x = 0;
        effect->pos.y = 0;
        if (effect->counter == -1) {
            effect->pos.z = -100;
        } else {
            effect->pos.z = 0;
        }

        Collide_GetJointAbsPosition(
            lara_item, &effect->pos, -1 - effect->counter);
        const int16_t room_num = lara_item->room_num;
        if (room_num != effect->room_num) {
            Effect_UpdateRoom(effect_num, room_num);
        }

        const int32_t water_height =
            Room_GetWaterHeight(effect->pos, effect->room_num);
        if ((water_height != NO_HEIGHT && effect->pos.y > water_height)
            || lara_info->water_status == LWS_CHEAT) {
            effect->counter = 0;
            Effect_Kill(Effect_GetIndex(effect));
            lara_info->burn = false;
        } else {
            M_TR12_DoEffects(effect);
            Lara_TakeDamage(M_ON_FIRE_DAMAGE, false);
            lara_info->burn = true;
        }
    }
}

static void M_Setup(OBJECT *const obj)
{
    obj->control_func = g_TRVersion == 3 ? M_TR3_Control : M_TR12_Control;
    obj->semi_transparent = true;
}

REGISTER_OBJECT(O_FLAME, M_Setup)
