#include <trx/game/objects/vehicles/kayak.h>

#include <trx/core/json/util/read_io.h>
#include <trx/core/json/util/write_io.h>
#include <trx/core/math.h>
#include <trx/core/utils.h>
#include <trx/game/camera.h>
#include <trx/game/collision.h>
#include <trx/game/fx/wake.h>
#include <trx/game/fx/water.h>
#include <trx/game/input.h>
#include <trx/game/items.h>
#include <trx/game/lara.h>
#include <trx/game/lara/skin/common.h>
#include <trx/game/matrix.h>
#include <trx/game/objects.h>
#include <trx/game/output.h>
#include <trx/game/random.h>
#include <trx/game/rooms.h>
#include <trx/game/sparks.h>
#include <trx/game/spawn.h>

typedef enum {
    M_STATE_BACK = 0,
    M_STATE_POSE = 1,
    M_STATE_LEFT = 2,
    M_STATE_RIGHT = 3,
    M_STATE_CLIMB_IN = 4,
    M_STATE_DEATH_IN = 5,
    M_STATE_FORWARD = 6,
    M_STATE_ROLL = 7,
    M_STATE_DROWN_IN = 8,
    M_STATE_JUMP_OUT = 9,
    M_STATE_TURN_L = 10,
    M_STATE_TURN_R = 11,
    M_STATE_CLIMB_IN_R = 12,
    M_STATE_CLIMB_OUT_L = 13,
    M_STATE_CLIMB_OUT_R = 14,
} M_STATE;

typedef struct {
    struct {
        bool frame2_latched;
        uint8_t stroke_count;
        bool equipped;
    } paddle;
    int32_t vel;
    int32_t rot;
    int32_t fall_speed_f;
    int32_t fall_speed_l;
    int32_t fall_speed_r;
    int32_t water;
    XYZ_32 old_pos;
    bool turn;
    bool forward;
    bool true_water;
    uint8_t counter;
} M_PRIV;

// clang-format off
// Hidden while mounting/in kayak to prevent clipping through the hull.
static const uint32_t m_KayakHiddenBodyMeshes =
    (1u << LM_HIPS)
    | (1u << LM_THIGH_L)
    | (1u << LM_CALF_L)
    | (1u << LM_FOOT_L)
    | (1u << LM_THIGH_R)
    | (1u << LM_CALF_R)
    | (1u << LM_FOOT_R);

static const XZ_16 m_MistPos[10] = {
    { .x = 32, .z = 900 },
    { .x = 96, .z = 750 },
    { .x = 170, .z = 600 },
    { .x = 220, .z = 450 },
    { .x = 300, .z = 300 },
    { .x = 400, .z = 150 },
    { .x = 400, .z = 0 },
    { .x = 300, .z = -150 },
    { .x = 200, .z = -300 },
    { .x = 64, .z = -450 },
};
// clang-format on

static void M_LoadPriv(ITEM *const item, JSON_READ_IO *const io)
{
    M_PRIV *const p = item->priv;
    JSON_SHOULD(JSON_READ(io, "vel", &p->vel));
    JSON_SHOULD(JSON_READ(io, "rot", &p->rot));
    JSON_SHOULD(JSON_READ(io, "fall_speed_f", &p->fall_speed_f));
    JSON_SHOULD(JSON_READ(io, "fall_speed_l", &p->fall_speed_l));
    JSON_SHOULD(JSON_READ(io, "fall_speed_r", &p->fall_speed_r));
    JSON_SHOULD(JSON_READ(io, "water", &p->water));
    JSON_SHOULD(JSON_READ(io, "old_pos", &p->old_pos));
    JSON_SHOULD(JSON_READ(io, "turn", &p->turn));
    JSON_SHOULD(JSON_READ(io, "forward", &p->forward));
    JSON_SHOULD(JSON_READ(io, "true_water", &p->true_water));
    JSON_SHOULD(JSON_READ(io, "counter", &p->counter));
    JSON_SHOULD(
        JSON_READ(io, "paddle_frame2_latched", &p->paddle.frame2_latched));
    JSON_SHOULD(JSON_READ(io, "paddle_stroke_count", &p->paddle.stroke_count));
    JSON_SHOULD(JSON_READ(io, "paddle_equipped", &p->paddle.equipped));
}

static void M_SavePriv(const ITEM *const item, JSON_WRITE_IO *const io)
{
    const M_PRIV *const p = item->priv;
    JSONW_WRITE(io, "vel", p->vel);
    JSONW_WRITE(io, "rot", p->rot);
    JSONW_WRITE(io, "fall_speed_f", p->fall_speed_f);
    JSONW_WRITE(io, "fall_speed_l", p->fall_speed_l);
    JSONW_WRITE(io, "fall_speed_r", p->fall_speed_r);
    JSONW_WRITE(io, "water", p->water);
    JSONW_WRITE(io, "old_pos", p->old_pos);
    JSONW_WRITE(io, "turn", p->turn);
    JSONW_WRITE(io, "forward", p->forward);
    JSONW_WRITE(io, "true_water", p->true_water);
    JSONW_WRITE(io, "counter", p->counter);
    JSONW_WRITE(io, "paddle_frame2_latched", p->paddle.frame2_latched);
    JSONW_WRITE(io, "paddle_stroke_count", p->paddle.stroke_count);
    JSONW_WRITE(io, "paddle_equipped", p->paddle.equipped);
}

static void M_Initialise(int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;
    p->rot = 0;
    p->vel = 0;
    p->fall_speed_r = 0;
    p->fall_speed_l = 0;
    p->fall_speed_f = 0;
    p->old_pos = item->pos;
    p->paddle.equipped = false;
    FX_Wake_Reset();
}

static int32_t M_GetInKayak(const int16_t item_num, const COLL_INFO *const coll)
{
    const LARA_INFO *const lara_info = Lara_GetLaraInfo();
    const ITEM *const lara_item = Lara_GetItem();
    if (!g_Input.action || lara_info->gun_status != LGS_ARMLESS
        || lara_item->gravity) {
        return 0;
    }

    const ITEM *const item = Item_Get(item_num);
    const int32_t dx = lara_item->pos.x - item->pos.x;
    const int32_t dz = lara_item->pos.z - item->pos.z;
    const int32_t dist = SQUARE(dx) + SQUARE(dz);
    if (dist > 130000) {
        return 0;
    }

    int16_t room_num = item->room_num;
    const SECTOR *const floor = Room_GetSector(item->pos, &room_num);
    const int32_t h = Room_GetHeight(floor, item->pos);
    if (h <= -32000) {
        return 0;
    }

    const int16_t ang =
        Math_Atan(
            item->pos.z - lara_item->pos.z, item->pos.x - lara_item->pos.x)
        - item->rot.y;
    const uint16_t temp_ang = lara_item->rot.y - item->rot.y;
    if (ang > -45 * DEG_1 && ang < 135 * DEG_1) {
        if (temp_ang > 45 * DEG_1 && temp_ang < 135 * DEG_1) {
            return -1;
        }
    } else {
        if (temp_ang > 225 * DEG_1 && temp_ang < 315 * DEG_1) {
            return 1;
        }
    }

    return 0;
}

static void M_Collision(
    const int16_t item_num, ITEM *const l, COLL_INFO *const coll)
{
    if (l->hit_points < 0 || Lara_Vehicle_GetIndex() != NO_ITEM) {
        return;
    }

    const int32_t lr = M_GetInKayak(item_num, coll);
    if (lr == 0) {
        coll->enable_baddie_push = 1;
        Object_Collision(item_num, l, coll);
        return;
    }

    Lara_Vehicle_SetIndex(item_num);
    ITEM *const item = Item_Get(item_num);

    LARA_INFO *const lara_info = Lara_GetLaraInfo();
    if (lara_info->gun_type == LGT_FLARE) {
        Lara_Flare_Dispose(false);
        lara_info->flare.control = false;
        lara_info->gun_type = LGT_UNARMED;
        lara_info->request_gun_type = LGT_UNARMED;
    }

    if (lr > 0) {
        Item_SwitchToObjAnim(l, 3, 0, O_LARA_VEHICLE_ANIM);
    } else {
        Item_SwitchToObjAnim(l, 28, 0, O_LARA_VEHICLE_ANIM);
    }
    l->current_anim_state = M_STATE_CLIMB_IN;
    l->goal_anim_state = M_STATE_CLIMB_IN;
    lara_info->water_status = LWS_ABOVE_WATER;
    l->pos = item->pos;
    l->rot.x = 0;
    l->rot.y = item->rot.y;
    l->rot.z = 0;
    l->gravity = false;
    l->speed = 0;
    l->fall_speed = 0;

    if (l->room_num != item->room_num) {
        Item_UpdateRoom(lara_info->item_num, item->room_num);
    }

    M_PRIV *p = item->priv;
    p->water = item->pos.y;
    p->paddle.equipped = false;
}

static void M_DoRipple(
    const ITEM *const item, const int16_t x_offset, const int16_t z_offset)
{
    XYZ_32 pos = item->pos;
    pos = XYZ_32_OffsetYaw(pos, item->rot.y, z_offset);
    pos = XYZ_32_OffsetYaw(pos, item->rot.y + DEG_90, x_offset);

    int16_t room_num = item->room_num;
    Room_GetSector(pos, &room_num);
    const int32_t wh = Room_GetWaterHeight(pos, room_num);
    if (wh == NO_HEIGHT) {
        return;
    }

    FX_WATER_RIPPLE *const ripple = FX_Water_SetupRipple(
        pos.x, pos.y, pos.z, -2 - (Random_GetControl() & 1), 0);
    if (ripple != nullptr) {
        ripple->init = 0;
    }
}

static int32_t M_TestHeight(
    const ITEM *const item, const int32_t x, const int32_t z, XYZ_32 *const pos)
{
    const int32_t zs = Math_Sin(item->rot.z);
    const int32_t xs = Math_Sin(item->rot.x);

    *pos = XYZ_32_OffsetYaw(item->pos, item->rot.y, z);
    *pos = XYZ_32_OffsetYaw(*pos, item->rot.y + DEG_90, x);
    pos->y = (item->pos.y + ((x * zs) >> W2V_SHIFT)) - ((z * xs) >> W2V_SHIFT);

    int16_t room_num = item->room_num;
    Room_GetSector(*pos, &room_num);
    int32_t h = Room_GetWaterHeight(*pos, room_num);

    if (h == NO_HEIGHT) {
        room_num = item->room_num;
        SECTOR *const floor = Room_GetSector(*pos, &room_num);
        h = Room_GetHeight(floor, *pos);
        if (h == NO_HEIGHT) {
            return h;
        }
    }

    return h - 5;
}

static bool M_CanGetOut(const ITEM *const item, const int32_t lr)
{
    XYZ_32 pos;
    const int32_t h = M_TestHeight(item, lr >= 0 ? 768 : -768, 0, &pos);
    return item->pos.y - h <= 0;
}

static void M_KayakUserInput(ITEM *const item, ITEM *const l, M_PRIV *const p)
{
    LARA_INFO *const lara_info = Lara_GetLaraInfo();

    if (l->hit_points <= 0 && l->current_anim_state != M_STATE_DEATH_IN) {
        Item_SwitchToObjAnim(l, 5, 0, O_LARA_VEHICLE_ANIM);
        l->current_anim_state = M_STATE_DEATH_IN;
        l->goal_anim_state = M_STATE_DEATH_IN;
    }

    const int16_t frame = Item_GetRelativeFrame(l);

    const int32_t time4 = Output_GetTimeInGame() * 4;
    switch (l->current_anim_state) {
    case M_STATE_BACK:
        if (!(g_Input.back)) {
            l->goal_anim_state = M_STATE_POSE;
        }

        if (l->anim_num - Object_Get(O_LARA_VEHICLE_ANIM)->anim_idx == 2) {
            if (frame == 8) {
                p->rot += 0x800000;
                p->vel -= 0x180000;
            }

            if (frame == 31) {
                p->rot -= 0x800000;
                p->vel -= 0x180000;
            }

            if (frame < 15 && (frame & 1) != 0) {
                M_DoRipple(item, 384, -128);
            } else if (frame >= 20 && frame <= 34 && frame & 1) {
                M_DoRipple(item, -384, -128);
            }
        }

        break;

    case M_STATE_POSE:
        if (g_Input.roll && lara_info->current.active == 0
            && lara_info->current.vel.x == 0 && lara_info->current.vel.z == 0) {
            if (g_Input.left && M_CanGetOut(item, -1)) {
                l->goal_anim_state = M_STATE_JUMP_OUT;
                l->required_anim_state = M_STATE_CLIMB_OUT_L;
            } else if (g_Input.right && M_CanGetOut(item, 1)) {
                l->goal_anim_state = M_STATE_JUMP_OUT;
                l->required_anim_state = M_STATE_CLIMB_OUT_R;
            }
        } else if (g_Input.forward) {
            l->goal_anim_state = M_STATE_RIGHT;
            p->turn = false;
            p->forward = true;
        } else if (g_Input.back) {
            l->goal_anim_state = M_STATE_BACK;
        } else if (g_Input.left) {
            l->goal_anim_state = M_STATE_LEFT;

            if (p->vel) {
                p->turn = false;
            } else {
                p->turn = true;
            }

            p->forward = false;
        } else if (g_Input.right) {
            l->goal_anim_state = M_STATE_RIGHT;

            if (p->vel) {
                p->turn = false;
            } else {
                p->turn = true;
            }

            p->forward = false;
        } else if (
            g_Input.step_left
            && (p->vel || lara_info->current.vel.x
                || lara_info->current.vel.z)) {
            l->goal_anim_state = M_STATE_TURN_L;
        } else if (
            g_Input.step_right
            && (p->vel || lara_info->current.vel.x
                || lara_info->current.vel.z)) {
            l->goal_anim_state = M_STATE_TURN_R;
        }

        break;

    case M_STATE_LEFT:
        if (!p->forward) {
            if (!g_Input.left) {
                l->goal_anim_state = M_STATE_POSE;
            }
        } else {
            if (frame == 0) {
                p->paddle.frame2_latched = false;
                p->paddle.stroke_count = 0;
            }

            if (frame == 2 && !p->paddle.frame2_latched) {
                p->paddle.frame2_latched = true;
                p->paddle.stroke_count++;
            } else if (frame > 2) {
                p->paddle.frame2_latched = false;
            }

            if (!g_Input.forward) {
                l->goal_anim_state = M_STATE_POSE;
            } else if (!g_Input.left) {
                l->goal_anim_state = M_STATE_RIGHT;
            } else if (p->paddle.stroke_count >= 2) {
                l->goal_anim_state = M_STATE_RIGHT;
            }
        }

        if (frame == 7) {
            if (p->forward) {
                p->rot -= 0x800000;
                CLAMPL(p->rot, -0x1000000);
                p->vel += 0x180000;
            } else if (p->turn) {
                p->rot -= 0x1000000;
                CLAMPL(p->rot, -0x1000000);
            } else {
                p->rot -= 0xC00000;
                CLAMPL(p->rot, -0xC00000);
                p->vel += 0x100000;
            }
        }

        if (frame > 6 && frame < 24 && frame & 1) {
            M_DoRipple(item, -384, -64);
        }
        break;

    case M_STATE_RIGHT:
        if (!p->forward) {
            if (!g_Input.right) {
                l->goal_anim_state = M_STATE_POSE;
            }
        } else {
            if (frame == 0) {
                p->paddle.frame2_latched = false;
                p->paddle.stroke_count = 0;
            }

            if (frame == 2 && !p->paddle.frame2_latched) {
                p->paddle.frame2_latched = true;
                p->paddle.stroke_count++;
            } else if (frame > 2) {
                p->paddle.frame2_latched = false;
            }

            if (!g_Input.forward) {
                l->goal_anim_state = M_STATE_POSE;
            } else if (!g_Input.right) {
                l->goal_anim_state = M_STATE_LEFT;
            } else if (p->paddle.stroke_count >= 2) {
                l->goal_anim_state = M_STATE_LEFT;
            }
        }

        if (frame == 7) {
            if (p->forward) {
                p->rot += 0x800000;
                CLAMPG(p->rot, 0x1000000);
                p->vel += 0x180000;
            } else if (p->turn) {
                p->rot += 0x1000000;
                CLAMPG(p->rot, 0x1000000);
            } else {
                p->rot += 0xC00000;
                CLAMPG(p->rot, 0xC00000);
                p->vel += 0x100000;
            }
        }

        if (frame > 6 && frame < 24 && frame & 1) {
            M_DoRipple(item, 384, -64);
        }
        break;

    case M_STATE_CLIMB_IN:
        if (l->anim_num == Object_Get(O_LARA_VEHICLE_ANIM)->anim_idx + 4
            && frame == 24 && !p->paddle.equipped) {
            Lara_Skin_SetExtraEquipment(LM_HAND_R, EXTRA_MESH_OAR);
            Item_SetMeshVisibleMask(l, m_KayakHiddenBodyMeshes, false);
            p->paddle.equipped = true;
        }
        break;

    case M_STATE_JUMP_OUT:
        if (l->anim_num == Object_Get(O_LARA_VEHICLE_ANIM)->anim_idx + 14
            && frame == 27 && p->paddle.equipped) {
            Lara_Skin_ClearEquipment(LM_HAND_R);
            Item_SetMeshVisibleMask(l, m_KayakHiddenBodyMeshes, true);
            p->paddle.equipped = false;
        }
        l->goal_anim_state = l->required_anim_state;
        break;

    case M_STATE_TURN_L:
        if (!g_Input.step_left
            || (!p->vel && !lara_info->current.vel.x
                && !lara_info->current.vel.z)) {
            l->goal_anim_state = M_STATE_POSE;
        } else if (
            l->anim_num - Object_Get(O_LARA_VEHICLE_ANIM)->anim_idx == 26) {
            if (p->vel >= 0) {
                p->rot -= 0x200000;
                CLAMPL(p->rot, -0x1000000);
                p->vel -= 0x8000;
                CLAMPL(p->vel, 0);
            }

            if (p->vel < 0) {
                p->vel += 0x8000;
                p->rot += 0x200000;
                CLAMPG(p->vel, 0);
            }

            if (!(time4 & 3)) {
                M_DoRipple(item, -256, -256);
            }
        }
        break;

    case M_STATE_TURN_R:
        if (!g_Input.step_right
            || (!p->vel && !lara_info->current.vel.x
                && !lara_info->current.vel.z)) {
            l->goal_anim_state = M_STATE_POSE;
        } else if (
            l->anim_num - Object_Get(O_LARA_VEHICLE_ANIM)->anim_idx == 27) {
            if (p->vel >= 0) {
                p->rot += 0x200000;
                CLAMPG(p->rot, 0x1000000);
                p->vel -= 0x8000;
                CLAMPL(p->vel, 0);
            }

            if (p->vel < 0) {
                p->vel += 0x8000;
                p->rot -= 0x200000;
                CLAMPG(p->vel, 0);
            }

            if (!(time4 & 3)) {
                M_DoRipple(item, 256, -256);
            }
        }
        break;

    case M_STATE_CLIMB_OUT_L:
        if (l->anim_num == Object_Get(O_LARA_VEHICLE_ANIM)->anim_idx + 24
            && frame == 83) {
            XYZ_32 pos = { .x = 0, .y = 350, .z = 500 };
            Lara_GetMeshPos(LM_HIPS, &pos);
            l->pos = pos;
            l->rot.x = 0;
            l->rot.y = item->rot.y - 0x4000;
            l->rot.z = 0;
            Item_SwitchToAnim(l, LA(LA_FREEFALL), 0);
            l->current_anim_state = LS_FAST_FALL;
            l->goal_anim_state = LS_FAST_FALL;
            l->gravity = true;
            l->fall_speed = 0;
            lara_info->gun_status = LGS_ARMLESS;
            Lara_Vehicle_SetIndex(NO_ITEM);
        }
        break;

    case M_STATE_CLIMB_OUT_R:
        if (l->anim_num == Object_Get(O_LARA_VEHICLE_ANIM)->anim_idx + 32
            && frame == 83) {
            XYZ_32 pos = { .x = 0, .y = 350, .z = 500 };
            Lara_GetMeshPos(LM_HIPS, &pos);
            l->pos = pos;
            l->rot.x = 0;
            l->rot.y = item->rot.y + 0x4000;
            l->rot.z = 0;
            Item_SwitchToAnim(l, LA(LA_FREEFALL), 0);
            l->current_anim_state = LS_FAST_FALL;
            l->goal_anim_state = LS_FAST_FALL;
            l->gravity = true;
            l->fall_speed = 0;
            lara_info->gun_status = LGS_ARMLESS;
            Lara_Vehicle_SetIndex(NO_ITEM);
        }
        break;
    }

    if (p->vel > 0) {
        p->vel -= 0x8000;
        CLAMPL(p->vel, 0);
    } else if (p->vel < 0) {
        p->vel += 0x8000;
        CLAMPG(p->vel, 0);
    }

    CLAMP(p->vel, -0x380000, 0x380000);
    item->speed = p->vel >> 16;

    if (p->rot < 0) {
        p->rot += 0x50000;
        CLAMPG(p->rot, 0);
    } else {
        p->rot -= 0x50000;
        CLAMPL(p->rot, 0);
    }
}

static void M_DoCurrent(ITEM *const item)
{
    LARA_INFO *const lara_info = Lara_GetLaraInfo();
    const ITEM *const lara_item = Lara_GetItem();

    if (lara_info->current.active != 0) {
        const OBJECT_VECTOR *const sink =
            Camera_GetFixedObject(lara_info->current.active - 1);
        const int32_t speed = sink->data;
        const int32_t angle =
            -Math_Atan(lara_item->pos.x - sink->x, lara_item->pos.z - sink->z)
            - DEG_90;
        const int32_t scaled_speed = speed << (W2V_SHIFT - 4);
        const XYZ_32 target_vel =
            XYZ_32_OffsetYaw((XYZ_32) {}, angle, scaled_speed);
        lara_info->current.vel.x +=
            (target_vel.x - lara_info->current.vel.x) >> 4;
        lara_info->current.vel.z +=
            (target_vel.z - lara_info->current.vel.z) >> 4;
    } else {
        int32_t shifter;
        int32_t abs_vel;

        abs_vel = ABS(lara_info->current.vel.x);
        if (abs_vel > 16) {
            shifter = 4;
        } else if (abs_vel > 8) {
            shifter = 3;
        } else {
            shifter = 2;
        }

        lara_info->current.vel.x -= lara_info->current.vel.x >> shifter;
        if (ABS(lara_info->current.vel.x) < 4) {
            lara_info->current.vel.x = 0;
        }

        abs_vel = ABS(lara_info->current.vel.z);
        if (abs_vel > 16) {
            shifter = 4;
        } else if (abs_vel > 8) {
            shifter = 3;
        } else {
            shifter = 2;
        }

        lara_info->current.vel.z -= lara_info->current.vel.z >> shifter;
        if (ABS(lara_info->current.vel.z) < 4) {
            lara_info->current.vel.z = 0;
        }

        if (lara_info->current.vel.x == 0 && lara_info->current.vel.z == 0) {
            return;
        }
    }

    item->pos.x += lara_info->current.vel.x >> 8;
    item->pos.z += lara_info->current.vel.z >> 8;
    lara_info->current.active = 0;
}

static int32_t M_DoDynamics(
    const int32_t h, int32_t fall_speed, int32_t *const y)
{
    if (h <= *y) {
        int32_t kick = (h - *y) << 2;
        CLAMPL(kick, -80);
        fall_speed += (kick - fall_speed) >> 3;
        CLAMPG(*y, h);
    } else {
        *y += fall_speed;

        if (*y <= h) {
            fall_speed += 6;
        } else {
            *y = h;
            fall_speed = 0;
        }
    }

    return fall_speed;
}

static int32_t M_DoShift(
    ITEM *const item, const XYZ_32 *const new_pos, XYZ_32 *const old_pos)
{
    const int32_t new_sector_x = new_pos->x >> WALL_SHIFT;
    const int32_t new_sector_z = new_pos->z >> WALL_SHIFT;
    const int32_t old_sector_x = old_pos->x >> WALL_SHIFT;
    const int32_t old_sector_z = old_pos->z >> WALL_SHIFT;
    const int32_t sector_offset_x = new_pos->x - ROUND_TO_SECTOR(new_pos->x);
    const int32_t sector_offset_z = new_pos->z - ROUND_TO_SECTOR(new_pos->z);
    int32_t shift_x = 0;
    int32_t shift_z = 0;

    if (new_sector_x == old_sector_x) {
        if (new_sector_z == old_sector_z) {
            item->pos.z += old_pos->z - new_pos->z;
            item->pos.x += old_pos->x - new_pos->x;
            return 0;
        } else if (new_sector_z <= old_sector_z) {
            item->pos.z += WALL_L - sector_offset_z;
            return item->pos.x - new_pos->x;
        } else {
            item->pos.z -= 1 + sector_offset_z;
            return new_pos->x - item->pos.x;
        }
    }

    if (new_sector_z == old_sector_z) {
        if (new_sector_x <= old_sector_x) {
            item->pos.x += WALL_L - sector_offset_x;
            return new_pos->z - item->pos.z;
        } else {
            item->pos.x -= 1 + sector_offset_x;
            return item->pos.z - new_pos->z;
        }
    }

    int16_t room_num = item->room_num;
    XYZ_32 sample_pos = { old_pos->x, new_pos->y, new_pos->z };
    const SECTOR *floor = Room_GetSector(sample_pos, &room_num);
    int32_t height = Room_GetHeight(floor, sample_pos);
    if (height < old_pos->y - 256) {
        if (new_pos->z > old_pos->z) {
            shift_z = -1 - sector_offset_z;
        } else {
            shift_z = WALL_L - sector_offset_z;
        }
    }

    room_num = item->room_num;
    sample_pos = (XYZ_32) { new_pos->x, new_pos->y, old_pos->z };
    floor = Room_GetSector(sample_pos, &room_num);
    height = Room_GetHeight(floor, sample_pos);
    if (height < old_pos->y - 256) {
        if (new_pos->x > old_pos->x) {
            shift_x = -1 - sector_offset_x;
        } else {
            shift_x = WALL_L - sector_offset_x;
        }
    }

    if (shift_x != 0 && shift_z != 0) {
        item->pos.x += shift_x;
        item->pos.z += shift_z;
        return 0;
    }

    if (shift_z != 0) {
        item->pos.z += shift_z;
        if (shift_z > 0) {
            return item->pos.x - new_pos->x;
        } else {
            return new_pos->x - item->pos.x;
        }
    }

    if (shift_x != 0) {
        item->pos.x += shift_x;
        if (shift_x > 0) {
            return new_pos->z - item->pos.z;
        } else {
            return item->pos.z - new_pos->z;
        }
    }

    item->pos.x += old_pos->x - new_pos->x;
    item->pos.z += old_pos->z - new_pos->z;
    return 0;
}

static int32_t M_GetCollisionAnim(const ITEM *const item, int32_t x, int32_t z)
{
    x = item->pos.x - x;
    z = item->pos.z - z;
    if (x == 0 && z == 0) {
        return 0;
    }

    const int32_t s = Math_Sin(item->rot.y);
    const int32_t c = Math_Cos(item->rot.y);
    const int32_t front = (x * s + z * c) >> W2V_SHIFT;
    const int32_t side = (x * c - z * s) >> W2V_SHIFT;
    if (ABS(front) <= ABS(side)) {
        if (side > 0) {
            return 3;
        } else {
            return 4;
        }
    } else {
        if (front > 0) {
            return 1;
        } else {
            return 2;
        }
    }
}

static void M_KayakToBackground(ITEM *const item, M_PRIV *const p)
{
    int32_t heights[8];
    XYZ_32 old_pos[9];
    p->old_pos = item->pos;
    heights[0] = M_TestHeight(item, 0, 1024, old_pos);
    heights[1] = M_TestHeight(item, -96, 512, &old_pos[1]);
    heights[2] = M_TestHeight(item, 96, 512, &old_pos[2]);
    heights[3] = M_TestHeight(item, -128, 128, &old_pos[3]);
    heights[4] = M_TestHeight(item, 128, 128, &old_pos[4]);
    heights[5] = M_TestHeight(item, -128, -320, &old_pos[5]);
    heights[6] = M_TestHeight(item, 128, -320, &old_pos[6]);
    heights[7] = M_TestHeight(item, 0, -640, &old_pos[7]);

    for (int32_t i = 0; i < 8; i++) {
        CLAMPG(old_pos[i].y, heights[i]);
    }

    old_pos[8] = item->pos;

    XYZ_32 front_pos;
    XYZ_32 left_pos;
    XYZ_32 right_pos;
    const int32_t front = M_TestHeight(item, 0, 1024, &front_pos);
    const int32_t left = M_TestHeight(item, -128, 128, &left_pos);
    const int32_t right = M_TestHeight(item, 128, 128, &right_pos);

    item->rot.y += p->rot >> 16;
    const XYZ_32 moved_pos =
        XYZ_32_OffsetYaw(item->pos, item->rot.y, item->speed);
    item->pos.x = moved_pos.x;
    item->pos.z = moved_pos.z;

    M_DoCurrent(item);
    p->fall_speed_l = M_DoDynamics(left, p->fall_speed_l, &left_pos.y);
    p->fall_speed_r = M_DoDynamics(right, p->fall_speed_r, &right_pos.y);
    p->fall_speed_f = M_DoDynamics(front, p->fall_speed_f, &front_pos.y);
    item->fall_speed = M_DoDynamics(p->water, item->fall_speed, &item->pos.y);

    const int32_t pitch_diff = (right_pos.y + left_pos.y) >> 1;
    const int16_t x_rot = Math_Atan(1024, item->pos.y - front_pos.y);
    const int16_t z_rot = Math_Atan(128, pitch_diff - left_pos.y);
    item->rot.x = x_rot;
    item->rot.z = z_rot;
    const int32_t old_x = item->pos.x;
    const int32_t old_z = item->pos.z;

    XYZ_32 pos;
    int32_t rot = 0;

    int32_t h = M_TestHeight(item, 0, -640, &pos);
    if (h < old_pos[7].y - 64) {
        rot = M_DoShift(item, &pos, &old_pos[7]);
    }

    h = M_TestHeight(item, 128, -320, &pos);
    if (h < old_pos[6].y - 64) {
        rot += M_DoShift(item, &pos, &old_pos[6]);
    }

    h = M_TestHeight(item, -128, -320, &pos);
    if (h < old_pos[5].y - 64) {
        rot += M_DoShift(item, &pos, &old_pos[5]);
    }

    h = M_TestHeight(item, 128, 128, &pos);
    if (h < old_pos[4].y - 64) {
        rot += M_DoShift(item, &pos, &old_pos[4]);
    }

    h = M_TestHeight(item, -128, 128, &pos);
    if (h < old_pos[3].y - 64) {
        rot += M_DoShift(item, &pos, &old_pos[3]);
    }

    h = M_TestHeight(item, 96, 512, &pos);
    if (h < old_pos[2].y - 64) {
        rot += M_DoShift(item, &pos, &old_pos[2]);
    }

    h = M_TestHeight(item, -96, 512, &pos);
    if (h < old_pos[1].y - 64) {
        rot += M_DoShift(item, &pos, &old_pos[1]);
    }

    h = M_TestHeight(item, 0, 1024, &pos);
    if (h < old_pos[0].y - 64) {
        rot += M_DoShift(item, &pos, &old_pos[0]);
    }

    item->rot.y += rot;

    int16_t room_num = item->room_num;
    const SECTOR *floor = Room_GetSector(item->pos, &room_num);
    h = Room_GetWaterHeight(item->pos, room_num);
    if (h == NO_HEIGHT) {
        h = Room_GetHeight(floor, item->pos);
    }
    if (h < item->pos.y - 64) {
        h = M_DoShift(item, (XYZ_32 *)&item->pos, &old_pos[8]);
    }

    room_num = item->room_num;
    floor = Room_GetSector(item->pos, &room_num);
    h = Room_GetWaterHeight(item->pos, room_num);
    if (h == NO_HEIGHT) {
        h = Room_GetHeight(floor, item->pos);
        if (h == NO_HEIGHT) {
            GAME_VECTOR reset_pos = {
                .pos = p->old_pos,
                .room_num = item->room_num,
            };
            Camera_Collide(&reset_pos, 256, 0);
            item->pos = reset_pos.pos;
            item->room_num = reset_pos.room_num;
        }
    }

    if (M_GetCollisionAnim(item, old_x, old_z)) {
        const int32_t sin_y = Math_Sin(item->rot.y);
        const int32_t cos_y = Math_Cos(item->rot.y);
        const int32_t dx = item->pos.x - old_pos[8].x;
        const int32_t dz = item->pos.z - old_pos[8].z;
        int32_t speed = (dx * sin_y + dz * cos_y) >> W2V_SHIFT;
        speed <<= 8;

        if ((p->vel > 0 && speed < p->vel) || (p->vel < 0 && speed > p->vel)) {
            p->vel = speed;
        }

        CLAMPL(p->vel, -0x380000);
    }
}

static void M_KayakSplash(
    const ITEM *const item, const int32_t fall_speed, const int32_t water)
{
    if (water == NO_HEIGHT) {
        return;
    }
    FX_WATER_SPLASH_SETUP splash_setup = {
        .x = item->pos.x,
        .y = item->pos.y,
        .z = item->pos.z,
        .inner_xz_off = 128,
        .inner_xz_size = 48,
        .inner_y_size = -384,
        .inner_xz_vel = 160,
        .inner_y_vel = (-fall_speed << 5),
        .inner_gravity = 128,
        .inner_friction = 7,
        .middle_xz_off = 192,
        .middle_xz_size = 96,
        .middle_y_size = -256,
        .middle_xz_vel = 224,
        .middle_y_vel = (-fall_speed << 4),
        .middle_gravity = 72,
        .middle_friction = 8,
        .outer_xz_off = 256,
        .outer_xz_size = 128,
        .outer_xz_vel = 272,
        .outer_friction = -9,
    };
    FX_Water_SetupSplash(&splash_setup);
}

static void M_DoWake(
    const ITEM *const item, const int32_t xoff, const int32_t zoff,
    const int16_t rotate)
{
    int16_t angle1;
    int16_t angle2;
    XYZ_32 pos;

    const int32_t start_idx = FX_Wake_GetStartIndex();
    if (FX_Wake_GetPoint(start_idx, rotate)->life != 0) {
        return;
    }

    pos = XYZ_32_OffsetYaw(item->pos, item->rot.y, zoff);
    pos = XYZ_32_OffsetYaw(pos, item->rot.y + DEG_90, xoff);
    int16_t room_num = item->room_num;
    Room_GetSector(pos, &room_num);
    const int32_t wh = Room_GetWaterHeight(pos, room_num);

    if (wh == NO_HEIGHT) {
        return;
    }

    if (item->speed >= 0) {
        if (rotate) {
            angle1 = item->rot.y + 30940;
            angle2 = item->rot.y + 27300;
        } else {
            angle1 = item->rot.y - 30940;
            angle2 = item->rot.y - 27300;
        }
    } else {
        if (rotate) {
            angle1 = item->rot.y + 1820;
            angle2 = item->rot.y + 5460;
        } else {
            angle1 = item->rot.y - 1820;
            angle2 = item->rot.y - 5460;
        }
    }

    XYZ_32 vel[2] = {
        XYZ_32_OffsetYaw((XYZ_32) {}, angle1, 4),
        XYZ_32_OffsetYaw((XYZ_32) {}, angle2, 6),
    };
    FX_WAKE_POINT *const pt = FX_Wake_GetPoint(start_idx, rotate);
    pt->life = 64;

    for (int32_t i = 0; i < 2; i++) {
        pt->pos[i].x = pos.x;
        pt->pos[i].y = item->pos.y + 32;
        pt->pos[i].z = pos.z;
        pt->prev_pos[i] = pt->pos[i];
        pt->vel[i].x = vel[i].x;
        pt->vel[i].z = vel[i].z;
    }

    if (rotate == 1) {
        FX_Wake_AdvanceStartIndex();
    }
}

static void M_TriggerRapidsMist(const XYZ_32 pos)
{
    SPARK *const spark = Sparks_GetFreeSpark();
    spark->on = true;
    spark->src_color.r = 128;
    spark->src_color.g = 128;
    spark->src_color.b = 128;
    spark->dst_color.r = 192;
    spark->dst_color.g = 192;
    spark->dst_color.b = 192;

    spark->col_fade_speed = 2;
    spark->fade_to_black = 4;
    spark->draw_type = DRAW_BLEND_ADD;
    spark->extras = 0;
    spark->life = (Random_GetControl() & 3) + 6;
    spark->s_life = spark->life;
    spark->dynamic = -1;
    spark->pos.x = pos.x + (Random_GetControl() & 0xF) - 8;
    spark->pos.y = pos.y + (Random_GetControl() & 0xF) - 8;
    spark->pos.z = pos.z + (Random_GetControl() & 0xF) - 8;
    spark->vel.x = (Random_GetControl() & 0xFF) - 128;
    spark->vel.y = (Random_GetControl() & 0xFF) - 128;
    spark->vel.z = (Random_GetControl() & 0xFF) - 128;
    spark->friction = 3;

    if (Random_GetControl() & 1) {
        spark->flags = SPARK_F_ALT_SPRITE | SPARK_F_ROTATE | SPARK_F_SPRITE
            | SPARK_F_SCALE;
        spark->rot_angle = Random_GetControl() & 0xFFF;

        if (Random_GetControl() & 1) {
            spark->rot_add = -16 - (Random_GetControl() & 0xF);
        } else {
            spark->rot_add = (Random_GetControl() & 0xF) + 16;
        }
    } else {
        spark->flags = SPARK_F_ALT_SPRITE | SPARK_F_SPRITE | SPARK_F_SCALE;
    }

    spark->scalar = 4;
    spark->sprite_idx = (uint8_t)Object_Get(O_EXPLOSION_1)->mesh_idx;
    spark->gravity = 0;
    spark->max_y_vel = 0;
    spark->dst_size.width = (Random_GetControl() & 7) + 16;
    spark->src_size.width = spark->dst_size.width >> 1;
    spark->size.width = spark->src_size.width;
    spark->src_size.height = spark->src_size.width;
    spark->size.height = spark->src_size.width;
    spark->dst_size.height = spark->dst_size.width;
    Sparks_FinishSetup(spark);
}

static void M_KayakToBaddieCollision(const ITEM *const p)
{
    const ITEM *const lara_item = Lara_GetItem();
    int16_t nearby_rooms[20] = { p->room_num };
    int16_t nearby_room_count = 1;

    const PORTALS *const portals = Room_Get(p->room_num)->portals;
    if (portals != nullptr) {
        for (int32_t i = 0; i < portals->count && nearby_room_count < 20; i++) {
            nearby_rooms[nearby_room_count] = portals->portal[i].room_num;
            nearby_room_count++;
        }
    }

    for (int32_t i = 0; i < nearby_room_count; i++) {
        const ITEM *item;
        for (int16_t item_num = Room_Get(nearby_rooms[i])->item_num;
             item_num != NO_ITEM; item_num = item->next_item) {
            item = Item_Get(item_num);

            if (!item->collidable || item->status == IS_INVISIBLE) {
                continue;
            }

            const OBJECT_ID obj_num = item->object_id;
            const OBJECT *const obj = Object_Get(obj_num);
            if (obj->collision_func == nullptr) {
                continue;
            }

            const bool is_hazard = obj_num == O_SPIKES || obj_num == O_DART
                || obj_num == O_TEETH_TRAP
                || (obj_num == O_BLADE && item->current_anim_state != 1)
                || (obj_num == O_ICICLE && item->current_anim_state != 3);
            if (!is_hazard) {
                continue;
            }

            const int32_t dx = p->pos.x - item->pos.x;
            const int32_t dy = p->pos.y - item->pos.y;
            const int32_t dz = p->pos.z - item->pos.z;
            if (dx <= -2048 || dx >= 2048 || dz <= -2048 || dz >= 2048
                || dy <= -2048 || dy >= 2048) {
                continue;
            }

            if (!Item_TestBoundsCollide(item, p, 256)) {
                continue;
            }

            Spawn_BloodBath(
                lara_item->pos.x, lara_item->pos.y - 256, lara_item->pos.z,
                p->speed, p->rot.y, lara_item->room_num, 3);
            Lara_TakeDamage(5, false);
        }
    }
}

static bool M_Draw(const ITEM *const item)
{
    ((ITEM *)item)->pos.y += 32;
    Object_DrawAnimatingItem(item);
    ((ITEM *)item)->pos.y -= 32;
    FX_Wake_Draw(item);
    return true;
}

static void M_Setup(OBJECT *const obj)
{
    obj->initialise_func = M_Initialise;
    obj->priv_size = sizeof(M_PRIV);
    obj->priv_load_func = M_LoadPriv;
    obj->priv_save_func = M_SavePriv;
    obj->collision_func = M_Collision;
    obj->draw_func = M_Draw;
    obj->save_position = true;
    obj->save_flags = true;
    obj->save_anim = true;
}

bool Kayak_Control(void)
{
    LARA_INFO *const lara_info = Lara_GetLaraInfo();
    ITEM *const lara_item = Lara_GetItem();
    ITEM *const item = Lara_Vehicle_GetItem();
    M_PRIV *const p = item->priv;
    const int32_t time4 = Output_GetTimeInGame() * 4;

    const LARA_SKIN_EQUIPMENT *const hand_r_equipment =
        Lara_Skin_GetEquipment(LM_HAND_R);
    if (p->paddle.equipped) {
        Lara_Skin_SetExtraEquipment(LM_HAND_R, EXTRA_MESH_OAR);
    } else if (
        hand_r_equipment->type == EQUIPMENT_TYPE_EXTRA
        && hand_r_equipment->data == EXTRA_MESH_OAR) {
        Lara_Skin_ClearEquipment(LM_HAND_R);
    }

    if (g_Input.look) {
        Lara_Look_UpDown();
    }

    const int32_t old_fall_speed = item->fall_speed;
    M_KayakUserInput(item, lara_item, p);
    M_KayakToBackground(item, p);
    int16_t room_num = item->room_num;
    const SECTOR *floor = Room_GetSector(item->pos, &room_num);
    int32_t h = Room_GetHeight(floor, item->pos);
    int32_t wh = Room_GetWaterHeight(item->pos, room_num);
    p->water = wh;

    if (wh == NO_HEIGHT) {
        wh = h;
        p->water = h;
        p->true_water = false;
    } else {
        p->true_water = true;
        p->water = wh - 5;
    }

    const int32_t damage = old_fall_speed - item->fall_speed;

    if (damage > 128 && !item->fall_speed && wh != NO_HEIGHT) {
        if (damage > 160) {
            Lara_TakeDamage((damage - 160) << 3, false);
        }

        M_KayakSplash(item, old_fall_speed - item->fall_speed, wh);
    }

    if (Lara_Vehicle_GetIndex() != NO_ITEM) {
        lara_item->pos.x = item->pos.x;
        lara_item->pos.y = item->pos.y + 32;
        lara_item->pos.z = item->pos.z;
        lara_item->rot.x = item->rot.x;
        lara_item->rot.y = item->rot.y;
        lara_item->rot.z = item->rot.z >> 1;
        Item_Animate(lara_item);
        item->anim_num = lara_item->anim_num + Object_Get(O_KAYAK)->anim_idx
            - Object_Get(O_LARA_VEHICLE_ANIM)->anim_idx;
        item->frame_num = lara_item->frame_num + Item_GetAnim(item)->frame_base
            - Item_GetAnim(lara_item)->frame_base;
        Item_UpdateRoom(Lara_Vehicle_GetIndex(), room_num);
        Item_UpdateRoom(lara_info->item_num, room_num);
        Room_TestTriggers(lara_item);
        Room_TestTriggers(item);
        g_Camera.target_elevation = -5460;
        g_Camera.target_distance = 2048;
    }

    if (!(time4 & 0xF) && p->true_water) {
        M_DoWake(item, -128, 0, 0);
        M_DoWake(item, 128, 0, 1);
    }

    if (time4 & 7 && !p->true_water && item->fall_speed < 20) {
        p->counter ^= 1;

        for (int32_t i = p->counter; i < 10; i += 2) {
            int32_t x;
            if (Random_GetControl() & 1) {
                x = m_MistPos[i].x >> 1;
            } else {
                x = -(m_MistPos[i].x >> 1);
            }

            const int32_t y = 50;
            const int32_t z = m_MistPos[i].z;

            Matrix_PushUnit();
            Matrix_Rot16(item->rot);

            // NOTE: not part of the OG PC
            const XYZ_32 pos =
                Matrix_MulVec32_M(g_MatrixPtr, (XYZ_32) { x, y, z });
            M_TriggerRapidsMist((XYZ_32) {
                .x = item->pos.x + pos.x,
                .y = item->pos.y + pos.y,
                .z = item->pos.z + pos.z,
            });

            Matrix_Pop();
        }
    }

    uint8_t wake_shade = FX_Wake_GetShade();
    if (item->speed != 0 || lara_info->current.vel.x != 0
        || lara_info->current.vel.z != 0) {
        if (wake_shade < 16) {
            wake_shade++;
        }
    } else if (wake_shade) {
        wake_shade--;
    }
    FX_Wake_SetShade(wake_shade);

    M_KayakToBaddieCollision(item);

    return Lara_Vehicle_GetIndex() != NO_ITEM;
}

REGISTER_OBJECT(O_KAYAK, M_Setup)
