#include <trx/config.h>
#include <trx/game/camera.h>
#include <trx/game/input.h>
#include <trx/game/lara.h>
#include <trx/game/lara/util.h>
#include <trx/version.h>

// clang-format off
#define M_CAM_HANG_ANGLE             0
#define M_CAM_HANG_ELEVATION         (-60 * DEG_1)              // = -10920
#define M_CAM_CLIMB_LEFT_ANGLE       (-30 * DEG_1)              // = -5460
#define M_CAM_CLIMB_LEFT_ELEVATION   (-15 * DEG_1)              // = -2730
#define M_CAM_CLIMB_RIGHT_ANGLE      (-M_CAM_CLIMB_LEFT_ANGLE)  // = 5460
#define M_CAM_CLIMB_RIGHT_ELEVATION  M_CAM_CLIMB_LEFT_ELEVATION // = -2730
#define M_CAM_CLIMB_STANCE_ELEVATION (-20 * DEG_1)              // = -3640
#define M_CAM_CLIMBING_ELEVATION     (30 * DEG_1)               // = 5460
#define M_CAM_CLIMB_END_ELEVATION    (-45 * DEG_1)              // = -8190
#define M_CAM_CLIMB_DOWN_ELEVATION   M_CAM_CLIMB_END_ELEVATION  // = -8190
// clang-format on

static void M_Hang(ITEM *const item, COLL_INFO *const coll)
{
    if (item->hit_points <= 0) {
        item->goal_anim_state = LS(LS_STOP);
        return;
    }

    if (g_Config.gameplay.look_mode != LOOK_MODE_RESTRICTED && g_Input.look) {
        Lara_Look_UpDown();
    }

    coll->enable_hit = 0;
    coll->enable_baddie_push = 0;
    g_Camera.target_angle = M_CAM_HANG_ANGLE;
    g_Camera.target_elevation = M_CAM_HANG_ELEVATION;
    if (g_Input.left || g_Input.step_left) {
        item->goal_anim_state = LS(LS_SHIMMY_LEFT);
    } else if (g_Input.right || g_Input.step_right) {
        item->goal_anim_state = LS(LS_SHIMMY_RIGHT);
    }
}

static void M_Shimmy(ITEM *const item, COLL_INFO *const coll)
{
    coll->enable_hit = 0;
    coll->enable_baddie_push = 0;
    g_Camera.target_angle = M_CAM_HANG_ANGLE;
    g_Camera.target_elevation = M_CAM_HANG_ELEVATION;

    const bool stop = item->current_anim_state == LS(LS_SHIMMY_LEFT)
        ? (!g_Input.left && !g_Input.step_left)
        : (!g_Input.right && !g_Input.step_right);
    if (stop) {
        item->goal_anim_state = LS(LS_HANG);
    }
}

static void M_StanceLadder(ITEM *const item, COLL_INFO *const coll)
{
    coll->enable_hit = 0;
    coll->enable_baddie_push = 0;
    g_Camera.target_elevation = M_CAM_CLIMB_STANCE_ELEVATION;

    if (g_Input.look) {
        Lara_Look_UpDown();
    }

    if (g_Input.left || g_Input.step_left) {
        item->goal_anim_state = LS(LS_CLIMB_LEFT);
    } else if (g_Input.right || g_Input.step_right) {
        item->goal_anim_state = LS(LS_CLIMB_RIGHT);
    } else if (g_Input.jump) {
        item->goal_anim_state = LS(LS_JUMP_BACK);
        LARA_INFO *const lara = Lara_GetLaraInfo();
        lara->gun_status = LGS_ARMLESS;
        lara->move_angle = item->rot.y + DEG_180;
    }
}

static void M_SideLadder(ITEM *const item, COLL_INFO *const coll)
{
    coll->enable_hit = 0;
    coll->enable_baddie_push = 0;
    if (item->current_anim_state == LS(LS_CLIMB_LEFT)) {
        g_Camera.target_angle = M_CAM_CLIMB_LEFT_ANGLE;
        g_Camera.target_elevation = M_CAM_CLIMB_LEFT_ELEVATION;
        if (!g_Input.left && !g_Input.step_left) {
            item->goal_anim_state = LS(LS_CLIMB_STANCE);
        }
    } else {
        g_Camera.target_angle = M_CAM_CLIMB_RIGHT_ANGLE;
        g_Camera.target_elevation = M_CAM_CLIMB_RIGHT_ELEVATION;
        if (!g_Input.right && !g_Input.step_right) {
            item->goal_anim_state = LS(LS_CLIMB_STANCE);
        }
    }
}

static void M_UpDownLadder(ITEM *const item, COLL_INFO *const coll)
{
    coll->enable_hit = 0;
    coll->enable_baddie_push = 0;
    switch (LS_U(item->current_anim_state)) {
    case LS_CLIMBING:
        g_Camera.target_elevation = M_CAM_CLIMBING_ELEVATION;
        break;
    case LS_CLIMB_DOWN:
        g_Camera.target_elevation = M_CAM_CLIMB_DOWN_ELEVATION;
        break;
    case LS_CLIMB_END:
        g_Camera.flags = CF_FOLLOW_CENTRE;
        g_Camera.target_angle = M_CAM_CLIMB_END_ELEVATION;
        break;
    default:
        break;
    }
}

// clang-format off
REGISTER_LARA_STATE(LS_HANG,         M_Hang)
REGISTER_LARA_STATE(LS_SHIMMY_LEFT,  M_Shimmy)
REGISTER_LARA_STATE(LS_SHIMMY_RIGHT, M_Shimmy)
REGISTER_LARA_STATE(LS_CLIMB_STANCE, M_StanceLadder)
REGISTER_LARA_STATE(LS_CLIMB_LEFT,   M_SideLadder)
REGISTER_LARA_STATE(LS_CLIMB_RIGHT,  M_SideLadder)
REGISTER_LARA_STATE(LS_CLIMBING,     M_UpDownLadder)
REGISTER_LARA_STATE(LS_CLIMB_DOWN,   M_UpDownLadder)
REGISTER_LARA_STATE(LS_CLIMB_END,    M_UpDownLadder)
// clang-format on
