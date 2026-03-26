#include <trx/config.h>
#include <trx/core/utils.h>
#include <trx/game/camera.h>
#include <trx/game/input.h>
#include <trx/game/lara.h>
#include <trx/game/lara/util.h>
#include <trx/game/rooms.h>
#include <trx/game/rooms/enum.h>

// clang-format off
#define M_CAM_MONKEY_ELEVATION  (10 * DEG_1)  // = 1820
#define M_CAM_HANG_ANGLE        0
#define M_CAM_HANG_ELEVATION    (-60 * DEG_1) // = -10920
#define M_MONKEY_TURN           ((DEG_1 * 1) + LARA_TURN_UNDO) // = 546
// clang-format on

static bool M_CanMonkeySwing(const ITEM *const item)
{
    int16_t room_num = item->room_num;
    const SECTOR *const sector = Room_GetSector(
        (XYZ_32) { item->pos.x, MAX_HEIGHT, item->pos.z }, &room_num);
    return (sector->ladder & LADDER_CEILING) != 0;
}

static void M_MonkeyIdle(ITEM *const item, COLL_INFO *const coll)
{
    coll->enable_hit = 0;
    coll->enable_baddie_push = 0;

    if (M_CanMonkeySwing(item)) {
        if (g_Input.action && item->hit_points > 0) {
            g_Camera.target_angle = M_CAM_HANG_ANGLE;
            g_Camera.target_elevation = M_CAM_HANG_ELEVATION;
        }
        return;
    }

    if (g_Config.gameplay.look_mode != LOOK_MODE_RESTRICTED && g_Input.look) {
        Lara_Look_UpDown();
    }
}

static void M_MonkeyForward(ITEM *const item, COLL_INFO *const coll)
{
    coll->enable_hit = 0;
    coll->enable_baddie_push = 0;

    if (item->hit_points <= 0) {
        item->goal_anim_state = LS(LS_MONKEY_IDLE);
        return;
    }

    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->move_angle = item->rot.y;

    if (g_Input.forward) {
        item->goal_anim_state = LS(LS_MONKEY_FORWARD);
    } else {
        item->goal_anim_state = LS(LS_MONKEY_IDLE);
    }

    if (g_Input.left) {
        lara->turn_rate -= LARA_TURN_RATE;
        CLAMPL(lara->turn_rate, -M_MONKEY_TURN);
    } else if (g_Input.right) {
        lara->turn_rate += LARA_TURN_RATE;
        CLAMPG(lara->turn_rate, +M_MONKEY_TURN);
    }
}

static void M_MonkeyShimmy(ITEM *const item, COLL_INFO *const coll)
{
    coll->enable_hit = 0;
    coll->enable_baddie_push = 0;
    g_Camera.target_elevation = M_CAM_MONKEY_ELEVATION;

    LARA_INFO *const lara = Lara_GetLaraInfo();
    if (item->current_anim_state == LS(LS_MONKEY_LEFT)) {
        lara->move_angle = item->rot.y - DEG_90;
    } else {
        lara->move_angle = item->rot.y + DEG_90;
    }

    const bool stop = item->current_anim_state == LS(LS_MONKEY_LEFT)
        ? !g_Input.step_left
        : !g_Input.step_right;
    if (stop) {
        item->goal_anim_state = LS(LS_MONKEY_IDLE);
    }
}

static void M_MonkeyTurn(ITEM *const item, COLL_INFO *const coll)
{
    coll->enable_hit = 0;
    coll->enable_baddie_push = 0;
    g_Camera.target_elevation = M_CAM_MONKEY_ELEVATION;

    if (item->hit_points <= 0) {
        item->goal_anim_state = LS(LS_MONKEY_IDLE);
        return;
    }

    if (item->current_anim_state == LS(LS_MONKEY_TURN_LEFT)) {
        item->rot.y -= LARA_LEAN_RATE;
        if (!g_Input.left) {
            item->goal_anim_state = LS(LS_MONKEY_IDLE);
        }
    } else {
        item->rot.y += LARA_LEAN_RATE;
        if (!g_Input.right) {
            item->goal_anim_state = LS(LS_MONKEY_IDLE);
        }
    }
}

static void M_MonkeyRoll(ITEM *const item, COLL_INFO *const coll)
{
    coll->enable_hit = 0;
    coll->enable_baddie_push = 0;
    item->goal_anim_state = LS(LS_MONKEY_IDLE);
}

// clang-format off
REGISTER_LARA_STATE(LS_MONKEY_IDLE,       M_MonkeyIdle)
REGISTER_LARA_STATE(LS_MONKEY_FORWARD,    M_MonkeyForward)
REGISTER_LARA_STATE(LS_MONKEY_LEFT,       M_MonkeyShimmy)
REGISTER_LARA_STATE(LS_MONKEY_RIGHT,      M_MonkeyShimmy)
REGISTER_LARA_STATE(LS_MONKEY_TURN_LEFT,  M_MonkeyTurn)
REGISTER_LARA_STATE(LS_MONKEY_TURN_RIGHT, M_MonkeyTurn)
REGISTER_LARA_STATE(LS_MONKEY_ROLL,       M_MonkeyRoll)
// clang-format on
