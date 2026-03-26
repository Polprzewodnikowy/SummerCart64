#include <trx/game/collision/common.h>

#include <trx/config.h>
#include <trx/core/utils.h>
#include <trx/game/interpolation.h>
#include <trx/game/items.h>
#include <trx/game/items/anim.h>
#include <trx/game/lara/common.h>
#include <trx/game/matrix.h>
#include <trx/game/rooms.h>
#include <trx/version.h>

#define M_HEADROOM 160 // Additional collision space above Lara's head.

static bool M_IsOnWalkable(
    const SECTOR *const sector, const XYZ_32 pos, const int32_t room_height)
{
    return g_Config.gameplay.fix_bridge_collision
        && Room_IsOnWalkable(sector, pos, room_height, NO_ITEM);
}

// Probes the front, left, and right of Lara and fills in the collision info for
// each side. The collision info depends on Lara's state. Her state determines
// how big slope and lava pit sectors are treated. For example, in the walk
// state, Lara won't walk up big slopes or walk down into lava pits.
static void M_FillSide(
    const COLL_INFO *const coll, COLL_SIDE *const side, const XYZ_32 pos,
    const XZ_32 probe, const int32_t obj_height, int16_t *const room_num)
{
    const int32_t y = pos.y - obj_height;
    const int32_t y_top = y - M_HEADROOM;

    int16_t local_room_num = *room_num;
    int16_t *const test_room_num =
        g_Config.gameplay.wall_glitch_mode == WALL_GLITCH_FIXED
        ? &local_room_num
        : room_num;

    const XYZ_32 sample_pos = {
        .x = pos.x + probe.x,
        .y = y_top,
        .z = pos.z + probe.z,
    };
    const SECTOR *sector = Room_GetSector(sample_pos, test_room_num);
    int32_t height = Room_GetHeight(sector, sample_pos);
    int32_t ceiling = Room_GetCeiling(sector, sample_pos);
    const int32_t room_height = height;
    const int32_t room_ceiling = ceiling;
    const bool sim_wall = room_height == ceiling && room_height != NO_HEIGHT
        && sector->ceiling.tilt == 0 && sector->floor.tilt == 0;
    if (height != NO_HEIGHT) {
        height -= pos.y;
    }
    if (ceiling != NO_HEIGHT) {
        ceiling -= y;
    }

    side->floor = height;
    side->ceiling = ceiling;
    side->type = Room_GetHeightType();

    const bool is_on_walkable = M_IsOnWalkable(sector, sample_pos, room_height);

    const bool is_front = side == &coll->side_front;
    if (is_front) {
        XYZ_32 front_probe_pos = sample_pos;
        front_probe_pos.x += probe.x;
        front_probe_pos.z += probe.z;
        sector = Room_GetSector(front_probe_pos, room_num);
        height = Room_GetHeight(sector, front_probe_pos);
        if (height != NO_HEIGHT) {
            height -= pos.y;
        }
    }

    if (!is_on_walkable) {
        if (coll->slopes_are_walls
            && (side->type == HT_BIG_SLOPE || side->type == HT_DIAGONAL)
            && side->floor < 0
            && (!is_front
                || (side->floor < coll->side_mid.floor
                    && height < side->floor))) {
            side->floor = -32767;
        } else if (
            coll->slopes_are_pits
            && (side->type == HT_BIG_SLOPE || side->type == HT_DIAGONAL)
            && side->floor > (is_front ? coll->side_mid.floor : 0)) {
            side->floor = STEP_L * 2;
        } else if (
            coll->lava_is_pit && side->floor > 0
            && Room_GetPitSector(sector, pos.x, pos.z)->is_death_sector) {
            side->floor = STEP_L * 2;
        }
    } else if (sim_wall) {
        side->floor = NO_HEIGHT;
        side->ceiling = NO_HEIGHT;
    }
}

int32_t Collide_GetSpheres(
    const ITEM *const item, SPHERE *const spheres, const bool world_space)
{
    if (item == nullptr) {
        return 0;
    }

    XYZ_32 pos;
    if (world_space) {
        pos = item->pos;
        Matrix_PushUnit();
    } else {
        pos.x = 0;
        pos.y = 0;
        pos.z = 0;
        Matrix_Push();
        Matrix_TranslateAbs32(item->pos);
    }

    Matrix_Rot16(item->rot);

    const ANIM_FRAME *const frame = Item_GetBestFrame(item);
    Matrix_TranslateRel16(frame->offset);
    Matrix_Rot16(frame->mesh_rots[0]);

    const OBJECT *const obj = Object_Get(item->object_id);
    const OBJECT_MESH *mesh = Object_GetMesh(obj->mesh_idx);
    Matrix_Push();
    Matrix_TranslateRel16(mesh->center);
    spheres[0].pos.x = pos.x + (g_MatrixPtr->_03 >> W2V_SHIFT);
    spheres[0].pos.y = pos.y + (g_MatrixPtr->_13 >> W2V_SHIFT);
    spheres[0].pos.z = pos.z + (g_MatrixPtr->_23 >> W2V_SHIFT);
    spheres[0].r = mesh->radius;
    Matrix_Pop();

    const int16_t *extra_rotation = item->extra_rotations;
    for (int32_t i = 1; i < obj->mesh_count; i++) {
        const ANIM_BONE *const bone = Object_GetBone(obj, i - 1);
        if (bone->matrix_pop) {
            Matrix_Pop();
        }
        if (bone->matrix_push) {
            Matrix_Push();
        }

        Matrix_TranslateRel32(bone->pos);
        Matrix_Rot16(frame->mesh_rots[i]);
        Object_ApplyExtraRotation(&extra_rotation, bone->rot, false);

        mesh = Object_GetMesh(obj->mesh_idx + i);
        Matrix_Push();
        Matrix_TranslateRel16(mesh->center);
        SPHERE *const sphere = &spheres[i];
        sphere->pos.x = pos.x + (g_MatrixPtr->_03 >> W2V_SHIFT);
        sphere->pos.y = pos.y + (g_MatrixPtr->_13 >> W2V_SHIFT);
        sphere->pos.z = pos.z + (g_MatrixPtr->_23 >> W2V_SHIFT);
        sphere->r = mesh->radius;
        Matrix_Pop();
    }

    Matrix_Pop();
    return obj->mesh_count;
}

int32_t Collide_TestCollision(ITEM *const item, const ITEM *const lara_item)
{
    SPHERE slist_baddie[34];
    SPHERE slist_lara[34];

    uint32_t touch_bits = 0;
    int32_t num1 = Collide_GetSpheres(item, slist_baddie, true);
    int32_t num2 = Collide_GetSpheres(lara_item, slist_lara, true);

    for (int32_t i = 0; i < num1; i++) {
        const SPHERE *const ptr1 = &slist_baddie[i];
        if (ptr1->r <= 0) {
            continue;
        }

        for (int32_t j = 0; j < num2; j++) {
            const SPHERE *const ptr2 = &slist_lara[j];
            if (ptr2->r <= 0) {
                continue;
            }

            const int32_t dx = ptr2->pos.x - ptr1->pos.x;
            const int32_t dy = ptr2->pos.y - ptr1->pos.y;
            const int32_t dz = ptr2->pos.z - ptr1->pos.z;
            const int32_t d1 = SQUARE(dx) + SQUARE(dy) + SQUARE(dz);
            const int32_t d2 = SQUARE(ptr1->r + ptr2->r);
            if (d1 < d2) {
                touch_bits |= 1 << i;
                break;
            }
        }
    }

    item->touch_bits = touch_bits;
    return touch_bits;
}

void Collide_GetJointAbsPosition(
    const ITEM *const item, XYZ_32 *const out_vec, const int32_t joint)
{
    const OBJECT *const obj = Object_Get(item->object_id);
    ANIM_FRAME *frames[2] = { nullptr, nullptr };
    int32_t rate = 0;
    const int32_t frac = Item_GetFrames(item, frames, &rate);
    const bool use_item_interp =
        Interpolation_IsActive() && item->enable_interpolation;
    const XYZ_32 item_pos =
        use_item_interp ? item->interp.result.pos : item->pos;
    const XYZ_16 item_rot =
        use_item_interp ? item->interp.result.rot : item->rot;

    if (frames[0] == nullptr) {
        Matrix_PushUnit();
        Matrix_Rot16(item_rot);
        Matrix_TranslateRel32(*out_vec);
        out_vec->x = item_pos.x + (g_MatrixPtr->_03 >> W2V_SHIFT);
        out_vec->y = item_pos.y + (g_MatrixPtr->_13 >> W2V_SHIFT);
        out_vec->z = item_pos.z + (g_MatrixPtr->_23 >> W2V_SHIFT);
        Matrix_Pop();
        return;
    }

    const ANIM_FRAME *const frame_a = frames[0];
    const ANIM_FRAME *const frame_b = frames[1];
    const bool do_interp = frame_b != nullptr && frac != 0 && rate != 0;

    int32_t stack = 1;
    Matrix_PushUnit();
    Matrix_Rot16(item_rot);
    if (do_interp) {
        Matrix_InitInterpolate(frac, rate);
        Matrix_TranslateRel16_ID(frame_a->offset, frame_b->offset);
        Matrix_Rot16_ID(frame_a->mesh_rots[0], frame_b->mesh_rots[0]);
    } else {
        Matrix_TranslateRel16(frame_a->offset);
        Matrix_Rot16(frame_a->mesh_rots[0]);
    }

    const int16_t *extra_rotation = item->extra_rotations;
    const int32_t max_joint = obj->mesh_count > 0 ? obj->mesh_count - 1 : 0;
    const int32_t abs_joint = MIN(max_joint, joint);
    for (int32_t i = 0; i < abs_joint; i++) {
        const ANIM_BONE *const bone = Object_GetBone(obj, i);
        if (bone->matrix_pop) {
            stack--;
            if (do_interp) {
                Matrix_Pop_I();
            } else {
                Matrix_Pop();
            }
        }
        if (bone->matrix_push) {
            stack++;
            if (do_interp) {
                Matrix_Push_I();
            } else {
                Matrix_Push();
            }
        }

        if (do_interp) {
            Matrix_TranslateRel32_I(bone->pos);
            Matrix_Rot16_ID(
                frame_a->mesh_rots[i + 1], frame_b->mesh_rots[i + 1]);
            Object_ApplyExtraRotation(&extra_rotation, bone->rot, true);
        } else {
            Matrix_TranslateRel32(bone->pos);
            Matrix_Rot16(frame_a->mesh_rots[i + 1]);
            Object_ApplyExtraRotation(&extra_rotation, bone->rot, false);
        }
    }

    if (do_interp) {
        Matrix_TranslateRel32_I(*out_vec);
        Matrix_Interpolate();
    } else {
        Matrix_TranslateRel32(*out_vec);
    }
    out_vec->x = item_pos.x + (g_MatrixPtr->_03 >> W2V_SHIFT);
    out_vec->y = item_pos.y + (g_MatrixPtr->_13 >> W2V_SHIFT);
    out_vec->z = item_pos.z + (g_MatrixPtr->_23 >> W2V_SHIFT);

    while (stack--) {
        if (do_interp) {
            Matrix_Pop_I();
        } else {
            Matrix_Pop();
        }
    }
}

void Collide_GetCollisionInfo(
    COLL_INFO *const coll, const int32_t x_pos, const int32_t y_pos,
    const int32_t z_pos, int16_t room_num, int32_t obj_height)
{
    coll->coll_type = COLL_NONE;
    coll->shift.x = 0;
    coll->shift.y = 0;
    coll->shift.z = 0;
    coll->quadrant = Math_GetDirection(coll->facing);

    bool reset_room = false;
    int16_t prev_room_num = room_num;
    if (obj_height < 0) {
        reset_room = true;
        obj_height = -obj_height;
    }

    int32_t x = x_pos;
    int32_t z = z_pos;
    const int32_t y = y_pos - obj_height;
    const int32_t y_top = y - M_HEADROOM;

    const XYZ_32 sample_pos = { .x = x, .y = y_top, .z = z };
    const SECTOR *sector = Room_GetSector(sample_pos, &room_num);
    int32_t height = Room_GetHeight(sector, sample_pos);
    int32_t room_height = height;
    if (height != NO_HEIGHT) {
        height -= y_pos;
    }
    int32_t ceiling = Room_GetCeiling(sector, sample_pos);
    if (ceiling != NO_HEIGHT) {
        ceiling -= y;
    }

    coll->side_mid.floor = height;
    coll->side_mid.ceiling = ceiling;
    coll->side_mid.type = Room_GetHeightType();

    bool is_on_walkable = M_IsOnWalkable(sector, sample_pos, room_height);
    if (is_on_walkable) {
        coll->tilt_z = 0;
        coll->tilt_x = 0;
    } else {
        const ITEM *const lara_item = Lara_GetItem();
        const int16_t tilt =
            Room_GetTiltType(sector, (XYZ_32) { x, lara_item->pos.y, z });
        coll->tilt_z = tilt >> 8;
        coll->tilt_x = (int8_t)tilt;
    }

    XZ_32 probe_left = {};
    XZ_32 probe_right = {};
    XZ_32 probe_front = {};
    switch (coll->quadrant) {
    case DIR_NORTH:
        probe_front.x = (coll->radius * Math_Sin(coll->facing)) >> W2V_SHIFT;
        probe_front.z = coll->radius;
        probe_left.x = -coll->radius;
        probe_left.z = coll->radius;
        probe_right.x = coll->radius;
        probe_right.z = coll->radius;
        break;

    case DIR_EAST:
        probe_front.x = coll->radius;
        probe_front.z = (coll->radius * Math_Cos(coll->facing)) >> W2V_SHIFT;
        probe_left.x = coll->radius;
        probe_left.z = coll->radius;
        probe_right.x = coll->radius;
        probe_right.z = -coll->radius;
        break;

    case DIR_SOUTH:
        probe_front.x = (coll->radius * Math_Sin(coll->facing)) >> W2V_SHIFT;
        probe_front.z = -coll->radius;
        probe_left.x = coll->radius;
        probe_left.z = -coll->radius;
        probe_right.x = -coll->radius;
        probe_right.z = -coll->radius;
        break;

    case DIR_WEST:
        probe_front.x = -coll->radius;
        probe_front.z = (coll->radius * Math_Cos(coll->facing)) >> W2V_SHIFT;
        probe_left.x = -coll->radius;
        probe_left.z = -coll->radius;
        probe_right.x = -coll->radius;
        probe_right.z = coll->radius;
        break;

    default:
        break;
    }

    if (reset_room) {
        room_num = prev_room_num;
    }

    const XYZ_32 probe_base = { x_pos, y_pos, z_pos };
    M_FillSide(
        coll, &coll->side_front, probe_base, probe_front, obj_height,
        &room_num);

    int16_t room_num2;
    room_num2 = prev_room_num;
    M_FillSide(
        coll, &coll->side_left, probe_base, probe_left, obj_height, &room_num2);
    room_num2 = prev_room_num;
    M_FillSide(
        coll, &coll->side_right, probe_base, probe_right, obj_height,
        &room_num2);

    M_FillSide(
        coll, &coll->side_left2, probe_base, probe_left, obj_height, &room_num);
    M_FillSide(
        coll, &coll->side_right2, probe_base, probe_right, obj_height,
        &room_num);

    const int16_t static_room_num = g_TRVersion >= 3 ? prev_room_num : room_num;
    if (Collide_CollideStaticObjects(
            coll, x_pos, y_pos, z_pos, static_room_num, obj_height)) {
        const XYZ_32 test_pos = {
            .x = x_pos + coll->shift.x,
            .y = y_pos,
            .z = z_pos + coll->shift.z,
        };
        sector = Room_GetSector(test_pos, &room_num);
        if (Room_GetHeight(sector, test_pos) < test_pos.y - WALL_L / 2
            || Room_GetCeiling(sector, test_pos) > y) {
            coll->shift.x = -coll->shift.x;
            coll->shift.z = -coll->shift.z;
        }
    }

    if (coll->side_mid.floor == NO_HEIGHT) {
        coll->shift.x = coll->old.x - x_pos;
        coll->shift.y = coll->old.y - y_pos;
        coll->shift.z = coll->old.z - z_pos;
        coll->coll_type = COLL_FRONT;
        return;
    }

    if (coll->side_mid.floor - coll->side_mid.ceiling <= 0) {
        coll->shift.x = coll->old.x - x_pos;
        coll->shift.y = coll->old.y - y_pos;
        coll->shift.z = coll->old.z - z_pos;
        coll->coll_type = COLL_CLAMP;
        return;
    }

    if (coll->side_mid.ceiling >= 0) {
        coll->shift.y = coll->side_mid.ceiling;
        coll->coll_type = COLL_TOP;
    }

    if (coll->side_front.floor > coll->bad_pos
        || coll->side_front.floor < coll->bad_neg
        || coll->side_front.ceiling > coll->bad_ceiling) {
        if (coll->side_front.type == HT_DIAGONAL
            || coll->side_front.type == HT_SPLIT_TRI) {
            coll->shift.x = coll->old.x - x;
            coll->shift.z = coll->old.z - z;
        } else {
            switch (coll->quadrant) {
            case DIR_NORTH:
            case DIR_SOUTH:
                coll->shift.x = coll->old.x - x_pos;
                coll->shift.z =
                    Room_FindGridShift(z_pos + probe_front.z, z_pos);
                break;

            case DIR_EAST:
            case DIR_WEST:
                coll->shift.x =
                    Room_FindGridShift(x_pos + probe_front.x, x_pos);
                coll->shift.z = coll->old.z - z_pos;
                break;

            default:
                break;
            }
        }

        coll->coll_type = COLL_FRONT;
        return;
    }

    if (coll->side_front.ceiling >= coll->bad_ceiling) {
        coll->shift.x = coll->old.x - x_pos;
        coll->shift.y = coll->old.y - y_pos;
        coll->shift.z = coll->old.z - z_pos;
        coll->coll_type = COLL_TOP_FRONT;
        return;
    }

    if (coll->side_left.floor > coll->bad_pos
        || coll->side_left.floor < coll->bad_neg) {
        if (coll->side_left.type == HT_SPLIT_TRI) {
            coll->shift.x = coll->old.x - x;
            coll->shift.z = coll->old.z - z;
        } else {
            switch (coll->quadrant) {
            case DIR_NORTH:
            case DIR_SOUTH:
                coll->shift.x = Room_FindGridShift(
                    x_pos + probe_left.x, x_pos + probe_front.x);
                break;

            case DIR_EAST:
            case DIR_WEST:
                coll->shift.z = Room_FindGridShift(
                    z_pos + probe_left.z, z_pos + probe_front.z);
                break;

            default:
                break;
            }
        }

        coll->coll_type = COLL_LEFT;
        return;
    }

    if (coll->side_right.floor > coll->bad_pos
        || coll->side_right.floor < coll->bad_neg) {
        if (coll->side_right.type == HT_SPLIT_TRI) {
            coll->shift.x = coll->old.x - x;
            coll->shift.z = coll->old.z - z;
        } else {
            switch (coll->quadrant) {
            case DIR_NORTH:
            case DIR_SOUTH:
                coll->shift.x = Room_FindGridShift(
                    x_pos + probe_right.x, x_pos + probe_front.x);
                break;

            case DIR_EAST:
            case DIR_WEST:
                coll->shift.z = Room_FindGridShift(
                    z_pos + probe_right.z, z_pos + probe_front.z);
                break;

            default:
                break;
            }
        }

        coll->coll_type = COLL_RIGHT;
        return;
    }
}

bool Collide_CollideStaticObjects(
    COLL_INFO *const coll, const int32_t x, const int32_t y, const int32_t z,
    const int16_t room_num, const int32_t height)
{
    coll->hit_static = 0;

    const int32_t in_x_min = x - coll->radius;
    const int32_t in_x_max = x + coll->radius;
    const int32_t in_y_min = y - height;
    const int32_t in_y_max = y;
    const int32_t in_z_min = z - coll->radius;
    const int32_t in_z_max = z + coll->radius;
    XYZ_32 shifter = { .x = 0, .z = 0 };

    Room_GetNearbyRooms(
        (XYZ_32) { x, y, z }, coll->radius + 50, height + 50, room_num);

    for (int32_t i = 0; i < Room_DrawGetCount(); i++) {
        const ROOM *const room = Room_Get(Room_DrawGetRoom(i));

        for (int32_t j = 0; j < room->num_static_meshes; j++) {
            const STATIC_MESH *const mesh = &room->static_meshes[j];
            const STATIC_OBJECT_3D *const obj =
                Object_Get3DStatic(mesh->static_num);

            if (!obj->collidable) {
                continue;
            }

            int32_t x_min;
            int32_t x_max;
            int32_t z_min;
            int32_t z_max;
            const int32_t y_min = mesh->pos.y + obj->collision_bounds.min.y;
            const int32_t y_max = mesh->pos.y + obj->collision_bounds.max.y;
            switch (mesh->rot.y) {
            case DEG_90:
                x_min = mesh->pos.x + obj->collision_bounds.min.z;
                x_max = mesh->pos.x + obj->collision_bounds.max.z;
                z_min = mesh->pos.z - obj->collision_bounds.max.x;
                z_max = mesh->pos.z - obj->collision_bounds.min.x;
                break;

            case -DEG_180:
                x_min = mesh->pos.x - obj->collision_bounds.max.x;
                x_max = mesh->pos.x - obj->collision_bounds.min.x;
                z_min = mesh->pos.z - obj->collision_bounds.max.z;
                z_max = mesh->pos.z - obj->collision_bounds.min.z;
                break;

            case -DEG_90:
                x_min = mesh->pos.x - obj->collision_bounds.max.z;
                x_max = mesh->pos.x - obj->collision_bounds.min.z;
                z_min = mesh->pos.z + obj->collision_bounds.min.x;
                z_max = mesh->pos.z + obj->collision_bounds.max.x;
                break;

            default:
                x_min = mesh->pos.x + obj->collision_bounds.min.x;
                x_max = mesh->pos.x + obj->collision_bounds.max.x;
                z_min = mesh->pos.z + obj->collision_bounds.min.z;
                z_max = mesh->pos.z + obj->collision_bounds.max.z;
                break;
            }

            if (in_x_max <= x_min || in_x_min >= x_max || in_y_max <= y_min
                || in_y_min >= y_max || in_z_max <= z_min
                || in_z_min >= z_max) {
                continue;
            }

            coll->hit_static = 1;
            if (g_Config.gameplay.enable_soft_statics) {
                return true;
            }

            int32_t shl = in_x_max - x_min;
            int32_t shr = x_max - in_x_min;
            if (shl < shr) {
                shifter.x = -shl;
            } else {
                shifter.x = shr;
            }

            shl = in_z_max - z_min;
            shr = z_max - in_z_min;
            if (shl < shr) {
                shifter.z = -shl;
            } else {
                shifter.z = shr;
            }

            switch (coll->quadrant) {
            case DIR_NORTH:
                if (shifter.x > coll->radius || shifter.x < -coll->radius) {
                    coll->coll_type = COLL_FRONT;
                    coll->shift.x = coll->old.x - x;
                    coll->shift.z = shifter.z;
                } else if (shifter.x > 0) {
                    coll->coll_type = COLL_LEFT;
                    coll->shift.x = shifter.x;
                    coll->shift.z = 0;
                } else if (shifter.x < 0) {
                    coll->coll_type = COLL_RIGHT;
                    coll->shift.x = shifter.x;
                    coll->shift.z = 0;
                }
                break;

            case DIR_EAST:
                if (shifter.z > coll->radius || shifter.z < -coll->radius) {
                    coll->coll_type = COLL_FRONT;
                    coll->shift.x = shifter.x;
                    coll->shift.z = coll->old.z - z;
                } else if (shifter.z > 0) {
                    coll->coll_type = COLL_RIGHT;
                    coll->shift.x = 0;
                    coll->shift.z = shifter.z;
                } else if (shifter.z < 0) {
                    coll->coll_type = COLL_LEFT;
                    coll->shift.x = 0;
                    coll->shift.z = shifter.z;
                }
                break;

            case DIR_SOUTH:
                if (shifter.x > coll->radius || shifter.x < -coll->radius) {
                    coll->coll_type = COLL_FRONT;
                    coll->shift.x = coll->old.x - x;
                    coll->shift.z = shifter.z;
                } else if (shifter.x > 0) {
                    coll->coll_type = COLL_RIGHT;
                    coll->shift.x = shifter.x;
                    coll->shift.z = 0;
                } else if (shifter.x < 0) {
                    coll->coll_type = COLL_LEFT;
                    coll->shift.x = shifter.x;
                    coll->shift.z = 0;
                }
                break;

            case DIR_WEST:
                if (shifter.z > coll->radius || shifter.z < -coll->radius) {
                    coll->coll_type = COLL_FRONT;
                    coll->shift.x = shifter.x;
                    coll->shift.z = coll->old.z - z;
                } else if (shifter.z > 0) {
                    coll->coll_type = COLL_LEFT;
                    coll->shift.x = 0;
                    coll->shift.z = shifter.z;
                } else if (shifter.z < 0) {
                    coll->coll_type = COLL_RIGHT;
                    coll->shift.x = 0;
                    coll->shift.z = shifter.z;
                }
                break;

            default:
                break;
            }

            return true;
        }
    }

    return false;
}

bool Collide_TestBoundsCollide(
    const COLL_ITEM *const src_item, const COLL_ITEM *const dst_item,
    const int32_t radius)
{
    const BOUNDS_16 *const src_bounds = &src_item->bounds;
    const BOUNDS_16 *const dst_bounds = &dst_item->bounds;

    if (src_item->pos.y + src_bounds->min.y
            >= dst_item->pos.y + dst_bounds->max.y
        || src_item->pos.y + src_bounds->max.y
            <= dst_item->pos.y + dst_bounds->min.y) {
        return false;
    }

    const int32_t c = Math_Cos(src_item->rot.y);
    const int32_t s = Math_Sin(src_item->rot.y);
    const int32_t dx = dst_item->pos.x - src_item->pos.x;
    const int32_t dz = dst_item->pos.z - src_item->pos.z;
    const int32_t rx = (c * dx - s * dz) >> W2V_SHIFT;
    const int32_t rz = (c * dz + s * dx) >> W2V_SHIFT;

    // clang-format off
    return (
        rx >= src_bounds->min.x - radius &&
        rx <= src_bounds->max.x + radius &&
        rz >= src_bounds->min.z - radius &&
        rz <= src_bounds->max.z + radius);
    // clang-format on
}

void Collide_DoProperDetection(ITEM *const item, const XYZ_32 old_pos)
{
    int32_t ceiling;
    int32_t height;
    int32_t oldonobj;
    int32_t bs;
    int32_t yang;

    int16_t room_num = item->room_num;
    const SECTOR *sector = Room_GetSector(old_pos, &room_num);
    int32_t oldheight = Room_GetHeight(sector, old_pos);
    int32_t oldtype = Room_GetHeightType();

    room_num = item->room_num;
    sector = Room_GetSector(item->pos, &room_num);
    height = Room_GetHeight(sector, item->pos);

    if (item->pos.y >= height) {
        bs = 0;

        if ((oldtype == HT_BIG_SLOPE || oldtype == HT_DIAGONAL)
            && oldheight < height) {
            yang = (uint16_t)item->rot.y;

            const int16_t tilt = Room_GetTiltType(sector, item->pos);
            const int8_t tiltxoff = tilt >> 8;
            const int8_t tiltyoff = (int8_t)tilt;
            if (tiltyoff < 0) {
                if (yang >= DEG_180) {
                    bs = 1;
                }
            } else if (tiltyoff > 0) {
                if (yang <= DEG_180) {
                    bs = 1;
                }
            }

            if (tiltxoff < 0) {
                if (yang >= DEG_90 && yang <= DEG_270) {
                    bs = 1;
                }
            } else if (tiltxoff > 0) {
                if (yang <= DEG_90 || yang >= DEG_270) {
                    bs = 1;
                }
            }
        }

        const bool x_cross = ROUND_TO_SECTOR(item->pos.x ^ old_pos.x) != 0;
        const bool z_cross = ROUND_TO_SECTOR(item->pos.z ^ old_pos.z) != 0;
        if (old_pos.y > height + 32 && !bs && (x_cross || z_cross)) {
            const bool xs = x_cross && z_cross
                ? ABS(old_pos.x - item->pos.x) < ABS(old_pos.z - item->pos.z)
                : true;
            item->rot.y = x_cross && xs ? -item->rot.y : -DEG_180 - item->rot.y;
            item->pos = old_pos;
            item->speed >>= 1;
        } else if (oldtype != HT_BIG_SLOPE && oldtype != HT_DIAGONAL) {
            if (item->fall_speed > 0) {
                if (item->fall_speed > 16) {
                    if (item->object_id == O_GRENADE) {
                        item->fall_speed =
                            (item->fall_speed >> 1) - item->fall_speed;
                    } else {
                        item->fall_speed = -(item->fall_speed >> 2);

                        if (item->fall_speed < -100) {
                            item->fall_speed = -100;
                        }
                    }
                } else {
                    item->fall_speed = 0;

                    if (item->object_id == O_GRENADE) {
                        item->speed--;
                        item->required_anim_state = 1;
                        item->rot.x = 0;
                    } else {
                        item->speed -= 3;
                    }

                    if (item->speed < 0) {
                        item->speed = 0;
                    }
                }
            }

            item->pos.y = height;
        } else {
            item->speed -= item->speed >> 2;

            const int16_t tilt = Room_GetTiltType(sector, item->pos);
            const int8_t tiltxoff = tilt >> 8;
            const int8_t tiltyoff = (int8_t)tilt;
            if (tiltyoff < 0 && ABS(tiltyoff) - ABS(tiltxoff) >= 2) {
                if ((uint16_t)item->rot.y > DEG_180) {
                    item->rot.y = -1 - item->rot.y;

                    if (item->fall_speed > 0) {
                        item->fall_speed = -(item->fall_speed >> 1);
                    }
                } else {
                    if (item->speed < 32) {
                        item->speed -= tiltyoff << 1;

                        if ((uint16_t)item->rot.y > DEG_90
                            && (uint16_t)item->rot.y < DEG_270) {
                            item->rot.y -= 0x1000;

                            if ((uint16_t)item->rot.y < DEG_90) {
                                item->rot.y = DEG_90;
                            }
                        } else if ((uint16_t)item->rot.y < DEG_90) {
                            item->rot.y += 0x1000;

                            if ((uint16_t)item->rot.y > DEG_90) {
                                item->rot.y = DEG_90;
                            }
                        }
                    }

                    item->fall_speed =
                        item->fall_speed > 0 ? -(item->fall_speed >> 1) : 0;
                }
            } else if (tiltyoff > 0 && ABS(tiltyoff) - ABS(tiltxoff) >= 2) {
                if ((uint16_t)item->rot.y < DEG_180) {
                    item->rot.y = -1 - item->rot.y;

                    if (item->fall_speed > 0) {
                        item->fall_speed = -(item->fall_speed >> 1);
                    }
                } else {
                    if (item->speed < 32) {
                        item->speed += tiltyoff << 1;

                        if ((uint16_t)item->rot.y > DEG_270
                            || (uint16_t)item->rot.y < DEG_90) {
                            item->rot.y -= 0x1000;

                            if ((uint16_t)item->rot.y < DEG_270) {
                                item->rot.y = -DEG_90;
                            }
                        } else if ((uint16_t)item->rot.y < DEG_270) {
                            item->rot.y += 0x1000;

                            if ((uint16_t)item->rot.y > DEG_270) {
                                item->rot.y = -DEG_90;
                            }
                        }
                    }

                    item->fall_speed =
                        item->fall_speed > 0 ? -(item->fall_speed >> 1) : 0;
                }
            } else if (tiltxoff < 0 && ABS(tiltxoff) - ABS(tiltyoff) >= 2) {
                if ((uint16_t)item->rot.y > DEG_90
                    && (uint16_t)item->rot.y < DEG_270) {
                    item->rot.y = 0x7FFF - item->rot.y;

                    if (item->fall_speed > 0) {
                        item->fall_speed = -(item->fall_speed >> 1);
                    }
                } else {
                    if (item->speed < 32) {
                        item->speed -= tiltxoff << 1;

                        if ((uint16_t)item->rot.y < DEG_180) {
                            item->rot.y -= DEG_90;

                            if ((uint16_t)item->rot.y > 61440) {
                                item->rot.y = 0;
                            }
                        } else {
                            item->rot.y += DEG_90;

                            if ((uint16_t)item->rot.y < DEG_90) {
                                item->rot.y = 0;
                            }
                        }
                    }

                    item->fall_speed =
                        item->fall_speed > 0 ? -(item->fall_speed >> 1) : 0;
                }
            } else if (tiltxoff > 0 && ABS(tiltxoff) - ABS(tiltyoff) >= 2) {
                if ((uint16_t)item->rot.y > DEG_270
                    || (uint16_t)item->rot.y < DEG_90) {
                    item->rot.y = 0x7FFF - item->rot.y;

                    if (item->fall_speed > 0) {
                        item->fall_speed = -(item->fall_speed >> 1);
                    }
                } else {
                    if (item->speed < 32) {
                        item->speed += tiltxoff << 1;

                        if ((uint16_t)item->rot.y > DEG_180) {
                            item->rot.y -= 0x1000;

                            if ((uint16_t)item->rot.y < DEG_180) {
                                item->rot.y = -DEG_180;
                            }
                        } else if ((uint16_t)item->rot.y < DEG_180) {
                            item->rot.y += 0x1000;

                            if ((uint16_t)item->rot.y > DEG_180) {
                                item->rot.y = -DEG_180;
                            }
                        }
                    }

                    item->fall_speed =
                        item->fall_speed > 0 ? -(item->fall_speed >> 1) : 0;
                }
            } else if (tiltyoff < 0 && tiltxoff < 0) {
                if ((uint16_t)item->rot.y > DEG_135
                    && (uint16_t)item->rot.y < DEG_315) {
                    item->rot.y = -(DEG_90 + 1) - item->rot.y;

                    if (item->fall_speed > 0) {
                        item->fall_speed = -(item->fall_speed >> 1);
                    }
                } else {
                    if (item->speed < 32) {
                        item->speed -= tiltxoff + tiltyoff;

                        if ((uint16_t)item->rot.y > DEG_45
                            && (uint16_t)item->rot.y < DEG_225) {
                            item->rot.y -= 0x1000;

                            if ((uint16_t)item->rot.y < DEG_45) {
                                item->rot.y = DEG_45;
                            }
                        } else if (item->rot.y != DEG_45) {
                            item->rot.y += 0x1000;

                            if ((uint16_t)item->rot.y > DEG_45) {
                                item->rot.y = DEG_45;
                            }
                        }
                    }

                    item->fall_speed =
                        item->fall_speed > 0 ? -(item->fall_speed >> 1) : 0;
                }
            } else if (tiltyoff < 0 && tiltxoff > 0) {
                if ((uint16_t)item->rot.y > DEG_225
                    || (uint16_t)item->rot.y < DEG_45) {
                    item->rot.y = DEG_90 - 1 - item->rot.y;

                    if (item->fall_speed > 0) {
                        item->fall_speed = -(item->fall_speed >> 1);
                    }
                } else {
                    if (item->speed < 32) {
                        item->speed += tiltxoff - tiltyoff;

                        if ((uint16_t)item->rot.y < DEG_315
                            && (uint16_t)item->rot.y > DEG_135) {
                            item->rot.y -= 0x1000;

                            if ((uint16_t)item->rot.y < DEG_135) {
                                item->rot.y = DEG_135;
                            }
                        } else if (item->rot.y != DEG_135) {
                            item->rot.y += 0x1000;

                            if ((uint16_t)item->rot.y > DEG_135) {
                                item->rot.y = DEG_135;
                            }
                        }
                    }

                    item->fall_speed =
                        item->fall_speed > 0 ? -(item->fall_speed >> 1) : 0;
                }
            } else if (tiltyoff > 0 && tiltxoff > 0) {
                if ((uint16_t)item->rot.y > DEG_315
                    || (uint16_t)item->rot.y < DEG_135) {
                    item->rot.y = -(DEG_90 + 1) - item->rot.y;

                    if (item->fall_speed > 0) {
                        item->fall_speed = -(item->fall_speed >> 1);
                    }
                } else {
                    if (item->speed < 32) {
                        item->speed += tiltxoff + tiltyoff;

                        if ((uint16_t)item->rot.y < DEG_45
                            || (uint16_t)item->rot.y > DEG_225) {
                            item->rot.y -= 0x1000;

                            if ((uint16_t)item->rot.y < DEG_225) {
                                item->rot.y = -DEG_135;
                            }
                        } else if ((uint16_t)item->rot.y != DEG_225) {
                            item->rot.y += 0x1000;

                            if ((uint16_t)item->rot.y > DEG_225) {
                                item->rot.y = -DEG_135;
                            }
                        }
                    }

                    item->fall_speed =
                        item->fall_speed > 0 ? -(item->fall_speed >> 1) : 0;
                }
            } else if (tiltyoff > 0 && tiltxoff < 0) {
                if ((uint16_t)item->rot.y > DEG_45
                    && (uint16_t)item->rot.y < DEG_225) {
                    item->rot.y = DEG_90 - 1 - item->rot.y;

                    if (item->fall_speed > 0) {
                        item->fall_speed = -(item->fall_speed >> 1);
                    }
                } else {
                    if (item->speed < 32) {
                        item->speed += tiltyoff - tiltxoff;

                        if ((uint16_t)item->rot.y < DEG_135
                            || (uint16_t)item->rot.y > DEG_315) {
                            item->rot.y -= 0x1000;

                            if ((uint16_t)item->rot.y < DEG_315) {
                                item->rot.y = -DEG_45;
                            }
                        } else if ((uint16_t)item->rot.y != DEG_315) {
                            item->rot.y += 0x1000;

                            if ((uint16_t)item->rot.y > DEG_315) {
                                item->rot.y = -DEG_45;
                            }
                        }
                    }

                    item->fall_speed =
                        item->fall_speed > 0 ? -(item->fall_speed >> 1) : 0;
                }
            }

            item->pos = old_pos;
        }
    } else {
        if (item->fall_speed >= 0) {
            const XYZ_32 test_pos = {
                item->pos.x,
                old_pos.y,
                item->pos.z,
            };
            room_num = item->room_num;
            sector = Room_GetSector(test_pos, &room_num);
            height = Room_GetHeight(sector, test_pos);
            const bool on_walkable =
                Room_IsOnWalkable(sector, test_pos, height, NO_ITEM);

            if (item->pos.y >= height && on_walkable) {
                if (item->fall_speed > 16) {
                    if (item->object_id == O_GRENADE) {
                        item->fall_speed = -(item->fall_speed / 2);
                    } else {
                        item->fall_speed = -(item->fall_speed / 4);
                        CLAMPL(item->fall_speed, -100);
                    }
                } else if (item->fall_speed > 0) {
                    item->fall_speed = 0;
                    if (item->object_id == O_GRENADE) {
                        item->speed--;
                        item->required_anim_state = 1;
                        item->rot.x = 0;
                    } else {
                        item->speed -= 3;
                    }
                    CLAMPL(item->speed, 0);
                }
                item->pos.y = height;
            }
        }

        room_num = item->room_num;
        sector = Room_GetSector(item->pos, &room_num);
        ceiling = Room_GetCeiling(sector, item->pos);

        if (item->pos.y < ceiling) {
            const bool x_cross = ROUND_TO_SECTOR(item->pos.x ^ old_pos.x) != 0;
            const bool z_cross = ROUND_TO_SECTOR(item->pos.z ^ old_pos.z) != 0;
            if (old_pos.y < ceiling && (x_cross || z_cross)) {
                item->rot.y = x_cross ? -item->rot.y : -DEG_180 - item->rot.y;

                if (item->object_id == O_GRENADE) {
                    item->speed -= item->speed >> 3;
                } else {
                    item->speed >>= 1;
                }

                item->pos = old_pos;
            } else {
                item->pos.y = ceiling;
            }

            if (item->fall_speed < 0) {
                item->fall_speed = -item->fall_speed;
            }
        }
    }

    room_num = item->room_num;
    Room_GetSector(item->pos, &room_num);

    if (item->room_num != room_num) {
        Item_UpdateRoom(Item_GetIndex(item), room_num);
    }
}

void Collide_ShiftItem(ITEM *const item, COLL_INFO *const coll)
{
    item->pos.x += coll->shift.x;
    item->pos.y += coll->shift.y;
    item->pos.z += coll->shift.z;
    coll->shift.x = 0;
    coll->shift.y = 0;
    coll->shift.z = 0;
}
