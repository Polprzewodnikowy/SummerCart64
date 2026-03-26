#include <trx/game/lara/col.h>

#include <trx/config.h>
#include <trx/debug.h>
#include <trx/game/lara.h>
#include <trx/game/rooms.h>
#include <trx/game/spawn.h>

#define M_MONKEY_CEILING_SNAP 704
#define M_PUSH_TIMEOUT 15

static void (*m_CollisionRoutines[LS_NUMBER_OF])(
    ITEM *item, COLL_INFO *coll) = {};

static void M_Push(
    const COLL_ITEM *const item, COLL_INFO *const coll, const bool hit_on,
    const bool big_push)
{
    ITEM *const target_item = Lara_GetItem();
    int32_t dx = target_item->pos.x - item->pos.x;
    int32_t dz = target_item->pos.z - item->pos.z;
    const int32_t c = Math_Cos(item->rot.y);
    const int32_t s = Math_Sin(item->rot.y);
    int32_t rx = (c * dx - s * dz) >> W2V_SHIFT;
    int32_t rz = (c * dz + s * dx) >> W2V_SHIFT;

    const BOUNDS_16 *const bounds = &item->bounds;
    int32_t min_x = bounds->min.x;
    int32_t max_x = bounds->max.x;
    int32_t min_z = bounds->min.z;
    int32_t max_z = bounds->max.z;

    if (big_push) {
        max_x += coll->radius;
        min_z -= coll->radius;
        max_z += coll->radius;
        min_x -= coll->radius;
    }

    if (rx < min_x || rx > max_x || rz < min_z || rz > max_z) {
        return;
    }

    const int32_t l = rx - min_x;
    const int32_t r = max_x - rx;
    const int32_t t = max_z - rz;
    const int32_t b = rz - min_z;

    if (l <= r && l <= t && l <= b) {
        rx -= l;
    } else if (r <= l && r <= t && r <= b) {
        rx += r;
    } else if (t <= l && t <= r && t <= b) {
        rz += t;
    } else {
        rz = min_z;
    }

    target_item->pos.x = item->pos.x + ((rz * s + rx * c) >> W2V_SHIFT);
    target_item->pos.z = item->pos.z + ((rz * c - rx * s) >> W2V_SHIFT);

    rz = (bounds->max.z + bounds->min.z) / 2;
    rx = (bounds->max.x + bounds->min.x) / 2;
    dx -= (c * rx + s * rz) >> W2V_SHIFT;
    dz -= (c * rz - s * rx) >> W2V_SHIFT;

    if (hit_on && bounds->max.y - bounds->min.y > STEP_L) {
        Lara_TakeHit(target_item, dx, dz);
    }

    const int16_t old_facing = coll->facing;
    coll->bad_pos = NO_BAD_POS;
    coll->bad_neg = -STEPUP_HEIGHT;
    coll->bad_ceiling = 0;
    coll->facing = Math_Atan(
        target_item->pos.z - coll->old.z, target_item->pos.x - coll->old.x);
    Collide_GetCollisionInfo(
        coll, target_item->pos.x, target_item->pos.y, target_item->pos.z,
        target_item->room_num, LARA_HEIGHT);
    coll->facing = old_facing;

    if (coll->coll_type != COLL_NONE) {
        target_item->pos.x = coll->old.x;
        target_item->pos.z = coll->old.z;
    } else {
        coll->old = target_item->pos;
        Lara_UpdateRoomToHeight(-10);
    }

    LARA_INFO *const lara_info = Lara_GetLaraInfo();
    if (lara_info->interact_target.is_moving
        && lara_info->interact_target.move_count > M_PUSH_TIMEOUT) {
        lara_info->interact_target.is_moving = false;
        lara_info->gun_status = LGS_ARMLESS;
    }
}

void Lara_Col_Register(
    const LARA_TRX_STATE state,
    void (*const handle_func)(ITEM *item, COLL_INFO *coll))
{
    ASSERT(state >= 0 && state < LS_NUMBER_OF);
    m_CollisionRoutines[state] = handle_func;
}

void Lara_Col_Update(ITEM *const item, COLL_INFO *const coll)
{
    const LARA_TRX_STATE state = LS_U(item->current_anim_state);
    if (state >= 0 && state < LS_NUMBER_OF
        && m_CollisionRoutines[state] != nullptr) {
        m_CollisionRoutines[state](item, coll);
    }
}

void Lara_Col_GetInfo(const ITEM *const item, COLL_INFO *const coll)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    coll->facing = lara->move_angle;
    Collide_GetCollisionInfo(
        coll, item->pos.x, item->pos.y, item->pos.z, item->room_num,
        LARA_HEIGHT);
}

void Lara_Col_Shift(COLL_INFO *const coll)
{
    Collide_ShiftItem(Lara_GetItem(), coll);
}

void Lara_Col_MonkeySwingSnap(ITEM *const item)
{
    int16_t room_num = item->room_num;
    const SECTOR *const sector = Room_GetSector(item->pos, &room_num);
    const int32_t ceiling = Room_GetCeiling(sector, item->pos);
    if (ceiling != NO_HEIGHT) {
        item->pos.y = ceiling + M_MONKEY_CEILING_SNAP;
    }
}

void Lara_Col_ItemPush(
    const ITEM *const item, COLL_INFO *const coll, const bool hit_on,
    const bool big_push)
{
    const COLL_ITEM src_item = {
        .bounds = Item_GetBestFrame(item)->bounds,
        .pos = item->pos,
        .rot = item->rot,
    };
    M_Push(&src_item, coll, hit_on, big_push);
}

void Lara_Col_Static3DPush(const STATIC_MESH *const mesh, COLL_INFO *const coll)
{
    const COLL_ITEM src_item = {
        .bounds = Object_Get3DStatic(mesh->static_num)->collision_bounds,
        .pos = mesh->pos,
        .rot = { .y = mesh->rot.y },
    };
    M_Push(&src_item, coll, false, true);
}

void Lara_Col_WadeSplash(ITEM *const item)
{
    if (!g_Config.gameplay.enable_wading) {
        return;
    }

    const LARA_INFO *const lara = Lara_GetLaraInfo();
    if (lara->water_status == LWS_CHEAT) {
        return;
    }

    const int32_t water_depth = Lara_GetWaterDepth(
        item->pos.x, item->pos.y, item->pos.z, item->room_num);
    const int32_t water_height = Room_GetWaterHeight(item->pos, item->room_num);
    const BOUNDS_16 *const bounds = &Item_GetBestFrame(item)->bounds;
    if (water_height != NO_HEIGHT && water_depth != NO_HEIGHT
        && bounds != nullptr && item->pos.y + bounds->min.y <= water_height
        && item->pos.y + bounds->max.y >= water_height
        && water_depth < LARA_SWIM_DEPTH - STEP_L) {
        Spawn_Splash(item);
    }
}
