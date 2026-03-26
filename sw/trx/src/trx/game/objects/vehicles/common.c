#include <trx/config.h>
#include <trx/core/utils.h>
#include <trx/game/camera.h>
#include <trx/game/collision.h>
#include <trx/game/cutscene.h>
#include <trx/game/game.h>
#include <trx/game/game_flow.h>
#include <trx/game/game_strings/table.h>
#include <trx/game/lara.h>
#include <trx/game/level.h>
#include <trx/game/music.h>
#include <trx/game/objects/vars.h>
#include <trx/game/output.h>
#include <trx/game/viewport.h>

typedef enum {
    LA_VEHICLE_HIT_LEFT = 11,
    LA_VEHICLE_HIT_RIGHT = 12,
    LA_VEHICLE_HIT_FRONT = 13,
    LA_VEHICLE_HIT_BACK = 14,
} LARA_ANIM_VEHICLE;

int32_t Vehicle_DoShift(
    ITEM *const vehicle, const XYZ_32 *const pos, const XYZ_32 *const old)
{
    int32_t x = pos->x >> WALL_SHIFT;
    int32_t z = pos->z >> WALL_SHIFT;
    const int32_t old_x = old->x >> WALL_SHIFT;
    const int32_t old_z = old->z >> WALL_SHIFT;
    const int32_t shift_x = pos->x & (WALL_L - 1);
    const int32_t shift_z = pos->z & (WALL_L - 1);

    if (x == old_x) {
        if (z == old_z) {
            vehicle->pos.x += old->x - pos->x;
            vehicle->pos.z += old->z - pos->z;
        } else if (z > old_z) {
            vehicle->pos.z -= shift_z + 1;
            return pos->x - vehicle->pos.x;
        } else {
            vehicle->pos.z += WALL_L - shift_z;
            return vehicle->pos.x - pos->x;
        }
    } else if (z == old_z) {
        if (x > old_x) {
            vehicle->pos.x -= shift_x + 1;
            return vehicle->pos.z - pos->z;
        } else {
            vehicle->pos.x += WALL_L - shift_x;
            return pos->z - vehicle->pos.z;
        }
    } else {
        int16_t room_num;
        const SECTOR *sector;
        int32_t height;

        x = 0;
        z = 0;

        XYZ_32 test_pos = (XYZ_32) { old->x, pos->y, pos->z };
        room_num = vehicle->room_num;
        sector = Room_GetSector(test_pos, &room_num);
        height = Room_GetHeight(sector, test_pos);
        if (height < old->y - STEP_L) {
            if (pos->z > old->z) {
                z = -shift_z - 1;
            } else {
                z = WALL_L - shift_z;
            }
        }

        test_pos = (XYZ_32) { pos->x, pos->y, old->z };
        room_num = vehicle->room_num;
        sector = Room_GetSector(test_pos, &room_num);
        height = Room_GetHeight(sector, test_pos);
        if (height < old->y - STEP_L) {
            if (pos->x > old->x) {
                x = -shift_x - 1;
            } else {
                x = WALL_L - shift_x;
            }
        }

        if (x != 0 && z != 0) {
            vehicle->pos.x += x;
            vehicle->pos.z += z;
        } else if (z != 0) {
            vehicle->pos.z += z;
            if (z > 0) {
                return vehicle->pos.x - pos->x;
            } else {
                return pos->x - vehicle->pos.x;
            }
        } else if (x != 0) {
            vehicle->pos.x += x;
            if (x > 0) {
                return pos->z - vehicle->pos.z;
            } else {
                return vehicle->pos.z - pos->z;
            }
        } else {
            vehicle->pos.x += old->x - pos->x;
            vehicle->pos.z += old->z - pos->z;
        }
    }

    return 0;
}

int32_t Vehicle_GetCollisionAnim(const ITEM *const vehicle, XYZ_32 *const moved)
{
    moved->x = vehicle->pos.x - moved->x;
    moved->z = vehicle->pos.z - moved->z;

    if (moved->x != 0 || moved->z != 0) {
        const int32_t c = Math_Cos(vehicle->rot.y);
        const int32_t s = Math_Sin(vehicle->rot.y);
        const int32_t front = (moved->x * s + moved->z * c) >> W2V_SHIFT;
        const int32_t side = (moved->x * c - moved->z * s) >> W2V_SHIFT;
        if (ABS(front) > ABS(side)) {
            if (front > 0) {
                return LA_VEHICLE_HIT_BACK;
            } else {
                return LA_VEHICLE_HIT_FRONT;
            }
        } else {
            if (side > 0) {
                return LA_VEHICLE_HIT_LEFT;
            } else {
                return LA_VEHICLE_HIT_RIGHT;
            }
        }
    }

    return 0;
}
