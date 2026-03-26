#include <trx/core/math.h>
#include <trx/debug.h>
#include <trx/game/camera.h>
#include <trx/game/lara.h>
#include <trx/game/rooms.h>

// clang-format off
#define M_LOS_STEPS         8
#define M_MAX_SNAPS         8
#define M_SNAP_DELTA        (STEP_L * 3) // = 768
#define M_DEFAULT_ELEVATION (-10 * DEG_1) // = -1820
#define M_MAX_ELEVATION     (85 * DEG_1) // = 15470
#define M_CHASE_SHIFT       (STEP_L * 3 / 2) // = 384
#define M_LOOK_SHIFT        (STEP_L * 7 / 8) // = 224
#define M_CHASE_SPEED       10
#define M_MAX_LOOK_TILT     (55 * DEG_1) // = 10010
#define M_MIN_LOOK_TILT     (-75 * DEG_1) // = -13650
#define M_MAX_LOOK_ROTATION (80 * DEG_1) // = 14560
#define M_MIN_LOOK_ROTATION -M_MAX_LOOK_ROTATION // = -14560
// clang-format on

typedef struct {
    struct {
        int16_t current_anim_state;
        int16_t goal_anim_state;
        XYZ_32 pos;
        XYZ_16 rot;
        XYZ_16 head_rot;
        XYZ_16 torso_rot;
    } lara;
    CAMERA_TYPE cam_type;
    int16_t additional_angle;
    int16_t additional_elevation;
} M_STATE;

typedef struct {
    GAME_VECTOR pos;
    GAME_VECTOR target;
} M_IDEAL;

static M_STATE m_LastState = {};
static M_IDEAL m_LastIdeal = {};
static M_IDEAL m_LastLookIdeal = {};
static int32_t m_Snaps = 0;

static CAMERA_LOOK_SETTINGS m_LookSettings = {
    // clang-format off
    .head_turn         = +2 * DEG_1,
    .max_head_rotation = +44 * DEG_1,
    .min_head_rotation = -44 * DEG_1,
    .max_head_tilt     = +30 * DEG_1,
    .min_head_tilt     = -35 * DEG_1,
    .torso_head_rot_y  = 1.0f,
    .torso_head_rot_x  = 1.0f,
    // clang-format on
};

static int16_t M_GetChaseSpeed(void)
{
    return M_CHASE_SPEED;
}

static const CAMERA_LOOK_SETTINGS *M_GetLookSettingsFunc(const bool on_surface)
{
    return &m_LookSettings;
}

static bool M_LOS(
    GAME_VECTOR *const start, GAME_VECTOR *const target, const int32_t shift)
{
    const XYZ_32 delta = {
        .x = (target->x - start->x) >> 3,
        .y = (target->y - start->y) >> 3,
        .z = (target->z - start->z) >> 3,
    };

    XYZ_32 pos = start->pos;
    int16_t room_num = start->room_num;
    bool valid_space = false;
    bool clipped = false;

    int32_t i = 0;
    for (; i < M_LOS_STEPS; i++) {
        int16_t next_room_num = room_num;
        const SECTOR *const sector = Room_GetSector(pos, &next_room_num);

        if (Room_Get(next_room_num)->flags.swamp) {
            clipped = true;
            break;
        }

        const int32_t height = Room_GetHeightEx(sector, pos, true, NO_ITEM);
        const int32_t ceiling = Room_GetCeilingEx(sector, pos, true);

        if (height == NO_HEIGHT || ceiling == NO_HEIGHT || ceiling >= height) {
            if (!valid_space) {
                pos.x += delta.x;
                pos.y += delta.y;
                pos.z += delta.z;
                continue;
            }

            clipped = true;
            break;
        }

        if (pos.y > height) {
            const int32_t height_diff = pos.y - height;
            if (height_diff < shift) {
                pos.y = height;
            } else {
                clipped = true;
                break;
            }
        }

        if (pos.y < ceiling) {
            const int32_t ceiling_diff = ceiling - pos.y;
            if (ceiling_diff < shift) {
                pos.y = ceiling;
            } else {
                clipped = true;
                break;
            }
        }

        valid_space = true;
        room_num = next_room_num;
        pos.x += delta.x;
        pos.y += delta.y;
        pos.z += delta.z;
    }

    if (i != 0) {
        pos.x -= delta.x;
        pos.y -= delta.y;
        pos.z -= delta.z;
    }

    Room_GetSector(pos, &room_num);
    target->pos = pos;
    target->room_num = room_num;

    return !clipped;
}

static inline void M_ClampY(int16_t room_num, XYZ_32 *const pos)
{
    const SECTOR *const sector = Room_GetSector(*pos, &room_num);
    const int32_t height = Room_GetHeightEx(sector, *pos, true, NO_ITEM);
    const int32_t ceiling = Room_GetCeilingEx(sector, *pos, true);

    if (ceiling < height && ceiling != NO_HEIGHT && height != NO_HEIGHT) {
        if (ceiling > pos->y - 255 && height < pos->y + 255) {
            pos->y = (ceiling + height) >> 1;
        } else if (height < pos->y + 255) {
            pos->y = height - 255;
        } else if (ceiling > pos->y - 255) {
            pos->y = ceiling + 255;
        }
    }
}

static bool M_Collide(
    GAME_VECTOR *const ideal, const int32_t shift, const bool y_first)
{
    XYZ_32 pos = ideal->pos;
    if (y_first) {
        M_ClampY(ideal->room_num, &pos);
    }

#define L_OUT_OF_BOUNDS                                                        \
    (height < pos.y || height == NO_HEIGHT || ceiling == NO_HEIGHT             \
     || ceiling >= height || pos.y < ceiling)

    // -X clamp
    int16_t room_num = ideal->room_num;
    XYZ_32 sample_pos = { .x = pos.x - shift, .y = pos.y, .z = pos.z };
    const SECTOR *sector = Room_GetSector(sample_pos, &room_num);
    int32_t height = Room_GetHeightEx(sector, sample_pos, true, NO_ITEM);
    int32_t ceiling = Room_GetCeilingEx(sector, sample_pos, true);
    if (L_OUT_OF_BOUNDS) {
        pos.x = ROUND_TO_SECTOR(pos.x) + shift;
    }

    // -Z clamp
    room_num = ideal->room_num;
    sample_pos = (XYZ_32) { .x = pos.x, .y = pos.y, .z = pos.z - shift };
    sector = Room_GetSector(sample_pos, &room_num);
    height = Room_GetHeightEx(sector, sample_pos, true, NO_ITEM);
    ceiling = Room_GetCeilingEx(sector, sample_pos, true);
    if (L_OUT_OF_BOUNDS) {
        pos.z = ROUND_TO_SECTOR(pos.z) + shift;
    }

    // +X clamp
    room_num = ideal->room_num;
    sample_pos = (XYZ_32) { .x = pos.x + shift, .y = pos.y, .z = pos.z };
    sector = Room_GetSector(sample_pos, &room_num);
    height = Room_GetHeightEx(sector, sample_pos, true, NO_ITEM);
    ceiling = Room_GetCeilingEx(sector, sample_pos, true);
    if (L_OUT_OF_BOUNDS) {
        pos.x = ROUND_TO_SECTOR_END(pos.x) - shift;
    }

    // +Z clamp
    room_num = ideal->room_num;
    sample_pos = (XYZ_32) { .x = pos.x, .y = pos.y, .z = pos.z + shift };
    sector = Room_GetSector(sample_pos, &room_num);
    height = Room_GetHeightEx(sector, sample_pos, true, NO_ITEM);
    ceiling = Room_GetCeilingEx(sector, sample_pos, true);
    if (L_OUT_OF_BOUNDS) {
        pos.z = ROUND_TO_SECTOR_END(pos.z) - shift;
    }

    if (!y_first) {
        M_ClampY(ideal->room_num, &pos);
    }

    room_num = ideal->room_num;
    sector = Room_GetSector(pos, &room_num);
    height = Room_GetHeightEx(sector, pos, true, NO_ITEM);
    ceiling = Room_GetCeilingEx(sector, pos, true);
    if (L_OUT_OF_BOUNDS) {
        return true;
    }

#undef L_OUT_OF_BOUNDS

    Room_GetSector(pos, &ideal->room_num);
    ideal->pos = pos;
    return false;
}

static bool M_UpdateLaraState(void)
{
    const LARA_INFO *const lara = Lara_GetLaraInfo();
    const ITEM *const lara_item = Lara_GetItem();

    bool same_lara_state =
        m_LastState.lara.current_anim_state == lara_item->current_anim_state
        && m_LastState.lara.goal_anim_state == lara_item->goal_anim_state
        && XYZ_16_AreEquivalent(m_LastState.lara.head_rot, lara->head_rot)
        && XYZ_32_AreEquivalent(m_LastState.lara.pos, lara_item->pos);
    bool same_camera_state = m_LastState.cam_type == g_Camera.type;
    if (g_Camera.type != CAM_LOOK) {
        same_lara_state &=
            XYZ_16_AreEquivalent(m_LastState.lara.rot, lara_item->rot)
            && XYZ_16_AreEquivalent(
                m_LastState.lara.torso_rot, lara->torso_rot);
        same_camera_state &=
            m_LastState.additional_angle == g_Camera.additional_angle
            && m_LastState.additional_elevation
                == g_Camera.additional_elevation;
    }
    if (same_lara_state && same_camera_state) {
        return false;
    }

    m_LastState.lara.current_anim_state = lara_item->current_anim_state;
    m_LastState.lara.goal_anim_state = lara_item->goal_anim_state;
    m_LastState.lara.head_rot = lara->head_rot;
    m_LastState.lara.pos = lara_item->pos;
    if (g_Camera.type != CAM_LOOK) {
        m_LastState.lara.rot = lara_item->rot;
        m_LastState.lara.torso_rot = lara->torso_rot;
        m_LastState.additional_angle = g_Camera.additional_angle;
        m_LastState.additional_elevation = g_Camera.additional_elevation;
    }

    return true;
}

static void M_Move(GAME_VECTOR *const ideal, const int32_t speed)
{
    if (M_UpdateLaraState()) {
        m_LastIdeal.pos = *ideal;
    } else {
        *ideal = m_LastIdeal.pos;
    }

    g_Camera.pos.x += (ideal->x - g_Camera.pos.x) / speed;
    g_Camera.pos.y += (ideal->y - g_Camera.pos.y) / speed;
    g_Camera.pos.z += (ideal->z - g_Camera.pos.z) / speed;
    g_Camera.pos.room_num = ideal->room_num;

    Camera_ApplyBounce();

    XYZ_32 pos = g_Camera.pos.pos;
    int16_t room_num = g_Camera.pos.room_num;
    const XYZ_32 sample_pos = { pos.x, pos.y + STEP_L, pos.z };
    const SECTOR *sector = Room_GetSector(sample_pos, &room_num);

    const ROOM *const room = Room_Get(room_num);
    if (room->flags.swamp) {
        pos.y = room->max_ceiling - STEP_L;
        Room_GetSector(pos, &g_Camera.pos.room_num);
    }

    sector = Room_GetSector(pos, &room_num);
    int32_t height = Room_GetHeightEx(sector, pos, true, NO_ITEM);
    int32_t ceiling = Room_GetCeilingEx(sector, pos, true);

    if (pos.y < ceiling || pos.y > height) {
        M_LOS(&g_Camera.target, &g_Camera.pos, 0);
        const XYZ_32 delta = {
            .x = ABS(g_Camera.pos.x - ideal->x),
            .y = ABS(g_Camera.pos.y - ideal->y),
            .z = ABS(g_Camera.pos.z - ideal->z),
        };

        if (delta.x < M_SNAP_DELTA && delta.y < M_SNAP_DELTA
            && delta.z < M_SNAP_DELTA) {
            GAME_VECTOR start = *ideal;
            GAME_VECTOR target = g_Camera.pos;

            if (!M_LOS(&start, &target, 0)) {
                m_Snaps++;
                if (m_Snaps >= M_MAX_SNAPS) {
                    g_Camera.pos = *ideal;
                    m_Snaps = 0;
                }
            }
        }
    }

    pos = g_Camera.pos.pos;
    room_num = g_Camera.pos.room_num;
    sector = Room_GetSector(pos, &room_num);
    height = Room_GetHeightEx(sector, pos, true, NO_ITEM);
    ceiling = Room_GetCeilingEx(sector, pos, true);

    if (pos.y - 255 < ceiling && pos.y + 255 > height && ceiling < height
        && ceiling != NO_HEIGHT && height != NO_HEIGHT) {
        g_Camera.pos.y = (ceiling + height) >> 1;
    } else if (
        pos.y + 255 > height && ceiling < height && ceiling != NO_HEIGHT
        && height != NO_HEIGHT) {
        g_Camera.pos.y = height - 255;
    } else if (
        pos.y - 255 < ceiling && ceiling < height && ceiling != NO_HEIGHT
        && height != NO_HEIGHT) {
        g_Camera.pos.y = ceiling + 255;
    } else if (
        ceiling >= height || height == NO_HEIGHT || ceiling == NO_HEIGHT) {
        g_Camera.pos = *ideal;
    }

    Room_GetSector(g_Camera.pos.pos, &g_Camera.pos.room_num);
    m_LastState.cam_type = g_Camera.type;

    Camera_UpdateMicPosition();
}

static GAME_VECTOR M_GetIdeal(
    const int32_t distance, const int16_t target_rot_y)
{
    int32_t farthest = 0x7FFFFFFF;
    int32_t farthest_num = 0;
    GAME_VECTOR temp[2] = {};
    GAME_VECTOR ideals[5] = {};

    for (int32_t i = 0; i < 5; i++) {
        ideals[i].y =
            ((Math_Sin(g_Camera.target_elevation) * g_Camera.target_distance)
             >> W2V_SHIFT)
            + g_Camera.target.y;
    }

    for (int32_t i = 0; i < 5; i++) {
        const int16_t angle = i > 0 ? ((i - 1) << W2V_SHIFT)
                                    : (g_Camera.target_angle + target_rot_y);
        ideals[i].x =
            g_Camera.target.x - ((distance * Math_Sin(angle)) >> W2V_SHIFT);
        ideals[i].z =
            g_Camera.target.z - ((distance * Math_Cos(angle)) >> W2V_SHIFT);

        ideals[i].room_num = g_Camera.target.room_num;

        if (M_LOS(&g_Camera.target, &ideals[i], 200)) {
            temp[0] = ideals[i];
            temp[1] = g_Camera.pos;

            if (i == 0 || M_LOS(&temp[0], &temp[1], 0)) {
                if (i == 0) {
                    farthest_num = 0;
                    break;
                }

                const int32_t dx = SQUARE(g_Camera.pos.x - ideals[i].x);
                const int32_t dz = SQUARE(g_Camera.pos.z - ideals[i].z) + dx;
                if (dz < farthest) {
                    farthest = dz;
                    farthest_num = i;
                }
            }
        } else if (i == 0) {
            temp[0] = ideals[i];
            temp[1] = g_Camera.pos;

            const int32_t dx = SQUARE(g_Camera.target.x - ideals[i].x);
            const int32_t dz = SQUARE(g_Camera.target.z - ideals[i].z) + dx;
            if (dz > 0x90000) {
                farthest_num = 0;
                break;
            }
        }
    }

    return ideals[farthest_num];
}

static void M_Chase(const ITEM *const item)
{
    if (g_Camera.target_elevation == 0) {
        g_Camera.target_elevation = M_DEFAULT_ELEVATION;
    }
    g_Camera.target_elevation += item->rot.x;
    CLAMP(g_Camera.target_elevation, -M_MAX_ELEVATION, M_MAX_ELEVATION);

    const int32_t distance =
        (g_Camera.target_distance * Math_Cos(g_Camera.target_elevation))
        >> W2V_SHIFT;

    int16_t room_num = g_Camera.target.room_num;
    const SECTOR *sector = Room_GetSector(
        (XYZ_32) {
            g_Camera.target.x,
            g_Camera.target.y + STEP_L,
            g_Camera.target.z,
        },
        &room_num);

    const ROOM *const room = Room_Get(room_num);
    if (room->flags.swamp) {
        g_Camera.target.y = room->max_ceiling - STEP_L;
    }

    XYZ_32 pos = g_Camera.target.pos;
    sector = Room_GetSector(pos, &g_Camera.target.room_num);
    int32_t height = Room_GetHeightEx(sector, pos, true, NO_ITEM);
    int32_t ceiling = Room_GetCeilingEx(sector, pos, true);

    if (ceiling + 16 > height - 16 && height != NO_HEIGHT
        && ceiling != NO_HEIGHT) {
        g_Camera.target.y = (height + ceiling) >> 1;
        g_Camera.target_elevation = 0;
    } else if (pos.y > height - 16 && height != NO_HEIGHT) {
        g_Camera.target.y = height - 16;
        g_Camera.target_elevation = 0;
    } else if (pos.y < ceiling + 16 && ceiling != NO_HEIGHT) {
        g_Camera.target.y = ceiling + 16;
        g_Camera.target_elevation = 0;
    }

    sector = Room_GetSector(g_Camera.target.pos, &g_Camera.target.room_num);
    pos = g_Camera.target.pos;
    room_num = g_Camera.target.room_num;
    sector = Room_GetSector(g_Camera.target.pos, &room_num);
    height = Room_GetHeightEx(sector, pos, true, NO_ITEM);
    ceiling = Room_GetCeilingEx(sector, pos, true);

    if (pos.y < ceiling || pos.y > height || ceiling >= height
        || height == NO_HEIGHT || ceiling == NO_HEIGHT) {
        g_Camera.target = m_LastIdeal.target;
    }

    GAME_VECTOR ideal = M_GetIdeal(distance, item->rot.y);
    M_Collide(&ideal, M_CHASE_SHIFT, true);

    if (m_LastState.cam_type == CAM_FIXED) {
        g_Camera.speed = 1;
    }

    M_Move(&ideal, g_Camera.speed);
}

static void M_Combat(const ITEM *const item)
{
    g_Camera.target.x = item->pos.x;
    g_Camera.target.z = item->pos.z;

    const LARA_INFO *const lara = Lara_GetLaraInfo();
    if (lara->target != nullptr) {
        g_Camera.target_angle = lara->target_angles[0] + item->rot.y;
        g_Camera.target_elevation = lara->target_angles[1] + item->rot.x;
    } else {
        g_Camera.target_angle =
            lara->torso_rot.y + lara->head_rot.y + item->rot.y;
        g_Camera.target_elevation =
            lara->head_rot.x + item->rot.x + lara->torso_rot.x - 2730;
    }

    int16_t room_num = g_Camera.target.room_num;
    const SECTOR *sector = Room_GetSector(
        (XYZ_32) {
            g_Camera.target.x,
            g_Camera.target.y + STEP_L,
            g_Camera.target.z,
        },
        &room_num);

    const ROOM *const room = Room_Get(room_num);
    if (room->flags.swamp) {
        g_Camera.target.y = room->max_ceiling - STEP_L;
    }

    XYZ_32 pos = g_Camera.target.pos;
    sector = Room_GetSector(pos, &g_Camera.target.room_num);
    int32_t height = Room_GetHeightEx(sector, pos, true, NO_ITEM);
    int32_t ceiling = Room_GetCeilingEx(sector, pos, true);

    if (ceiling + 64 > height - 64 && height != NO_HEIGHT
        && ceiling != NO_HEIGHT) {
        g_Camera.target.y = (ceiling + height) >> 1;
        g_Camera.target_elevation = 0;
    } else if (pos.y > height - 64 && height != NO_HEIGHT) {
        g_Camera.target.y = height - 64;
        g_Camera.target_elevation = 0;
    } else if (pos.y < ceiling + 64 && ceiling != NO_HEIGHT) {
        g_Camera.target.y = ceiling + 64;
        g_Camera.target_elevation = 0;
    }

    pos = g_Camera.target.pos;
    Room_GetSector(pos, &g_Camera.target.room_num);
    room_num = g_Camera.target.room_num;
    sector = Room_GetSector(pos, &room_num);
    height = Room_GetHeightEx(sector, pos, true, NO_ITEM);
    ceiling = Room_GetCeilingEx(sector, pos, true);

    if (pos.y < ceiling || pos.y > height || ceiling >= height
        || height == NO_HEIGHT || ceiling == NO_HEIGHT) {
        g_Camera.target = m_LastIdeal.target;
    }

    g_Camera.target_distance = CAMERA_DEFAULT_DISTANCE;
    const int32_t distance =
        g_Camera.target_distance * Math_Cos(g_Camera.target_elevation)
        >> W2V_SHIFT;

    GAME_VECTOR ideal = M_GetIdeal(distance, 0);
    M_Collide(&ideal, M_CHASE_SHIFT, true);

    if (m_LastState.cam_type == CAM_FIXED) {
        g_Camera.speed = 1;
    }

    M_Move(&ideal, g_Camera.speed);
}

static void M_Fixed(void)
{
    const OBJECT_VECTOR *const fixed = Camera_GetFixedObject(g_Camera.num);
    GAME_VECTOR target = {
        .x = fixed->x,
        .y = fixed->y,
        .z = fixed->z,
        .room_num = fixed->data,
    };

    g_Camera.fixed_camera = true;
    M_Move(&target, g_Camera.speed);

    if (g_Camera.timer != 0) {
        g_Camera.timer--;
        if (g_Camera.timer == 0) {
            g_Camera.timer = -1;
        }
    }
}

static XYZ_32 M_GetHeadPos(const int32_t x, const int32_t y, const int32_t z)
{
    XYZ_32 pos = {
        .x = x,
        .y = y,
        .z = z,
    };
    Lara_GetMeshPos(LM_HEAD, &pos);
    return pos;
}

static void M_Look(const ITEM *const item)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    const ITEM *const lara_item = Lara_GetItem();
    XYZ_16 head_rot = lara->head_rot;
    XYZ_16 torso_rot = lara->torso_rot;

    lara->torso_rot.x = 0;
    lara->torso_rot.y = 0;
    lara->head_rot.x <<= 1;
    lara->head_rot.y <<= 1;

    CLAMP(lara->head_rot.x, M_MIN_LOOK_TILT, M_MAX_LOOK_TILT);
    CLAMP(lara->head_rot.y, M_MIN_LOOK_ROTATION, M_MAX_LOOK_ROTATION);

    // Get head-relative points in mesh space (faithful), then project the
    // camera's ray onto the head->forward axis to remove breathing-induced
    // roll/tilt wobble without moving the ray off Lara's head.
    const XYZ_32 head_pos = M_GetHeadPos(0, 0, 0);
    XYZ_32 pos_1 = M_GetHeadPos(0, 16, 64);

    int16_t room_num = lara_item->room_num;
    const XYZ_32 sample_pos = { pos_1.x, pos_1.y + STEP_L, pos_1.z };
    const SECTOR *sector = Room_GetSector(sample_pos, &room_num);

    const ROOM *room = Room_Get(room_num);
    if (room->flags.swamp) {
        pos_1.y = room->max_ceiling - STEP_L;
    }

    sector = Room_GetSector(pos_1, &room_num);
    int32_t height = Room_GetHeight(sector, pos_1);
    int32_t ceiling = Room_GetCeiling(sector, pos_1);

    if (height == NO_HEIGHT || ceiling == NO_HEIGHT || ceiling >= height
        || pos_1.y > height || pos_1.y < ceiling) {
        pos_1 = M_GetHeadPos(0, 16, 0);
        room_num = lara_item->room_num;
        sector = Room_GetSector(
            (XYZ_32) { pos_1.x, pos_1.y + STEP_L, pos_1.z }, &room_num);

        room = Room_Get(room_num);
        if (room->flags.swamp) {
            pos_1.y = room->max_ceiling - STEP_L;
        }

        sector = Room_GetSector(pos_1, &room_num);
        height = Room_GetHeight(sector, pos_1);
        ceiling = Room_GetCeiling(sector, pos_1);

        if (height == NO_HEIGHT || ceiling == NO_HEIGHT || ceiling >= height
            || pos_1.y > height || pos_1.y < ceiling) {
            pos_1 = M_GetHeadPos(0, 16, -64);
        }
    }

    XYZ_32 pos_2 = M_GetHeadPos(0, 0, -1024);
    XYZ_32 pos_3 = M_GetHeadPos(0, 0, 2048);

    // Constrain the camera ray to pass through Lara's head (OG behavior), but
    // remove idle-breathing wobble by projecting onto a stable forward axis
    // derived from yaw + pitch (no roll/tilt from torso animation).
    const int16_t yaw = lara_item->rot.y + lara->head_rot.y;
    const int16_t pitch = lara_item->rot.x + lara->head_rot.x;
    const XYZ_32 axis = XYZ_32_FromYawPitch(yaw, pitch, 1 << W2V_SHIFT);

    const int64_t axis_len2 = XYZ_32_GetLength2_64(axis);
    if (axis_len2 != 0) {
        XYZ_32_ProjectPointOntoAxis(head_pos, axis, axis_len2, &pos_1);
        XYZ_32_ProjectPointOntoAxis(head_pos, axis, axis_len2, &pos_2);
        XYZ_32_ProjectPointOntoAxis(head_pos, axis, axis_len2, &pos_3);
    }

    const XYZ_32 delta = {
        .x = (pos_2.x - pos_1.x) >> 3,
        .y = (pos_2.y - pos_1.y) >> 3,
        .z = (pos_2.z - pos_1.z) >> 3,
    };

    XYZ_32 ideal_pos = pos_1;
    room_num = lara_item->room_num;

    int32_t i = 0;
    for (; i < M_LOS_STEPS; i++) {
        int16_t next_room_num = room_num;
        sector = Room_GetSector(
            (XYZ_32) { ideal_pos.x, ideal_pos.y + STEP_L, ideal_pos.z },
            &next_room_num);

        if (Room_Get(next_room_num)->flags.swamp) {
            ideal_pos.y = Room_Get(next_room_num)->max_ceiling - STEP_L;
            break;
        }

        sector = Room_GetSector(ideal_pos, &next_room_num);
        height = Room_GetHeight(sector, ideal_pos);
        ceiling = Room_GetCeiling(sector, ideal_pos);

        if (height == NO_HEIGHT || ceiling == NO_HEIGHT || ceiling >= height
            || ideal_pos.y > height || ideal_pos.y < ceiling) {
            break;
        }

        room_num = next_room_num;
        ideal_pos.x += delta.x;
        ideal_pos.y += delta.y;
        ideal_pos.z += delta.z;
    }

    if (i != 0) {
        ideal_pos.x -= delta.x;
        ideal_pos.y -= delta.y;
        ideal_pos.z -= delta.z;
    }

    GAME_VECTOR ideal = {
        .pos = ideal_pos,
        .room_num = room_num,
    };
    if (M_UpdateLaraState()) {
        m_LastLookIdeal.pos = ideal;
        m_LastLookIdeal.target.pos = pos_3;
    } else {
        ideal = m_LastLookIdeal.pos;
        pos_3 = m_LastLookIdeal.target.pos;
    }

    M_Collide(&ideal, M_LOOK_SHIFT, true);

    if (m_LastState.cam_type == CAM_FIXED) {
        g_Camera.pos.pos = ideal.pos;
        g_Camera.target.pos = pos_3;
    } else {
        g_Camera.pos.x += (ideal.x - g_Camera.pos.x) >> 2;
        g_Camera.pos.y += (ideal.y - g_Camera.pos.y) >> 2;
        g_Camera.pos.z += (ideal.z - g_Camera.pos.z) >> 2;
        g_Camera.target.x += (pos_3.x - g_Camera.target.x) >> 2;
        g_Camera.target.y += (pos_3.y - g_Camera.target.y) >> 2;
        g_Camera.target.z += (pos_3.z - g_Camera.target.z) >> 2;
    }

    g_Camera.target.room_num = lara_item->room_num;

    if (g_Camera.type == m_LastState.cam_type) {
        Camera_ApplyBounce();
    }

    Room_GetSector(g_Camera.pos.pos, &g_Camera.pos.room_num);
    ideal_pos = g_Camera.pos.pos;
    room_num = g_Camera.pos.room_num;
    sector = Room_GetSector(ideal_pos, &room_num);
    height = Room_GetHeight(sector, ideal_pos);
    ceiling = Room_GetCeiling(sector, ideal_pos);

    if (ceiling != NO_HEIGHT && height != NO_HEIGHT) {
        if (ceiling > ideal_pos.y - 255 && height < ideal_pos.y + 255
            && height > ceiling) {
            g_Camera.pos.y = (ceiling + height) >> 1;
        } else if (height < ideal_pos.y + 255 && height > ceiling) {
            g_Camera.pos.y = height - 255;
        } else if (ceiling > ideal_pos.y - 255 && height > ceiling) {
            g_Camera.pos.y = ceiling + 255;
        }
    }

    ideal_pos = g_Camera.pos.pos;
    room_num = g_Camera.pos.room_num;
    sector = Room_GetSector(ideal_pos, &room_num);
    height = Room_GetHeight(sector, ideal_pos);
    ceiling = Room_GetCeiling(sector, ideal_pos);

    if (Room_Get(room_num)->flags.swamp) {
        g_Camera.pos.y = Room_Get(room_num)->max_ceiling - STEP_L;
    } else if (
        ideal_pos.y < ceiling || ideal_pos.y > height || ceiling >= height
        || height == NO_HEIGHT || ceiling == NO_HEIGHT) {
        M_LOS(&g_Camera.target, &g_Camera.pos, 0);
    }

    ideal_pos = g_Camera.pos.pos;
    room_num = g_Camera.pos.room_num;
    sector = Room_GetSector(ideal_pos, &room_num);
    height = Room_GetHeight(sector, ideal_pos);
    ceiling = Room_GetCeiling(sector, ideal_pos);

    if (ideal_pos.y < ceiling || ideal_pos.y > height || ceiling >= height
        || height == NO_HEIGHT || ceiling == NO_HEIGHT
        || Room_Get(room_num)->flags.swamp) {
        g_Camera.pos.pos = pos_1;
        g_Camera.pos.room_num = lara_item->room_num;
    }

    Room_GetSector(g_Camera.pos.pos, &g_Camera.pos.room_num);
    m_LastState.cam_type = g_Camera.type;

    Camera_UpdateMicPosition();

    lara->head_rot = head_rot;
    lara->torso_rot = torso_rot;
}

static void M_ClampResult(void)
{
    XYZ_32 pos = g_Camera.interp.result.pos;
    int16_t room_num = g_Camera.interp.room_num;
    const SECTOR *const sector = Room_GetSector(pos, &room_num);
    const int32_t height = Room_GetHeightEx(sector, pos, true, NO_ITEM);
    const int32_t ceiling = Room_GetCeilingEx(sector, pos, true);
    if (height == NO_HEIGHT || ceiling == NO_HEIGHT || height < g_Camera.pos.y
        || ceiling > g_Camera.pos.y) {
        pos = g_Camera.pos.pos;
        room_num = g_Camera.pos.room_num;
    }
    Room_GetSector(pos, &room_num);
    g_Camera.interp.result.pos = pos;
    g_Camera.interp.room_num = room_num;
}

static void M_Reset(void)
{
    const ITEM *const lara_item = Lara_GetItem();
    ASSERT(lara_item != nullptr);
    g_Camera.shift = 0;
    m_LastIdeal.target.pos = lara_item->pos;
    m_LastIdeal.target.pos.y -= WALL_L;
    m_LastIdeal.target.room_num = lara_item->room_num;

    g_Camera.target = m_LastIdeal.target;
    g_Camera.pos = m_LastIdeal.target;
    g_Camera.pos.z -= 100;
}

static void M_Update(
    const ITEM *const item, const bool fixed_camera, int32_t target_y)
{
    Camera_SetChunky(false);
    if (g_Camera.type != CAM_LOOK) {
        m_LastIdeal.target = g_Camera.target;
    }

    const BOUNDS_16 *bounds = Item_GetBoundsAccurate(item);

    if (g_Camera.item != nullptr && !fixed_camera) {
        bounds = Item_GetBoundsAccurate(g_Camera.item);

        const int32_t dx = g_Camera.item->pos.x - item->pos.x;
        const int32_t dz = g_Camera.item->pos.z - item->pos.z;
        const int32_t shift = Math_Sqrt(SQUARE(dx) + SQUARE(dz));
        int16_t angle = Math_Atan(dz, dx) - item->rot.y;

        int16_t tilt = Math_Atan(
            shift,
            target_y - (bounds->min.y + bounds->max.y) / 2
                - g_Camera.item->pos.y);
        angle >>= 1;
        tilt >>= 1;

        if (angle > CAMERA_MIN_HEAD_ROTATION && angle < CAMERA_MAX_HEAD_ROTATION
            && tilt > CAMERA_MIN_HEAD_TILT && tilt < CAMERA_MAX_HEAD_TILT) {
            LARA_INFO *const lara_info = Lara_GetLaraInfo();
            int16_t change = angle - lara_info->head_rot.y;
            if (change > CAMERA_HEAD_TURN) {
                lara_info->head_rot.y += CAMERA_HEAD_TURN;
            } else if (change < -CAMERA_HEAD_TURN) {
                lara_info->head_rot.y -= CAMERA_HEAD_TURN;
            } else {
                lara_info->head_rot.y = angle;
            }

            change = tilt - lara_info->head_rot.x;
            if (change > CAMERA_HEAD_TURN) {
                lara_info->head_rot.x += CAMERA_HEAD_TURN;
            } else if (change < -CAMERA_HEAD_TURN) {
                lara_info->head_rot.x -= CAMERA_HEAD_TURN;
            } else {
                lara_info->head_rot.x = tilt;
            }
            lara_info->torso_rot.x = lara_info->head_rot.x;
            lara_info->torso_rot.y = lara_info->head_rot.y;
            g_Camera.type = CAM_LOOK;
            g_Camera.item->looked_at = true;
        }
    }

    if (g_Camera.type == CAM_LOOK || g_Camera.type == CAM_COMBAT) {
        target_y -= STEP_L;
        g_Camera.target.room_num = item->room_num;
        if (g_Camera.fixed_camera) {
            g_Camera.target.y = target_y;
            g_Camera.speed = 1;
        } else {
            g_Camera.target.y += (target_y - g_Camera.target.y) >> 2;
            g_Camera.speed = g_Camera.type == CAM_LOOK ? CAMERA_LOOK_SPEED
                                                       : CAMERA_COMBAT_SPEED;
        }
        g_Camera.fixed_camera = false;
        if (g_Camera.type == CAM_LOOK) {
            M_Look(item);
        } else {
            M_Combat(item);
        }
    } else {
        g_Camera.target.x = item->pos.x;
        g_Camera.target.z = item->pos.z;

        if (g_Camera.flags == CF_FOLLOW_CENTRE) {
            const int32_t shift = (bounds->min.z + bounds->max.z) / 2;
            g_Camera.target.z += (shift * Math_Cos(item->rot.y)) >> W2V_SHIFT;
            g_Camera.target.x += (shift * Math_Sin(item->rot.y)) >> W2V_SHIFT;
        }

        g_Camera.target.room_num = item->room_num;
        g_Camera.target.y = target_y;

        if (g_Camera.fixed_camera != fixed_camera) {
            g_Camera.fixed_camera = true;
            g_Camera.speed = 1;
        } else {
            g_Camera.fixed_camera = false;
        }

        if (g_Camera.speed != 1 && m_LastState.cam_type != CAM_LOOK) {
            g_Camera.target.x =
                ((g_Camera.target.x - m_LastIdeal.target.x) >> 2)
                + m_LastIdeal.target.x;
            g_Camera.target.y =
                ((g_Camera.target.y - m_LastIdeal.target.y) >> 2)
                + m_LastIdeal.target.y;
            g_Camera.target.z =
                ((g_Camera.target.z - m_LastIdeal.target.z) >> 2)
                + m_LastIdeal.target.z;
        }

        Room_GetSector(g_Camera.target.pos, &g_Camera.target.room_num);

        if (g_Camera.type == CAM_CHASE || g_Camera.flags == CF_CHASE_OBJECT) {
            M_Chase(item);
        } else {
            M_Fixed();
        }
    }
}

bool Camera_LOSCheck(
    GAME_VECTOR *const start, GAME_VECTOR *const target, const int32_t shift)
{
    return M_LOS(start, target, shift);
}

bool Camera_Collide(
    GAME_VECTOR *const ideal, const int32_t shift, const bool y_first)
{
    return M_Collide(ideal, shift, y_first);
}

static const CAMERA_STRATEGY m_Strategy = {
    .get_chase_speed_func = M_GetChaseSpeed,
    .get_look_settings_func = M_GetLookSettingsFunc,
    .clamp_result_func = M_ClampResult,
    .reset_func = M_Reset,
    .update_func = M_Update,
};

REGISTER_CAMERA(CAMERA_MODE_TR3, m_Strategy)
