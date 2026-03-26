#include <trx/core/utils.h>
#include <trx/game/camera.h>
#include <trx/game/input.h>
#include <trx/game/lara.h>
#include <trx/game/lara/util.h>
#include <trx/game/rooms.h>

// clang-format off
#define M_MONKEY_RADIUS           100
#define M_MONKEY_HEIGHT           600
#define M_MONKEY_CEILING_SHIFT    50
#define M_MONKEY_FALL_FRAME       9
#define M_CAM_MONKEY_ELEVATION   (10 * DEG_1) // = 1820
#define M_LF_CLIMB_1_START       54
#define M_LF_CLIMB_1_END         60
#define M_LF_CLIMB_2_START       78
#define M_LF_CLIMB_2_END         84
#define M_LF_CLIMB_3_START       102
#define M_LF_CLIMB_3_END         155
// clang-format on

static bool M_CanMonkeySwing(const ITEM *const item)
{
    int16_t room_num = item->room_num;
    const SECTOR *const sector = Room_GetSector(
        (XYZ_32) { item->pos.x, MAX_HEIGHT, item->pos.z }, &room_num);
    return (sector->ladder & LADDER_CEILING) != 0;
}

static bool M_CanClimb(const ITEM *const item, const COLL_INFO *const coll)
{
    const LARA_INFO *const lara = Lara_GetLaraInfo();
    if (!lara->climb_status || !g_Input.forward
        || coll->side_mid.ceiling > -STEP_L) {
        return false;
    }

    if (Item_TestAnimEqual(item, LA(LA_MONKEY_IDLE))) {
        return true;
    }

    if (!Item_TestAnimEqual(item, LA(LA_SWING_IN_SLOW))) {
        return false;
    }

    return Item_TestFrameRange(item, M_LF_CLIMB_1_START, M_LF_CLIMB_1_END)
        || Item_TestFrameRange(item, M_LF_CLIMB_2_START, M_LF_CLIMB_2_END)
        || Item_TestFrameRange(item, M_LF_CLIMB_3_START, M_LF_CLIMB_3_END);
}

static void M_MonkeySwingFall(ITEM *const item)
{
    item->goal_anim_state = LS(LS_JUMP_UP);
    item->current_anim_state = LS(LS_JUMP_UP);
    Item_SwitchToAnim(item, LA(LA_JUMP_UP), M_MONKEY_FALL_FRAME);

    item->gravity = true;
    item->speed = 2;
    item->fall_speed = 1;
    item->pos.y += STEP_L;

    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->gun_status = LGS_ARMLESS;
}

static void M_GetMonkeyCollisionInfo(
    ITEM *const item, COLL_INFO *const coll, const int16_t move_angle,
    const int32_t bad_neg, const bool slopes_are_walls)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->move_angle = move_angle;

    coll->bad_pos = NO_BAD_POS;
    coll->bad_neg = bad_neg;
    coll->bad_ceiling = 0;
    coll->facing = lara->move_angle;
    coll->radius = M_MONKEY_RADIUS;
    coll->slopes_are_walls = slopes_are_walls;

    Collide_GetCollisionInfo(
        coll, item->pos.x, item->pos.y, item->pos.z, item->room_num,
        M_MONKEY_HEIGHT);
}

static bool M_IsDirOctant(const int16_t rot)
{
    const int16_t abs_rot = ABS(rot);
    return abs_rot >= DEG_45 && abs_rot <= DEG_135;
}

static bool M_TestMonkeySide(
    ITEM *const item, COLL_INFO *const coll, const int16_t angle_delta,
    const bool is_right)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    const int16_t old_move_angle = lara->move_angle;
    lara->move_angle = item->rot.y + angle_delta;

    M_GetMonkeyCollisionInfo(
        item, coll, lara->move_angle, is_right ? -STEPUP_HEIGHT : NO_BAD_NEG,
        false);

    bool ok = true;
    if (ABS(coll->side_mid.ceiling - coll->side_front.ceiling)
        > M_MONKEY_CEILING_SHIFT) {
        ok = false;
        goto cleanup;
    }

    if (coll->coll_type != COLL_NONE) {
        const bool oct = M_IsDirOctant(item->rot.y);
        if (!oct && coll->coll_type == COLL_FRONT) {
            ok = false;
            goto cleanup;
        }

        if (!is_right) {
            if ((!oct && coll->coll_type == COLL_LEFT)
                || (oct
                    && (coll->coll_type == COLL_RIGHT
                        || coll->coll_type == COLL_LEFT))) {
                ok = false;
                goto cleanup;
            }
        } else {
            if (oct
                && (coll->coll_type == COLL_FRONT
                    || coll->coll_type == COLL_RIGHT
                    || coll->coll_type == COLL_LEFT)) {
                ok = false;
                goto cleanup;
            }
        }
    }

cleanup:
    lara->move_angle = old_move_angle;
    return ok;
}

static bool M_HandleIdleState(ITEM *const item, COLL_INFO *const coll)
{
    if (!M_CanMonkeySwing(item)) {
        return false;
    }

    if (!g_Input.action || item->hit_points <= 0) {
        M_MonkeySwingFall(item);
        return true;
    }

    item->gravity = false;
    item->fall_speed = 0;
    item->speed = 0;

    M_GetMonkeyCollisionInfo(item, coll, item->rot.y, NO_BAD_NEG, false);
    if (coll->side_mid.ceiling < -STEP_L) {
        // Lara is in a slow-swing state far below the ceiling.
        return false;
    }

    if (g_Input.forward && coll->coll_type != COLL_FRONT
        && ABS(coll->side_mid.ceiling - coll->side_front.ceiling)
            < M_MONKEY_CEILING_SHIFT) {
        item->goal_anim_state = LS(LS_MONKEY_FORWARD);
    } else if (
        g_Input.step_left && M_TestMonkeySide(item, coll, -DEG_90, false)) {
        item->goal_anim_state = LS(LS_MONKEY_LEFT);
    } else if (
        g_Input.step_right && M_TestMonkeySide(item, coll, DEG_90, true)) {
        item->goal_anim_state = LS(LS_MONKEY_RIGHT);
    } else if (g_Input.left) {
        item->goal_anim_state = LS(LS_MONKEY_TURN_LEFT);
    } else if (g_Input.right) {
        item->goal_anim_state = LS(LS_MONKEY_TURN_RIGHT);
    }

    Lara_Col_MonkeySwingSnap(item);
    return true;
}

static void M_MonkeyIdle(ITEM *const item, COLL_INFO *const coll)
{
    if (M_HandleIdleState(item, coll)) {
        return;
    }

    // Monkey idle state can be the result of swinging on a thin ledge as well
    // as actually being on monkeybars. LA_SWING_IN_SLOW links to this state.
    Lara_Col_HangTest(item, coll);
    if (item->goal_anim_state != LS(LS_MONKEY_IDLE)) {
        return;
    }

    if (g_Input.forward && coll->side_front.floor > -850
        && coll->side_front.floor < -650
        && coll->side_front.floor - coll->side_front.ceiling >= 0
        && coll->side_left2.floor - coll->side_left2.ceiling >= 0
        && coll->side_right2.floor - coll->side_right2.ceiling >= 0
        && !coll->hit_static) {
        item->goal_anim_state = LS(g_Input.slow ? LS_GYMNAST : LS_PULL_UP);
        return;
    }

    if (M_CanClimb(item, coll)) {
        item->goal_anim_state = LS(LS_HANG);
        item->current_anim_state = LS(LS_HANG);
        Item_SwitchToAnim(item, LA(LA_LADDER_UP_HANGING), 0);
        return;
    }

    if ((g_Input.forward || g_Input.crouch) && coll->side_front.floor > -850
        && coll->side_front.floor < -650
        && coll->side_front.floor - coll->side_front.ceiling >= -256
        && coll->side_left2.floor - coll->side_left2.ceiling >= -256
        && coll->side_right2.floor - coll->side_right2.ceiling >= -256
        && !coll->hit_static) {
        item->goal_anim_state = LS(LS_CLIMB_TO_CRAWL);
        item->required_anim_state = LS(LS_CROUCH_IDLE);
    } else if (g_Input.left || g_Input.step_left) {
        item->goal_anim_state = LS(LS_SHIMMY_LEFT);
    } else if (g_Input.right || g_Input.step_right) {
        item->goal_anim_state = LS(LS_SHIMMY_RIGHT);
    }
}

static void M_MonkeyForward(ITEM *const item, COLL_INFO *const coll)
{
    if (!g_Input.action || !M_CanMonkeySwing(item)) {
        M_MonkeySwingFall(item);
        return;
    }

    item->gravity = false;
    item->fall_speed = 0;

    M_GetMonkeyCollisionInfo(item, coll, item->rot.y, NO_BAD_NEG, false);

    if (coll->coll_type == COLL_FRONT
        || ABS(coll->side_mid.ceiling - coll->side_front.ceiling)
            > M_MONKEY_CEILING_SHIFT) {
        Item_SwitchToAnim(item, LA(LA_MONKEY_IDLE), 0);
        item->current_anim_state = LS(LS_MONKEY_IDLE);
        item->goal_anim_state = LS(LS_MONKEY_IDLE);
        Lara_Col_MonkeySwingSnap(item);
        return;
    }

    g_Camera.target_elevation = M_CAM_MONKEY_ELEVATION;
    Lara_Col_MonkeySwingSnap(item);
}

static void M_MonkeySide(ITEM *const item, COLL_INFO *const coll)
{
    if (!g_Input.action || !M_CanMonkeySwing(item)) {
        M_MonkeySwingFall(item);
        return;
    }

    item->gravity = false;
    item->fall_speed = 0;
    item->speed = 0;

    const bool is_right = item->current_anim_state == LS(LS_MONKEY_RIGHT);
    const int16_t angle_delta = is_right ? DEG_90 : -DEG_90;

    if (M_TestMonkeySide(item, coll, angle_delta, is_right)) {
        LARA_INFO *const lara = Lara_GetLaraInfo();
        lara->move_angle = item->rot.y + angle_delta;
        g_Camera.target_elevation = M_CAM_MONKEY_ELEVATION;
        Lara_Col_MonkeySwingSnap(item);
    } else {
        Item_SwitchToAnim(item, LA(LA_MONKEY_IDLE), 0);
        item->current_anim_state = LS(LS_MONKEY_IDLE);
        item->goal_anim_state = LS(LS_MONKEY_IDLE);
        Lara_Col_MonkeySwingSnap(item);
    }
}

static void M_MonkeyTurn(ITEM *const item, COLL_INFO *const coll)
{
    if (!g_Input.action || !M_CanMonkeySwing(item)) {
        M_MonkeySwingFall(item);
        return;
    }

    item->gravity = false;
    item->fall_speed = 0;
    item->speed = 0;

    M_GetMonkeyCollisionInfo(item, coll, item->rot.y, -STEPUP_HEIGHT, true);

    Lara_Col_MonkeySwingSnap(item);
}

static void M_MonkeyRoll(ITEM *const item, COLL_INFO *const coll)
{
    M_MonkeyForward(item, coll);
}

// clang-format off
REGISTER_LARA_COL(LS_MONKEY_IDLE,       M_MonkeyIdle)
REGISTER_LARA_COL(LS_MONKEY_FORWARD,    M_MonkeyForward)
REGISTER_LARA_COL(LS_MONKEY_LEFT,       M_MonkeySide)
REGISTER_LARA_COL(LS_MONKEY_RIGHT,      M_MonkeySide)
REGISTER_LARA_COL(LS_MONKEY_TURN_LEFT,  M_MonkeyTurn)
REGISTER_LARA_COL(LS_MONKEY_TURN_RIGHT, M_MonkeyTurn)
REGISTER_LARA_COL(LS_MONKEY_ROLL,       M_MonkeyRoll)
// clang-format on
