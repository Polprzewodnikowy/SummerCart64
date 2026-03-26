#include "sophia_internal.h"

#include <trx/core/math/func.h>
#include <trx/game/items/manager.h>
#include <trx/game/lara.h>
#include <trx/game/matrix.h>
#include <trx/game/objects.h>
#include <trx/game/output.h>
#include <trx/game/output/sources/poly_fx.h>
#include <trx/game/random.h>
#include <trx/game/sound.h>
#include <trx/game/sparks.h>

#define M_SPEED 384

typedef struct {
    int16_t light_falloff;
    int16_t light_phase;
    bool summon_bolt;
    int16_t summon_lifetime;
} M_PRIV;

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;

    const XYZ_32 old_pos = item->pos;
    const int16_t old_room_num = item->room_num;

    const int32_t speed = (item->speed * Math_Cos(item->rot.x)) >> W2V_SHIFT;
    item->pos = XYZ_32_OffsetYaw(item->pos, item->rot.y, speed);
    item->pos.y -= (item->speed * Math_Sin(item->rot.x)) >> W2V_SHIFT;

    if (item->speed < M_SPEED) {
        item->speed += (item->speed >> 3) + 2;
    }

    if (p->summon_bolt && item->speed > 192) {
        p->summon_lifetime++;

        if (p->summon_lifetime >= 16) {
            Item_Kill(item_num);
            return;
        }
    }

    int16_t room_num = item->room_num;
    const SECTOR *const sector = Room_GetSector(item->pos, &room_num);
    if (item->room_num != room_num) {
        Item_UpdateRoom(item_num, room_num);
    }

    if (!p->summon_bolt) {
        const bool hit = Lara_IsNearItem(&item->pos, 400);
        item->floor = Room_GetHeight(sector, item->pos);
        const int32_t c = Room_GetCeiling(sector, item->pos);

        if (hit || item->pos.y >= item->floor || item->pos.y <= c) {
            Sound_Effect(SFX_EXPLOSION_1, &item->pos, SPM_NORMAL);
            int32_t extras = (p->light_falloff >= 0) + 2;

            Sparks_TriggerExplosionSparks(
                old_pos, extras, -2, 2, item->room_num);
            for (int32_t i = 0; i < extras; i++) {
                Sparks_TriggerExplosionSparks(
                    old_pos, 2, -1, 2, item->room_num);
            }

            extras++;

            for (int32_t i = 0; i < extras; i++) {
                Sophia_TriggerPlasmaBall(1, old_pos, old_room_num, item->rot.y);
            }

            if (hit) {
                Lara_TakeDamage(30 + ((p->light_falloff >= 0) << 9), true);
            } else {
                const ITEM *const lara_item = Lara_GetItem();
                const XYZ_32 delta = {
                    .x = lara_item->pos.x - item->pos.x,
                    .y = lara_item->pos.y - item->pos.y - 256,
                    .z = lara_item->pos.z - item->pos.z,
                };
                const int32_t dist = XYZ_32_GetLength(delta);
                if (dist < WALL_L) {
                    Lara_TakeDamage(
                        (WALL_L - dist) >> (6 - 2 * (p->light_falloff >= 0)),
                        true);
                }
            }

            Item_Kill(item_num);
            return;
        }
    }

    int32_t g = 255 - (Random_GetControl() & 0x3F);
    int32_t b = g >> 1;
    int32_t falloff;

    if (p->light_falloff < 0) {
        if (p->summon_bolt) {
            falloff = 16 - p->summon_lifetime;
            g = (falloff * g) >> 4;
            b = (falloff * b) >> 4;
        }

        falloff = -p->light_falloff;

        if (falloff > 10) {
            p->light_phase += 2;
            p->light_falloff++;
        }
    } else {
        falloff = p->light_falloff;

        if (falloff > 16) {
            p->light_phase += 4;
            p->light_falloff--;
        }
    }

    Output_AddDynamicLightRGB(item->pos, falloff, (RGB_888) { 0, g, b });
}

static bool M_Draw(const ITEM *const item)
{
    const M_PRIV *const p = item->priv;
    const XYZ_32 origin = item->interp.result.pos;
    const XYZ_16 rot = item->interp.result.rot;

    const int32_t beam_speed = (item->speed * Math_Cos(rot.x)) >> W2V_SHIFT;
    const XYZ_32 dir = {
        .x = (beam_speed * Math_Sin(rot.y)) >> W2V_SHIFT,
        .y = -((item->speed * Math_Sin(rot.x)) >> W2V_SHIFT),
        .z = (beam_speed * Math_Cos(rot.y)) >> W2V_SHIFT,
    };

    Matrix_PushUnit();
    Matrix_Rot16(rot);

    const int32_t radius =
        p->summon_bolt ? p->light_phase >> 1 : p->light_phase;
    XYZ_32 base[4] = {};
    for (int32_t i = 0; i < 4; i++) {
        const XYZ_32 local = {
            .x = (i == 0 || i == 3) ? -radius : radius,
            .y = (i < 2) ? -radius : radius,
            .z = 0,
        };
        base[i] = Matrix_MulVec32_M(g_WMatrixPtr, local);
    }

    Matrix_Pop();

    const int32_t section_count = p->summon_bolt ? 3 : 6;
    XYZ_32 sections[6][4] = {};
    uint8_t intensities[6] = {};

    for (int32_t i = 0; i < 4; i++) {
        sections[0][i] = origin;
    }
    intensities[0] = 128;

    const int32_t ring_count = section_count - 2;
    for (int32_t i = 0; i < ring_count; i++) {
        const int32_t step = i + 2;
        const XYZ_32 center = {
            .x = origin.x - dir.x * step,
            .y = origin.y - dir.y * step,
            .z = origin.z - dir.z * step,
        };
        const int32_t scale = 4 - i;
        for (int32_t j = 0; j < 4; j++) {
            sections[i + 1][j] = (XYZ_32) {
                .x = center.x + (base[j].x * scale) / 4,
                .y = center.y + (base[j].y * scale) / 4,
                .z = center.z + (base[j].z * scale) / 4,
            };
        }
        intensities[i + 1] = 64 - (i << 4);
    }

    const int32_t end_step = p->summon_bolt ? 5 : 6;
    const XYZ_32 end = {
        .x = origin.x - dir.x * end_step,
        .y = origin.y - dir.y * end_step,
        .z = origin.z - dir.z * end_step,
    };
    for (int32_t i = 0; i < 4; i++) {
        sections[section_count - 1][i] = end;
    }

    const int32_t fade = p->summon_bolt ? 16 - p->summon_lifetime : 16;
    for (int32_t i = 0; i < section_count - 1; i++) {
        int32_t c0 = intensities[i];
        int32_t c1 = intensities[i + 1];
        if (p->summon_bolt) {
            c0 = (c0 * fade) >> 4;
            c1 = (c1 * fade) >> 4;
        }

        const RGBA_8888 color0 = { c0 >> 2, c0, c0 >> 1, 0xC0 };
        const RGBA_8888 color1 = { c1 >> 2, c1, c1 >> 1, 0xC0 };
        for (int32_t j = 0; j < 4; j++) {
            const int32_t next = (j + 1) & 3;
            const XYZ_32 world_pos[4] = {
                sections[i][j],
                sections[i + 1][next],
                sections[i + 1][j],
                sections[i][next],
            };
            const RGBA_8888 color[4] = { color0, color0, color1, color1 };
            OutputSource_PolyFX_StageQuadExt(
                -1, world_pos, nullptr, color,
                VERT_FLAT_SHADED | VERT_NO_LIGHTING | VERT_NO_WIBBLE,
                DRAW_BLEND_ADD);

            const XYZ_32 world_pos_rev[4] = {
                world_pos[3],
                world_pos[2],
                world_pos[1],
                world_pos[0],
            };
            OutputSource_PolyFX_StageQuadExt(
                -1, world_pos_rev, nullptr, color,
                VERT_FLAT_SHADED | VERT_NO_LIGHTING | VERT_NO_WIBBLE,
                DRAW_BLEND_ADD);
        }
    }

    return true;
}

static void M_Setup(OBJECT *const obj)
{
    obj->control_func = M_Control;
    obj->draw_func = M_Draw;
    obj->priv_size = sizeof(M_PRIV);
}

void Sophia_TriggerLaserBolt(
    const XYZ_32 pos, const ITEM *const item, const int32_t type,
    const int16_t angle)
{
    const int16_t item_num = Item_Create();
    if (item_num == NO_ITEM) {
        return;
    }

    ITEM *const bolt = Item_Get(item_num);
    bolt->object_id = O_SOPHIA_LASER_BOLT;
    bolt->room_num = item->room_num;
    bolt->pos = pos;
    Item_Initialise(item_num);

    M_PRIV *const bolt_priv = bolt->priv;
    if (type == 2) {
        bolt->pos.y += item->pos.y - 384;
        bolt->rot.x = -pos.y << 5;
        bolt->rot.y = Random_GetControl() << 1;
    } else {
        const ITEM *const lara_item = Lara_GetItem();
        int16_t angles[2];
        Math_GetVectorAngles(
            lara_item->pos.x - pos.x, lara_item->pos.y - pos.y - 256,
            lara_item->pos.z - pos.z, angles);
        bolt->rot.x = angles[1];
        bolt->rot.y = angle;
        bolt->rot.z = 0;
    }

    if (type == 1) {
        bolt->speed = 24;
        bolt_priv->light_falloff = 31;
        bolt_priv->light_phase = 16;
    } else {
        bolt->speed = 16;
        bolt_priv->light_falloff = -24;
        bolt_priv->light_phase = 4;

        if (type == 2) {
            bolt_priv->summon_bolt = true;
        }
    }

    Item_AddActive(item_num);
    bolt->status = IS_ACTIVE;
}

REGISTER_OBJECT(O_SOPHIA_LASER_BOLT, M_Setup)
