#include <trx/game/fx/laser.h>

#include <trx/core/utils.h>
#include <trx/game/const.h>
#include <trx/game/los.h>
#include <trx/game/output/sources/poly_fx.h>

#define M_MAX_LASERS 32
#define M_LIFETIME 2

typedef struct {
    bool active;
    int16_t owner_item_num;
    int16_t lifetime;
    BITE bite;
    RGBA_8888 color;
    float width;
} M_LASER;

typedef struct {
    M_LASER lasers[M_MAX_LASERS];
    int32_t next_idx;
} M_PRIV;

static M_PRIV m_Priv = {};

static bool M_GetOwnerItem(const M_LASER *const laser, ITEM **const out_item)
{
    if (laser->owner_item_num < 0
        || laser->owner_item_num >= Item_GetTotalCount()) {
        return false;
    }
    *out_item = Item_Get(laser->owner_item_num);
    return *out_item != nullptr;
}

bool FX_Laser_Spawn(const ITEM *const owner_item, const CREATURE_GUN *const gun)
{
    if (owner_item == nullptr || gun == nullptr || !gun->tr3_enemy_flash) {
        return false;
    }

    M_LASER *const laser = &m_Priv.lasers[m_Priv.next_idx];
    laser->active = true;
    laser->lifetime = M_LIFETIME;
    laser->owner_item_num = Item_GetIndex(owner_item);
    laser->bite = gun->tr3_laser.bite;
    laser->color = gun->tr3_laser.color;
    laser->width = gun->tr3_laser.width;

    m_Priv.next_idx = (m_Priv.next_idx + 1) % M_MAX_LASERS;
    return true;
}

void FX_Laser_Control(void)
{
    for (int32_t i = 0; i < M_MAX_LASERS; i++) {
        M_LASER *const laser = &m_Priv.lasers[i];
        if (!laser->active) {
            continue;
        }

        ITEM *owner_item = nullptr;
        if (!M_GetOwnerItem(laser, &owner_item)) {
            laser->active = false;
            continue;
        }

        laser->lifetime--;
        if (laser->lifetime <= 0) {
            laser->active = false;
        }
    }
}

void FX_Laser_Draw(void)
{
    for (int32_t i = 0; i < M_MAX_LASERS; i++) {
        const M_LASER *const laser = &m_Priv.lasers[i];
        if (!laser->active) {
            continue;
        }

        ITEM *owner_item = nullptr;
        if (!M_GetOwnerItem(laser, &owner_item)) {
            continue;
        }

        GAME_VECTOR start = {
            .pos = laser->bite.pos,
            .room_num = owner_item->room_num,
        };
        GAME_VECTOR target = start;
        target.y *= 32;
        Collide_GetJointAbsPosition(
            owner_item, &start.pos, laser->bite.mesh_num);
        Collide_GetJointAbsPosition(
            owner_item, &target.pos, laser->bite.mesh_num);

        LOS_Check(&start, &target, false);

        const int32_t dist = XYZ_32_GetDistance(start.pos, target.pos);
        int32_t segment_count = 2 * dist / WALL_L;
        CLAMP(segment_count, 8, 32);

        const XYZ_32 delta = {
            .x = target.x - start.x,
            .y = target.y - start.y,
            .z = target.z - start.z,
        };

        for (int32_t j = 0; j < segment_count; j++) {
            const float seg_start = (float)j / segment_count;
            const float seg_end = (float)(j + 1) / segment_count;

            const XYZ_32 p1 = {
                .x = start.x + delta.x * seg_start,
                .y = start.y + delta.y * seg_start,
                .z = start.z + delta.z * seg_start,
            };
            const XYZ_32 p2 = {
                .x = start.x + delta.x * seg_end,
                .y = start.y + delta.y * seg_end,
                .z = start.z + delta.z * seg_end,
            };
            OutputSource_PolyFX_StageLineSegment(
                p1, laser->color, p2, laser->color, laser->width,
                DRAW_BLEND_ADD);
        }
    }
}
