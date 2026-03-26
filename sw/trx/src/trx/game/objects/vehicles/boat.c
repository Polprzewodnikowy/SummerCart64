#include <trx/config.h>
#include <trx/core/json/util/read_io.h>
#include <trx/core/json/util/write_io.h>
#include <trx/core/math.h>
#include <trx/core/strings.h>
#include <trx/core/utils.h>
#include <trx/game/camera.h>
#include <trx/game/collision.h>
#include <trx/game/effects.h>
#include <trx/game/input.h>
#include <trx/game/lara.h>
#include <trx/game/matrix.h>
#include <trx/game/objects/common.h>
#include <trx/game/objects/traps/gondola.h>
#include <trx/game/objects/vehicles/common.h>
#include <trx/game/output.h>
#include <trx/game/random.h>
#include <trx/game/sound.h>
#include <trx/game/spawn.h>

#define BOAT_FALL_ANIM 15
#define BOAT_DEATH_ANIM 18
#define BOAT_GET_ON_LW_ANIM 0
#define BOAT_GET_ON_RW_ANIM 8
#define BOAT_GET_ON_J_ANIM 6
#define BOAT_GET_ON_START 1

#define LF_BOAT_EXIT_END 24

#define BOAT_RADIUS 500
#define BOAT_SIDE 300
#define BOAT_FRONT 750
#define BOAT_TIP (BOAT_FRONT + 250)
#define BOAT_MIN_SPEED 20
#define BOAT_MAX_SPEED 90
#define BOAT_SLOW_SPEED (BOAT_MAX_SPEED / 3) // = 30
#define BOAT_FAST_SPEED (BOAT_MAX_SPEED + 50) // = 140
#define BOAT_MAX_BACK (-20)
#define BOAT_ACCELERATION 5
#define BOAT_BRAKE 5
#define BOAT_REVERSE (-5)
#define BOAT_SLOWDOWN 1
#define BOAT_WAKE 700
#define BOAT_UNDO_TURN (DEG_1 / 4) // = 45
#define BOAT_TURN (DEG_1 / 8) // = 22
#define BOAT_MAX_TURN (DEG_1 * 4) // = 728
#define BOAT_SOUND_CEILING (WALL_L * 5) // = 5120
#define BOAT_SHIFT_Y (-5)

typedef enum {
    BOAT_STATE_GET_ON = 0,
    BOAT_STATE_STILL = 1,
    BOAT_STATE_MOVING = 2,
    BOAT_STATE_JUMP_R = 3,
    BOAT_STATE_JUMP_L = 4,
    BOAT_STATE_HIT = 5,
    BOAT_STATE_FALL = 6,
    BOAT_STATE_DEATH = 8,
} BOAT_STATE;

typedef struct {
    int32_t boat_turn;
    int32_t left_fallspeed;
    int32_t right_fallspeed;
    int16_t tilt_angle;
    int16_t extra_rotation;
    int32_t water;
    int32_t pitch;
} M_PRIV;

static void M_LoadPriv(ITEM *const item, JSON_READ_IO *const io)
{
    M_PRIV *const p = item->priv;
    JSON_SHOULD(JSON_READ(io, "boat_turn", &p->boat_turn));
    JSON_SHOULD(JSON_READ(io, "left_fallspeed", &p->left_fallspeed));
    JSON_SHOULD(JSON_READ(io, "right_fallspeed", &p->right_fallspeed));
    JSON_SHOULD(JSON_READ(io, "tilt_angle", &p->tilt_angle));
    JSON_SHOULD(JSON_READ(io, "extra_rotation", &p->extra_rotation));
    JSON_SHOULD(JSON_READ(io, "water", &p->water));
    JSON_SHOULD(JSON_READ(io, "pitch", &p->pitch));
}

static void M_SavePriv(const ITEM *const item, JSON_WRITE_IO *const io)
{
    const M_PRIV *const p = item->priv;
    JSONW_WRITE(io, "boat_turn", p->boat_turn);
    JSONW_WRITE(io, "left_fallspeed", p->left_fallspeed);
    JSONW_WRITE(io, "right_fallspeed", p->right_fallspeed);
    JSONW_WRITE(io, "tilt_angle", p->tilt_angle);
    JSONW_WRITE(io, "extra_rotation", p->extra_rotation);
    JSONW_WRITE(io, "water", p->water);
    JSONW_WRITE(io, "pitch", p->pitch);
}

static int32_t M_CheckGetOn(const int16_t item_num, const COLL_INFO *const coll)
{
    const LARA_INFO *const lara = Lara_GetLaraInfo();
    if (lara->gun_status != LGS_ARMLESS) {
        return 0;
    }

    ITEM *const boat_item = Item_Get(item_num);
    const ITEM *const lara_item = Lara_GetItem();
    const int32_t dist =
        ((lara_item->pos.z - boat_item->pos.z) * Math_Cos(-boat_item->rot.y)
         - (lara_item->pos.x - boat_item->pos.x) * Math_Sin(-boat_item->rot.y))
        >> W2V_SHIFT;

    if (dist > 200) {
        return 0;
    }

    int32_t get_on = 0;
    const int16_t rot = boat_item->rot.y - lara_item->rot.y;

    if (lara->water_status == LWS_SURFACE || lara->water_status == LWS_WADE) {
        if (!g_Input.action || lara_item->gravity || boat_item->speed) {
            return 0;
        }

        if (rot > DEG_45 && rot < DEG_135) {
            get_on = 1;
        } else if (rot > -DEG_135 && rot < -DEG_45) {
            get_on = 2;
        }
    } else if (lara->water_status == LWS_ABOVE_WATER) {
        int16_t fall_speed = lara_item->fall_speed;
        if (fall_speed > 0) {
            if (rot > -DEG_135 && rot < DEG_135
                && lara_item->pos.y > boat_item->pos.y) {
                get_on = 3;
            }
        } else if (!fall_speed && rot > -DEG_135 && rot < DEG_135) {
            if (lara_item->pos.x == boat_item->pos.x
                && lara_item->pos.y == boat_item->pos.y
                && lara_item->pos.z == boat_item->pos.z) {
                get_on = 4;
            } else {
                get_on = 3;
            }
        }
    }

    if (!get_on) {
        return 0;
    }

    if (!Item_TestBoundsCollide(boat_item, lara_item, coll->radius)) {
        return 0;
    }

    if (!Collide_TestCollision(boat_item, lara_item)) {
        return 0;
    }

    return get_on;
}

static int32_t M_TestWaterHeight(
    const ITEM *const item, const int32_t z_off, const int32_t x_off,
    XYZ_32 *const pos)
{
    // clang-format off
    pos->y = item->pos.y
        + ((x_off * Math_Sin(item->rot.z)) >> W2V_SHIFT)
        - ((z_off * Math_Sin(item->rot.x)) >> W2V_SHIFT);
    // clang-format on

    const int32_t c = Math_Cos(item->rot.y);
    const int32_t s = Math_Sin(item->rot.y);
    pos->x = item->pos.x + ((x_off * c + z_off * s) >> W2V_SHIFT);
    pos->z = item->pos.z + ((z_off * c - x_off * s) >> W2V_SHIFT);

    int16_t room_num = item->room_num;
    Room_GetSector(*pos, &room_num);
    int32_t height = Room_GetWaterHeight(*pos, room_num);
    if (height == NO_HEIGHT) {
        const SECTOR *const sector = Room_GetSector(*pos, &room_num);
        height = Room_GetHeight(sector, *pos);
        if (height != NO_HEIGHT) {
            return height;
        }
    }

    return height + BOAT_SHIFT_Y;
}

static void M_DoWakeEffect(const ITEM *const boat_item)
{
    g_MatrixPtr->_23 = 0;
    g_WMatrixPtr->_23 = 0;
    Output_CalculateLight(boat_item->pos, boat_item->room_num);

    const int16_t frame =
        (Random_GetDraw() * Object_Get(O_WATER_SPRITE)->mesh_count) >> 15;

    for (int32_t i = 0; i < 3; i++) {
        const int16_t effect_num = Effect_Create(boat_item->room_num);
        if (effect_num == NO_EFFECT) {
            continue;
        }

        EFFECT *const effect = Effect_Get(effect_num);
        effect->object_id = O_WATER_SPRITE;
        effect->room_num = boat_item->room_num;
        effect->frame_num = frame;

        const int32_t c = Math_Cos(boat_item->rot.y);
        const int32_t s = Math_Sin(boat_item->rot.y);
        const int32_t w = (1 - i) * BOAT_SIDE;
        const int32_t h = BOAT_WAKE;
        effect->pos.x = boat_item->pos.x + ((-c * w - s * h) >> W2V_SHIFT);
        effect->pos.y = boat_item->pos.y;
        effect->pos.z = boat_item->pos.z + ((-c * h + s * w) >> W2V_SHIFT);
        effect->rot.y = boat_item->rot.y + (i << W2V_SHIFT) - DEG_90;

        effect->counter = 20;
        effect->speed = boat_item->speed >> 2;
        if (boat_item->speed < 64) {
            effect->fall_speed =
                (Random_GetDraw() * (ABS(boat_item->speed) - 64)) >> 15;
        } else {
            effect->fall_speed = 0;
        }

        effect->shade = Output_GetLightAdder() - 768;
        CLAMPL(effect->shade, 0);
    }
}

static void M_DoShift(const int32_t boat_num)
{
    ITEM *const boat_item = Item_Get(boat_num);
    int16_t item_num = Room_Get(boat_item->room_num)->item_num;

    while (item_num != NO_ITEM) {
        ITEM *const item = Item_Get(item_num);

        if (item->object_id == O_BOAT && item_num != boat_num
            && Lara_Vehicle_GetIndex() != item_num) {
            const int32_t dx = item->pos.x - boat_item->pos.x;
            const int32_t dz = item->pos.z - boat_item->pos.z;
            const int32_t dist = SQUARE(dx) + SQUARE(dz);

            if (dist < SQUARE(BOAT_RADIUS * 2)) {
                boat_item->pos.x =
                    item->pos.x - SQUARE(BOAT_RADIUS * 2) * dx / dist;
                boat_item->pos.z =
                    item->pos.z - SQUARE(BOAT_RADIUS * 2) * dz / dist;
            }
            break;
        }

        if (item->object_id == O_GONDOLA
            && item->current_anim_state == GONDOLA_STATE_FLOATING) {
            const int32_t c = Math_Cos(item->rot.y);
            const int32_t s = Math_Sin(item->rot.y);
            const int32_t ix = item->pos.x - ((s * STEP_L * 2) >> W2V_SHIFT);
            const int32_t iz = item->pos.z - ((c * STEP_L * 2) >> W2V_SHIFT);
            const int32_t dx = ix - boat_item->pos.x;
            const int32_t dz = iz - boat_item->pos.z;
            const int32_t dist = SQUARE(dx) + SQUARE(dz);

            if (dist < SQUARE(BOAT_RADIUS * 2)) {
                if (boat_item->speed < BOAT_MAX_SPEED - 10) {
                    boat_item->pos.x = ix - SQUARE(BOAT_RADIUS * 2) * dx / dist;
                    boat_item->pos.z = iz - SQUARE(BOAT_RADIUS * 2) * dz / dist;
                } else if (item->pos.y - boat_item->pos.y < WALL_L * 2) {
                    Sound_Effect(SFX_BOAT_INTO_WATER, &item->pos, SPM_NORMAL);
                    item->goal_anim_state = GONDOLA_STATE_CRASH;
                }
            }
        }

        item_num = item->next_item;
    }
}

static int32_t M_DoDynamics(
    const int32_t height, int32_t fall_speed, int32_t *const y)
{
    if (height > *y) {
        *y = fall_speed + *y;
        if (*y > height) {
            *y = height;
            fall_speed = 0;
        } else {
            fall_speed += GRAVITY;
        }
    } else {
        fall_speed += ((height - fall_speed - *y) >> 3);
        CLAMPL(fall_speed, -20);
        CLAMPG(*y, height);
    }

    return fall_speed;
}

static int32_t M_Dynamics(const int16_t boat_num)
{
    ITEM *const boat_item = Item_Get(boat_num);
    M_PRIV *const p = boat_item->priv;
    boat_item->rot.z -= p->tilt_angle;

    XYZ_32 fl_old;
    XYZ_32 bl_old;
    XYZ_32 fr_old;
    XYZ_32 br_old;
    XYZ_32 f_old;
    const int32_t hfl_old =
        M_TestWaterHeight(boat_item, BOAT_FRONT, -BOAT_SIDE, &fl_old);
    const int32_t hfr_old =
        M_TestWaterHeight(boat_item, BOAT_FRONT, BOAT_SIDE, &fr_old);
    const int32_t hbl_old =
        M_TestWaterHeight(boat_item, -BOAT_FRONT, -BOAT_SIDE, &bl_old);
    const int32_t hbr_old =
        M_TestWaterHeight(boat_item, -BOAT_FRONT, BOAT_SIDE, &br_old);
    const int32_t hf_old = M_TestWaterHeight(boat_item, BOAT_TIP, 0, &f_old);
    XYZ_32 old = boat_item->pos;
    CLAMPG(bl_old.y, hbl_old);
    CLAMPG(br_old.y, hbr_old);
    CLAMPG(fl_old.y, hfl_old);
    CLAMPG(fr_old.y, hfr_old);
    CLAMPG(f_old.y, hf_old);

    boat_item->rot.y += p->extra_rotation + p->boat_turn;
    p->tilt_angle = p->boat_turn * 6;

    boat_item->pos.z +=
        (boat_item->speed * Math_Cos(boat_item->rot.y)) >> W2V_SHIFT;
    boat_item->pos.x +=
        (boat_item->speed * Math_Sin(boat_item->rot.y)) >> W2V_SHIFT;

    int32_t slip = (Math_Sin(boat_item->rot.z) * 30) >> W2V_SHIFT;
    if (!slip && boat_item->rot.z) {
        slip = boat_item->rot.z > 0 ? 1 : -1;
    }
    boat_item->pos.z -= (slip * Math_Sin(boat_item->rot.y)) >> W2V_SHIFT;
    boat_item->pos.x += (slip * Math_Cos(boat_item->rot.y)) >> W2V_SHIFT;

    slip = (Math_Sin(boat_item->rot.x) * 10) >> W2V_SHIFT;
    if (!slip && boat_item->rot.x) {
        slip = boat_item->rot.x > 0 ? 1 : -1;
    }

    boat_item->pos.z -= (slip * Math_Cos(boat_item->rot.y)) >> W2V_SHIFT;
    boat_item->pos.x =
        boat_item->pos.x - ((slip * Math_Sin(boat_item->rot.y)) >> W2V_SHIFT);

    XYZ_32 moved = {
        .x = boat_item->pos.x,
        .y = 0,
        .z = boat_item->pos.z,
    };
    M_DoShift(boat_num);

    int32_t rot = 0;

    XYZ_32 bl;
    const int32_t hbl =
        M_TestWaterHeight(boat_item, -BOAT_FRONT, -BOAT_SIDE, &bl);
    if (hbl < bl_old.y - STEP_L / 2) {
        rot = Vehicle_DoShift(boat_item, &bl, &bl_old);
    }

    XYZ_32 br;
    const int32_t hbr =
        M_TestWaterHeight(boat_item, -BOAT_FRONT, BOAT_SIDE, &br);
    if (hbr < br_old.y - STEP_L / 2) {
        rot += Vehicle_DoShift(boat_item, &br, &br_old);
    }

    XYZ_32 fl;
    const int32_t hfl =
        M_TestWaterHeight(boat_item, BOAT_FRONT, -BOAT_SIDE, &fl);
    if (hfl < fl_old.y - STEP_L / 2) {
        rot += Vehicle_DoShift(boat_item, &fl, &fl_old);
    }

    XYZ_32 fr;
    const int32_t hfr =
        M_TestWaterHeight(boat_item, BOAT_FRONT, BOAT_SIDE, &fr);
    if (hfr < fr_old.y - STEP_L / 2) {
        rot += Vehicle_DoShift(boat_item, &fr, &fr_old);
    }

    if (!slip) {
        XYZ_32 f;
        const int32_t hf = M_TestWaterHeight(boat_item, BOAT_TIP, 0, &f);
        if (hf < f_old.y - STEP_L / 2) {
            Vehicle_DoShift(boat_item, &f, &f_old);
        }
    }

    int16_t room_num = boat_item->room_num;
    const SECTOR *const sector = Room_GetSector(boat_item->pos, &room_num);
    int32_t height = Room_GetWaterHeight(boat_item->pos, room_num);
    if (height == NO_HEIGHT) {
        height = Room_GetHeight(sector, boat_item->pos);
    }
    if (height < boat_item->pos.y - STEP_L / 2) {
        Vehicle_DoShift(boat_item, &boat_item->pos, &old);
    }

    p->extra_rotation = rot;

    const int32_t collide = Vehicle_GetCollisionAnim(boat_item, &moved);
    if (slip || collide) {
        // clang-format off
        const int32_t new_speed = (
            (boat_item->pos.z - old.z) * Math_Cos(boat_item->rot.y) +
            (boat_item->pos.x - old.x) * Math_Sin(boat_item->rot.y)
        ) >> W2V_SHIFT;
        // clang-format on

        if (Lara_Vehicle_GetIndex() == boat_num) {
            if (boat_item->speed > BOAT_MAX_SPEED + BOAT_ACCELERATION
                && new_speed < boat_item->speed - 10) {
                Lara_TakeDamage((boat_item->speed - new_speed) / 2, true);
                Sound_Effect(SFX_LARA_INJURY, &Lara_GetItem()->pos, SPM_NORMAL);
            }
        }

        if (slip) {
            if (boat_item->speed <= BOAT_MAX_SPEED + 10) {
                boat_item->speed = new_speed;
            }
        } else {
            if (boat_item->speed > 0 && new_speed < boat_item->speed) {
                boat_item->speed = new_speed;
            } else if (boat_item->speed < 0 && new_speed > boat_item->speed) {
                boat_item->speed = new_speed;
            }
        }

        CLAMPL(boat_item->speed, BOAT_MAX_BACK);
    }

    return collide;
}

static int32_t M_UserControl(ITEM *const boat_item)
{
    int32_t no_turn = 1;

    M_PRIV *const p = boat_item->priv;
    if (boat_item->pos.y < p->water - STEP_L / 2 || p->water == NO_HEIGHT) {
        return no_turn;
    }

    if (g_Input.look && boat_item->speed == 0) {
        Lara_Look_UpDown();
        return no_turn;
    }

    if (g_Input.jump) {
        return no_turn;
    }

    const bool look =
        g_Input.look && g_Config.gameplay.look_mode != LOOK_MODE_RESTRICTED;
    const bool left_input = g_Input.left && !look;
    const bool right_input = g_Input.right && !look;

    if ((left_input && !g_Input.back) || (right_input && g_Input.back)) {
        if (p->boat_turn > 0) {
            p->boat_turn -= BOAT_UNDO_TURN;
        } else {
            p->boat_turn -= BOAT_TURN;
            CLAMPL(p->boat_turn, -BOAT_MAX_TURN);
        }
        no_turn = 0;
    } else if ((right_input && !g_Input.back) || (left_input && g_Input.back)) {
        if (p->boat_turn < 0) {
            p->boat_turn += BOAT_UNDO_TURN;
        } else {
            p->boat_turn += BOAT_TURN;
            CLAMPG(p->boat_turn, BOAT_MAX_TURN);
        }
        no_turn = 0;
    }

    if (g_Input.back) {
        if (boat_item->speed > 0) {
            boat_item->speed -= BOAT_BRAKE;
        } else if (boat_item->speed > BOAT_MAX_BACK) {
            boat_item->speed += BOAT_REVERSE;
        }
    } else if (g_Input.forward) {
        int32_t max_speed;
        if (g_Input.action) {
            max_speed = BOAT_FAST_SPEED;
        } else {
            max_speed = g_Input.slow ? BOAT_SLOW_SPEED : BOAT_MAX_SPEED;
        }

        if (boat_item->speed < max_speed) {
            boat_item->speed += BOAT_ACCELERATION / 2
                + BOAT_ACCELERATION * boat_item->speed / (2 * max_speed);
        } else if (boat_item->speed > max_speed + BOAT_SLOWDOWN) {
            boat_item->speed -= BOAT_SLOWDOWN;
        }
    } else if (
        boat_item->speed >= 0 && boat_item->speed < BOAT_MIN_SPEED
        && (left_input || right_input)) {
        boat_item->speed = BOAT_MIN_SPEED;
    } else if (boat_item->speed > BOAT_SLOWDOWN) {
        boat_item->speed -= BOAT_SLOWDOWN;
    } else {
        boat_item->speed = 0;
    }

    return no_turn;
}

static void M_Animation(const ITEM *const boat_item, const int32_t collide)
{
    ITEM *const lara_item = Lara_GetItem();
    const M_PRIV *const p = boat_item->priv;

    if (lara_item->hit_points <= 0) {
        if (lara_item->current_anim_state == BOAT_STATE_DEATH) {
            return;
        }
        Item_SwitchToObjAnim(lara_item, BOAT_DEATH_ANIM, 0, O_LARA_BOAT);
        lara_item->goal_anim_state = BOAT_STATE_DEATH;
        lara_item->current_anim_state = BOAT_STATE_DEATH;
        return;
    }

    if (boat_item->pos.y < p->water - STEP_L / 2 && boat_item->fall_speed > 0) {
        if (lara_item->current_anim_state == BOAT_STATE_FALL) {
            return;
        }
        Item_SwitchToObjAnim(lara_item, BOAT_FALL_ANIM, 0, O_LARA_BOAT);
        lara_item->goal_anim_state = BOAT_STATE_FALL;
        lara_item->current_anim_state = BOAT_STATE_FALL;
        return;
    }

    if (collide) {
        if (lara_item->current_anim_state == BOAT_STATE_HIT) {
            return;
        }
        Item_SwitchToObjAnim(lara_item, collide, 0, O_LARA_BOAT);
        lara_item->goal_anim_state = BOAT_STATE_HIT;
        lara_item->current_anim_state = BOAT_STATE_HIT;
        return;
    }

    switch (lara_item->current_anim_state) {
    case BOAT_STATE_STILL:
        if (g_Input.jump) {
            if (g_Input.right) {
                lara_item->goal_anim_state = BOAT_STATE_JUMP_R;
            } else if (g_Input.left) {
                lara_item->goal_anim_state = BOAT_STATE_JUMP_L;
            }
        }

        if (boat_item->speed > 0) {
            lara_item->goal_anim_state = BOAT_STATE_MOVING;
        }
        break;

    case BOAT_STATE_MOVING:
        if (g_Input.jump) {
            if (g_Input.right) {
                lara_item->goal_anim_state = BOAT_STATE_JUMP_R;
            } else if (g_Input.left) {
                lara_item->goal_anim_state = BOAT_STATE_JUMP_L;
            }
        } else if (boat_item->speed <= 0) {
            lara_item->goal_anim_state = BOAT_STATE_STILL;
        }
        break;

    case BOAT_STATE_FALL:
        lara_item->goal_anim_state = BOAT_STATE_MOVING;
        break;
    }
}

static void M_Initialise(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;
    p->boat_turn = 0;
    p->left_fallspeed = 0;
    p->right_fallspeed = 0;
    p->tilt_angle = 0;
    p->extra_rotation = 0;
    p->water = 0;
    p->pitch = 0;
}

static void M_Collision(
    const int16_t item_num, ITEM *const lara_item, COLL_INFO *const coll)
{
    if (lara_item->hit_points < 0 || Lara_Vehicle_IsMounted()) {
        return;
    }

    const int32_t get_on = M_CheckGetOn(item_num, coll);
    if (!get_on) {
        coll->enable_baddie_push = 1;
        Object_Collision(item_num, lara_item, coll);
        return;
    }

    Lara_Vehicle_SetIndex(item_num);

    int16_t boat_anim_idx;
    switch (get_on) {
    case 1:
        boat_anim_idx = BOAT_GET_ON_RW_ANIM;
        break;
    case 2:
        boat_anim_idx = BOAT_GET_ON_LW_ANIM;
        break;
    case 3:
        boat_anim_idx = BOAT_GET_ON_J_ANIM;
        break;
    default:
        boat_anim_idx = BOAT_GET_ON_START;
        break;
    }

    Item_SwitchToObjAnim(lara_item, boat_anim_idx, 0, O_LARA_BOAT);

    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->water_status = LWS_ABOVE_WATER;
    lara->hit_direction = -1;

    ITEM *const boat_item = Item_Get(item_num);

    lara_item->pos.x = boat_item->pos.x;
    lara_item->pos.y = boat_item->pos.y + BOAT_SHIFT_Y;
    lara_item->pos.z = boat_item->pos.z;
    lara_item->gravity = false;
    lara_item->rot.x = 0;
    lara_item->rot.y = boat_item->rot.y;
    lara_item->rot.z = 0;
    lara_item->speed = 0;
    lara_item->fall_speed = 0;
    lara_item->goal_anim_state = 0;
    lara_item->current_anim_state = 0;

    Item_UpdateRoom(lara->item_num, boat_item->room_num);

    Item_Animate(lara_item);
    if (boat_item->status != IS_ACTIVE) {
        Item_AddActive(item_num);
        boat_item->status = IS_ACTIVE;
    }
}

static void M_Control(const int16_t item_num)
{
    ITEM *const lara_item = Lara_GetItem();
    const LARA_INFO *const lara = Lara_GetLaraInfo();

    ITEM *const boat_item = Item_Get(item_num);
    M_PRIV *const p = boat_item->priv;

    bool drive = false;
    int32_t no_turn = 1;
    int32_t collide = M_Dynamics(item_num);

    XYZ_32 fl;
    XYZ_32 fr;
    const int32_t hfl =
        M_TestWaterHeight(boat_item, BOAT_FRONT, -BOAT_SIDE, &fl);
    const int32_t hfr =
        M_TestWaterHeight(boat_item, BOAT_FRONT, BOAT_SIDE, &fr);

    int16_t room_num = boat_item->room_num;
    const SECTOR *sector = Room_GetSector(
        (XYZ_32) {
            boat_item->pos.x,
            boat_item->pos.y + BOAT_SHIFT_Y,
            boat_item->pos.z,
        },
        &room_num);
    int32_t height = Room_GetHeight(sector, boat_item->pos);
    const int32_t ceiling = Room_GetCeiling(sector, boat_item->pos);

    const int32_t water_height = Room_GetWaterHeight(boat_item->pos, room_num);
    p->water = water_height;

    if (Lara_Vehicle_GetIndex() == item_num && lara_item->hit_points > 0) {
        switch (lara_item->current_anim_state) {
        case BOAT_STATE_GET_ON:
        case BOAT_STATE_JUMP_R:
        case BOAT_STATE_JUMP_L:
            break;

        default:
            drive = true;
            no_turn = M_UserControl(boat_item);
            break;
        }
    } else if (boat_item->speed > BOAT_SLOWDOWN) {
        boat_item->speed -= BOAT_SLOWDOWN;
    } else {
        boat_item->speed = 0;
    }

    if (no_turn) {
        if (p->boat_turn < -BOAT_UNDO_TURN) {
            p->boat_turn += BOAT_UNDO_TURN;
        } else if (p->boat_turn > BOAT_UNDO_TURN) {
            p->boat_turn -= BOAT_UNDO_TURN;
        } else {
            p->boat_turn = 0;
        }
    }

    boat_item->floor = height + BOAT_SHIFT_Y;
    if (p->water == NO_HEIGHT) {
        p->water = height;
    } else {
        p->water -= 5;
    }

    p->left_fallspeed = M_DoDynamics(hfl, p->left_fallspeed, &fl.y);
    p->right_fallspeed = M_DoDynamics(hfr, p->right_fallspeed, &fr.y);
    boat_item->fall_speed =
        M_DoDynamics(p->water, boat_item->fall_speed, &boat_item->pos.y);

    height = (fr.y + fl.y) / 2;

    const int16_t x_rot = Math_Atan(BOAT_FRONT, boat_item->pos.y - height);
    const int16_t z_rot = Math_Atan(BOAT_SIDE, height - fl.y);
    boat_item->rot.x += (x_rot - boat_item->rot.x) / 2;
    boat_item->rot.z += (z_rot - boat_item->rot.z) / 2;

    if (x_rot == 0 && ABS(boat_item->rot.x) < 4) {
        boat_item->rot.x = 0;
    }
    if (z_rot == 0 && ABS(boat_item->rot.z) < 4) {
        boat_item->rot.z = 0;
    }

    if (Lara_Vehicle_GetIndex() == item_num) {
        M_Animation(boat_item, collide);

        Item_UpdateRoom(item_num, room_num);

        boat_item->rot.z += p->tilt_angle;
        lara_item->pos.x = boat_item->pos.x;
        lara_item->pos.y = boat_item->pos.y;
        lara_item->pos.z = boat_item->pos.z;
        lara_item->rot.x = boat_item->rot.x;
        lara_item->rot.y = boat_item->rot.y;
        lara_item->rot.z = boat_item->rot.z;
        Room_TestTriggers(lara_item);
        Room_TestTriggers(boat_item);

        sector = Room_GetSector(
            (XYZ_32) {
                lara_item->pos.x,
                lara_item->pos.y + BOAT_SHIFT_Y,
                lara_item->pos.z,
            },
            &room_num);
        Item_UpdateRoom(lara->item_num, room_num);

        Item_Animate(lara_item);

        if (lara_item->hit_points > 0) {
            const int16_t lara_anim_num =
                Item_GetRelativeObjAnim(lara_item, O_LARA_BOAT);
            const int16_t lara_frame_num = Item_GetRelativeFrame(lara_item);
            Item_SwitchToAnim(boat_item, lara_anim_num, lara_frame_num);
        }

        g_Camera.target_elevation = -20 * DEG_1;
        g_Camera.target_distance = 2 * WALL_L;
    } else {
        Item_UpdateRoom(item_num, room_num);
        boat_item->rot.z += p->tilt_angle;
    }

    const int32_t pitch = water_height - ceiling < BOAT_SOUND_CEILING
        ? boat_item->speed * (water_height - ceiling) / BOAT_SOUND_CEILING
        : boat_item->speed;

    p->pitch += ((pitch - p->pitch) >> 2);
    if (boat_item->speed != 0
        && water_height + BOAT_SHIFT_Y != boat_item->pos.y) {
        Sound_Effect(SFX_BOAT_ENGINE, &boat_item->pos, SPM_NORMAL);
    } else if (boat_item->speed > 20) {
        Sound_Effect(
            SFX_BOAT_MOVING, &boat_item->pos,
            SPM_PITCH | ((0x10000 - (BOAT_MAX_SPEED - p->pitch) * 100) << 8));

    } else if (drive) {
        Sound_Effect(
            SFX_BOAT_IDLE, &boat_item->pos,
            SPM_PITCH | ((0x10000 - (BOAT_MAX_SPEED - p->pitch) * 100) << 8));
    }

    if (boat_item->speed && water_height + BOAT_SHIFT_Y == boat_item->pos.y) {
        M_DoWakeEffect(boat_item);
    }

    if (Lara_Vehicle_GetIndex() != item_num) {
        return;
    }

    if ((lara_item->current_anim_state == BOAT_STATE_JUMP_R
         || lara_item->current_anim_state == BOAT_STATE_JUMP_L)
        && Item_TestFrameEqual(lara_item, LF_BOAT_EXIT_END)) {
        if (lara_item->current_anim_state == BOAT_STATE_JUMP_L) {
            lara_item->rot.y -= DEG_90;
        } else {
            lara_item->rot.y += DEG_90;
        }

        Item_SwitchToAnim(lara_item, LA(LA_JUMP_FORWARD), 0);
        lara_item->goal_anim_state = LS(LS_JUMP_FORWARD);
        lara_item->current_anim_state = LS(LS_JUMP_FORWARD);
        lara_item->gravity = true;
        lara_item->rot.x = 0;
        lara_item->rot.z = 0;
        lara_item->speed = 20;
        lara_item->fall_speed = -40;
        Lara_Vehicle_SetIndex(NO_ITEM);

        const XYZ_32 pos = {
            .x = lara_item->pos.x
                + ((360 * Math_Sin(lara_item->rot.y)) >> W2V_SHIFT),
            .y = lara_item->pos.y - 90,
            .z = lara_item->pos.z
                + ((360 * Math_Cos(lara_item->rot.y)) >> W2V_SHIFT),
        };

        room_num = lara_item->room_num;
        sector = Room_GetSector(pos, &room_num);
        if (Room_GetHeight(sector, pos) >= pos.y - STEP_L) {
            lara_item->pos.x = pos.x;
            lara_item->pos.z = pos.z;
            Item_UpdateRoom(lara->item_num, room_num);
        }

        lara_item->pos.y = pos.y;
        Item_SwitchToAnim(boat_item, 0, 0);
    }
}

static void M_Setup(OBJECT *const obj)
{
    obj->initialise_func = M_Initialise;
    obj->control_func = M_Control;
    obj->collision_func = M_Collision;
    obj->priv_size = sizeof(M_PRIV);
    obj->priv_load_func = M_LoadPriv;
    obj->priv_save_func = M_SavePriv;
    obj->save_position = true;
    obj->save_flags = true;
    obj->save_anim = true;
}

REGISTER_OBJECT(O_BOAT, M_Setup)
