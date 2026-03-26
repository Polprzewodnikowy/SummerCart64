#include <trx/game/objects/general/flare_item.h>

#include <trx/core/utils.h>
#include <trx/game/gun.h>
#include <trx/game/items/anim.h>
#include <trx/game/matrix.h>
#include <trx/game/objects.h>
#include <trx/game/objects/general/pickup.h>
#include <trx/game/output.h>
#include <trx/game/random.h>
#include <trx/game/sound.h>
#include <trx/game/sparks.h>
#include <trx/game/spawn.h>
#include <trx/version.h>

// clang-format off
#define M_FLARE_INTENSITY 12
#define M_FLARE_FALL_OFF  11

#define M_MAX_FLARE_AGE_TR12   (60 * LOGIC_FPS)                       // = 1800
#define M_FLARE_OLD_AGE_TR12   (M_MAX_FLARE_AGE_TR12 - 2 * LOGIC_FPS) // = 1740
#define M_FLARE_YOUNG_AGE_TR12 (LOGIC_FPS)                            // = 30

#define M_MAX_FLARE_AGE_TR3   (30 * LOGIC_FPS)         // = 900
#define M_FLARE_DYING_AGE_TR3 (M_MAX_FLARE_AGE_TR3 - 90) // = 810
#define M_FLARE_END_AGE_TR3   (M_MAX_FLARE_AGE_TR3 - 24) // = 876
// clang-format on

typedef struct {
    int32_t raw_age;
} M_PRIV;

static XYZ_32 M_TransformLocalOffset(
    const XYZ_32 pos, const XYZ_16 rot, const XYZ_32 local_offset)
{
    Matrix_PushUnit();
    Matrix_TranslateAbs32(pos);
    Matrix_Rot16(rot);
    Matrix_TranslateRel32(local_offset);
    const XYZ_32 out = {
        .x = g_WMatrixPtr->_03 >> W2V_SHIFT,
        .y = g_WMatrixPtr->_13 >> W2V_SHIFT,
        .z = g_WMatrixPtr->_23 >> W2V_SHIFT,
    };
    Matrix_Pop();
    return out;
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    const ROOM *const room = Room_Get(item->room_num);
    if (room->flags.swamp) {
        Item_Kill(item_num);
        return;
    }

    if (item->fall_speed != 0) {
        item->rot.x += DEG_1 * 3;
        item->rot.z += DEG_1 * 5;
    } else {
        item->rot.x = 0;
        item->rot.z = 0;
    }

    const XYZ_32 old_pos = item->pos;
    item->pos = XYZ_32_OffsetYaw(item->pos, item->rot.y, item->speed);

    if (room->flags.underwater) {
        item->fall_speed += (5 - item->fall_speed) / 2;
        item->speed = item->speed + (5 - item->speed) / 2;
    } else {
        item->fall_speed += GRAVITY;
    }
    item->pos.y += item->fall_speed;

    Collide_DoProperDetection(item, old_pos);

    int32_t flare_age = FlareItem_GetAge(item);
    bool is_active = FlareItem_IsActive(item);
    if (flare_age < Flare_GetMaxAge()) {
        flare_age++;
    } else if (item->fall_speed == 0 && item->speed == 0) {
        Item_Kill(item_num);
        return;
    }

    if (Flare_GenerateLight(item->pos, flare_age)) {
        is_active = true;
        Flare_GenerateEffects(&item->pos, item->pos, item->room_num);
    }

    if (g_TRVersion >= 3) {
        if (flare_age < Flare_GetMaxAge() && is_active) {
            const BOUNDS_16 *const bounds = &Item_GetBestFrame(item)->bounds;
            const XYZ_32 flare_size = {
                .x = bounds->max.x - bounds->min.x,
                .y = bounds->max.y - bounds->min.y,
                .z = bounds->max.z - bounds->min.z,
            };
            const XYZ_32 flare_offset = {
                .x = -flare_size.x,
                .y = -flare_size.y,
                .z = -flare_size.z,
            };

            const XYZ_32 flare_pos = item->pos;
            const XYZ_32 tip_local = {
                .x = flare_offset.x - 6,
                .y = flare_offset.y + 6,
                .z = flare_offset.z + 32,
            };
            const XYZ_32 tip_pos =
                M_TransformLocalOffset(flare_pos, item->rot, tip_local);

            const XYZ_32 vel_local = {
                .x = (Random_GetControl() & 0x7F) - 64,
                .y = (Random_GetControl() & 0x7F) - 64,
                .z = (Random_GetControl() & 0x1FF) + 512,
            };
            const XYZ_32 vel_pos =
                M_TransformLocalOffset(flare_pos, item->rot, vel_local);
            const XYZ_32 vel = {
                .x = vel_pos.x - flare_pos.x,
                .y = vel_pos.y - flare_pos.y,
                .z = vel_pos.z - flare_pos.z,
            };

            for (int32_t i = 0; i < (Random_GetControl() & 3) + 4; i++) {
                const bool smoke = (i >> 2) != 0;
                Sparks_TriggerFlareSparks(tip_pos, vel, smoke);
            }
        }
    }

    FlareItem_SetAge(item, flare_age, is_active);
}

static void M_DrawFlash(const CLIP clip)
{
    WEAPON_INFO *const flare_info = &g_Weapons[LGT_FLARE];
    SWAP(flare_info->flash_pos, flare_info->flash_pos_alt);
    Gun_DrawFlash(LGT_FLARE, clip, false);
    SWAP(flare_info->flash_pos, flare_info->flash_pos_alt);
}

static bool M_Draw(const ITEM *const item)
{
    int32_t rate;
    ANIM_FRAME *frames[2];
    Item_GetFrames(item, frames, &rate);
    Matrix_Push();
    Matrix_TranslateAbs32(item->interp.result.pos);
    Matrix_Rot16(item->interp.result.rot);
    const CLIP clip = Output_CheckBoundsClip(&frames[0]->bounds);

    const XYZ_32 flare_size = {
        .x = frames[0]->bounds.max.x - frames[0]->bounds.min.x,
        .y = frames[0]->bounds.max.y - frames[0]->bounds.min.y,
        .z = frames[0]->bounds.max.z - frames[0]->bounds.min.z,
    };
    const XYZ_32 flare_offset = {
        .x = -flare_size.x,
        .y = -flare_size.y,
        .z = -flare_size.z,
    };
    Matrix_TranslateRel32(flare_offset);

    if (clip == CLIP_NOT_VISIBLE) {
        goto end;
    }

    Output_CalculateObjectLighting(item, &frames[0]->bounds);
    Object_DrawMesh(Object_Get(O_FLARE_ITEM)->mesh_idx, clip, false);
    if (!FlareItem_IsActive(item)) {
        goto end;
    }

    if (g_TRVersion >= 3) {
        goto end;
    }

    M_DrawFlash(clip);

end:
    Matrix_Pop();
    return true;
}

static bool M_GenerateLight_TR12(const XYZ_32 pos, const int32_t flare_age)
{
    if (flare_age >= M_MAX_FLARE_AGE_TR12) {
        return false;
    }

    const int32_t random = Random_GetDraw();
    const XYZ_32 light_pos = {
        .x = pos.x + (random & 0xA0),
        .y = pos.y,
        .z = pos.z,
    };

    if (flare_age < M_FLARE_YOUNG_AGE_TR12) {
        const int32_t intensity = M_FLARE_INTENSITY
                * (flare_age - M_FLARE_YOUNG_AGE_TR12)
                / (2 * M_FLARE_YOUNG_AGE_TR12)
            + M_FLARE_INTENSITY;
        Output_AddDynamicLight(light_pos, intensity, M_FLARE_FALL_OFF);
        return true;
    }

    if (flare_age < M_FLARE_OLD_AGE_TR12) {
        Output_AddDynamicLight(light_pos, M_FLARE_INTENSITY, M_FLARE_FALL_OFF);
        return true;
    }

    if (random > 0x2000) {
        Output_AddDynamicLight(
            light_pos, M_FLARE_INTENSITY - (random & 3), M_FLARE_FALL_OFF);
        return true;
    }

    Output_AddDynamicLight(light_pos, M_FLARE_INTENSITY, M_FLARE_FALL_OFF / 2);
    return false;
}

static bool M_GenerateLight_TR3(const XYZ_32 pos, const int32_t flare_age)
{
    if (flare_age >= M_MAX_FLARE_AGE_TR3) {
        return false;
    }

    const int32_t rnd = Random_GetControl();
    const XYZ_32 light_pos = {
        .x = pos.x + ((rnd & 0xF) << 3),
        .y = pos.y + ((rnd >> 1) & 0x78),
        .z = pos.z + ((rnd >> 5) & 0x78),
    };

    int32_t r = 0;
    int32_t g = 0;
    int32_t b = 0;
    int32_t falloff = 0;

    if (flare_age < 4) {
        r = (rnd & 0x1F) + (flare_age << 4) + 160;
        g = ((rnd >> 4) & 0x1F) + (flare_age << 3) + 32;
        b = ((rnd >> 8) & 0x1F) + (flare_age << 4);
        falloff = (rnd & 3) + (flare_age << 2) + 4;

        if (falloff > 16) {
            falloff -= (rnd >> 12) & 3;
        }
    } else if (flare_age < 16) {
        r = (rnd & 0x3F) + (flare_age << 2) + 128;
        g = ((rnd >> 4) & 0x1F) + (flare_age << 2) + 64;
        b = ((rnd >> 8) & 0x1F) + (flare_age << 2) + 16;
        falloff = (rnd & 1) + flare_age + 2;
    } else if (flare_age < M_FLARE_DYING_AGE_TR3) {
        r = (rnd & 0x3F) + 192;
        g = ((rnd >> 4) & 0x1F) + 128;
        b = ((rnd >> 8) & 0x20) + (((rnd >> 6) & 0x10) << 1);
        falloff = 16;
    } else if (flare_age < M_FLARE_END_AGE_TR3) {
        if (rnd > 0x2000) {
            r = (rnd & 0x3F) + 192;
            g = ((rnd >> 4) & 0x1F) + 64;
            b = ((rnd >> 8) & 0x20) + (((rnd >> 6) & 0x10) << 1);
            falloff = 16;
        } else {
            const int32_t rnd2 = Random_GetControl();
            const int32_t rnd3 = Random_GetControl();
            const int32_t rnd4 = Random_GetControl();
            r = (rnd2 & 0x3F) + 192;
            g = (rnd3 & 0x3F) + 64;
            b = rnd4 & 0x7F;
            falloff = (Random_GetControl() & 6) + 8;
            Output_AddDynamicLightRGB(
                light_pos, falloff, (RGB_888) { r, g, b });
            return false;
        }
    } else {
        const int32_t rnd2 = Random_GetControl();
        const int32_t rnd3 = Random_GetControl();
        const int32_t rnd4 = Random_GetControl();
        r = (rnd2 & 0x3F) + 192;
        g = (rnd3 & 0x3F) + 64;
        b = rnd4 & 0x1F;
        falloff = 16 - ((flare_age - M_FLARE_END_AGE_TR3) >> 1);
        Output_AddDynamicLightRGB(light_pos, falloff, (RGB_888) { r, g, b });
        return (rnd & 1) != 0;
    }

    Output_AddDynamicLightRGB(light_pos, falloff, (RGB_888) { r, g, b });
    return true;
}

void Flare_GenerateEffects(
    const XYZ_32 *const sound_pos, const XYZ_32 flare_pos, int16_t room_num)
{
    Room_GetSector(flare_pos, &room_num);
    if (Room_Get(room_num)->flags.underwater) {
        Sound_Effect(SFX_LARA_FLARE_BURN, sound_pos, SPM_UNDERWATER);
        if (Random_GetDraw() < 0x4000) {
            Spawn_Bubble(&flare_pos, room_num);
        }
    } else {
        Sound_Effect(SFX_LARA_FLARE_BURN, sound_pos, SPM_NORMAL);
    }
}

bool Flare_GenerateLight(const XYZ_32 pos, const int32_t flare_age)
{
    if (g_TRVersion >= 3) {
        return M_GenerateLight_TR3(pos, flare_age);
    } else {
        return M_GenerateLight_TR12(pos, flare_age);
    }
}

int32_t Flare_GetMaxAge(void)
{
    return g_TRVersion >= 3 ? M_MAX_FLARE_AGE_TR3 : M_MAX_FLARE_AGE_TR12;
}

int32_t FlareItem_GetAge(const ITEM *const item)
{
    const M_PRIV *const p = item->priv;
    return p->raw_age & 0x7FFF;
}

bool FlareItem_IsActive(const ITEM *const item)
{
    const M_PRIV *const p = item->priv;
    return (p->raw_age & 0x8000) != 0;
}

void FlareItem_SetAge(
    ITEM *const item, const int32_t flare_age, const bool is_active)
{
    M_PRIV *const p = item->priv;
    p->raw_age = flare_age & 0x7FFF;
    if (is_active) {
        p->raw_age |= 0x8000;
    }
}

static void M_Setup(OBJECT *const obj)
{
    obj->collision_func = Pickup_Collision;
    obj->bounds_func = Pickup_Bounds;
    obj->control_func = M_Control;
    obj->draw_func = M_Draw;
    obj->priv_size = sizeof(M_PRIV);
    obj->save_position = true;
    obj->save_flags = true;

    if (obj->loaded) {
        for (int32_t i = 0; i < obj->mesh_count; i++) {
            OBJECT_MESH *const obj_mesh = Object_GetMesh(obj->mesh_idx + i);
            obj_mesh->depth_adjustment = -0.5;
        }
    }
}

REGISTER_OBJECT(O_FLARE_ITEM, M_Setup)
