#include <trx/config.h>
#include <trx/game/camera.h>
#include <trx/game/input.h>
#include <trx/game/lara.h>
#include <trx/game/lara/util.h>
#include <trx/game/sound.h>
#include <trx/version.h>

// clang-format off
#define M_SCREAM_SPEED          (DAMAGE_START + DAMAGE_LENGTH) // = 154
#define M_JUMP_TURN             ((DEG_1 * 1) + LARA_TURN_UNDO) // = 546
#define M_FAST_FALL_SPEED       (FAST_FALL_SPEED + 3)          // = 131
#define M_SWING_FAST_FALL_SPEED (M_FAST_FALL_SPEED + 2)        // = 133
#define M_CAM_BACK_JUMP_ANGLE   (135 * DEG_1)                  // = 24570
#define M_CAM_REACH_ANGLE       (85 * DEG_1)                   // = 15470
#define M_CAM_ZIPLINE_ANGLE     (70 * DEG_1)                   // = 12740
#define M_LF_NEUTRAL_TWIST_WADE_SPLASH -5
// clang-format on

static void M_Compress(ITEM *const item, COLL_INFO *const coll)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    if (lara->water_status != LWS_WADE) {
        if (g_Input.forward
            && Lara_FloorFront(item, item->rot.y, STEP_L) >= -STEPUP_HEIGHT) {
            item->goal_anim_state = LS(LS_JUMP_FORWARD);
            lara->move_angle = item->rot.y;
        } else if (
            g_Input.left
            && Lara_FloorFront(item, item->rot.y - DEG_90, STEP_L)
                >= -STEPUP_HEIGHT) {
            item->goal_anim_state = LS(LS_JUMP_LEFT);
            lara->move_angle = item->rot.y - DEG_90;
        } else if (
            g_Input.right
            && Lara_FloorFront(item, item->rot.y + DEG_90, STEP_L)
                >= -STEPUP_HEIGHT) {
            item->goal_anim_state = LS(LS_JUMP_RIGHT);
            lara->move_angle = item->rot.y + DEG_90;
        } else if (
            g_Input.back
            && Lara_FloorFront(item, item->rot.y + DEG_180, STEP_L)
                >= -STEPUP_HEIGHT) {
            item->goal_anim_state = LS(LS_JUMP_BACK);
            lara->move_angle = item->rot.y + DEG_180;
        } else if (
            g_Input.roll && g_Config.gameplay.enable_neutral_twists
            && Lara_State_IsResponsive(LA_STAND_TO_JUMP)) {
            item->goal_anim_state = LS(LS_RESPONSIVE);
        }
    }

    if (item->fall_speed > M_FAST_FALL_SPEED) {
        item->goal_anim_state = LS(LS_FAST_FALL);
    }
}

static void M_UpJump(ITEM *const item, COLL_INFO *const coll)
{
    const int16_t fast_speed = g_Config.gameplay.enable_swing_cancel
        ? M_SWING_FAST_FALL_SPEED
        : M_FAST_FALL_SPEED;
    if (item->fall_speed > fast_speed) {
        item->goal_anim_state = LS(LS_FAST_FALL);
    }
}

static void M_NeutralJumpRoll(ITEM *const item, COLL_INFO *const coll)
{
    coll->enable_hit = 0;
    if (g_Input.jump && g_Input.roll) {
        item->goal_anim_state = LS(LS_RESPONSIVE);
    } else if (g_Input.jump) {
        item->goal_anim_state = LS(LS_COMPRESS);
    } else if (g_Input.roll) {
        item->goal_anim_state = LS(LS_ROLL);
    } else {
        if (Item_TestFrameEqual(item, M_LF_NEUTRAL_TWIST_WADE_SPLASH)) {
            Lara_Col_WadeSplash(item);
        }
        item->goal_anim_state = LS(LS_STOP);
    }
}

static void M_ForwardJump(ITEM *const item, COLL_INFO *const coll)
{
    if (item->goal_anim_state == LS(LS_SWAN_DIVE)
        || item->goal_anim_state == LS(LS_REACH)) {
        item->goal_anim_state = LS(LS_JUMP_FORWARD);
    }

    LARA_INFO *const lara = Lara_GetLaraInfo();
    if (item->goal_anim_state != LS(LS_DEATH)
        && item->goal_anim_state != LS(LS_STOP)
        && item->goal_anim_state != LS(LS_RUN)) {
        if (g_Input.action && lara->gun_status == LGS_ARMLESS) {
            item->goal_anim_state = LS(LS_REACH);
        }
        if (g_Config.gameplay.enable_jump_twists
            && (g_Input.roll || g_Input.back)) {
            item->goal_anim_state = LS(LS_TWIST);
        }
        if (g_Input.slow && lara->gun_status == LGS_ARMLESS) {
            item->goal_anim_state = LS(LS_SWAN_DIVE);
        }
        if (item->fall_speed > M_FAST_FALL_SPEED) {
            item->goal_anim_state = LS(LS_FAST_FALL);
        }
    }

    if (g_Input.left) {
        lara->turn_rate -= LARA_TURN_RATE;
        CLAMPL(lara->turn_rate, -M_JUMP_TURN);
    } else if (g_Input.right) {
        lara->turn_rate += LARA_TURN_RATE;
        CLAMPG(lara->turn_rate, +M_JUMP_TURN);
    }
}

static void M_BackJump(ITEM *const item, COLL_INFO *const coll)
{
    if (!Item_TestAnimEqual(item, LA(LA_HANG_TO_JUMP_BACK))) {
        g_Camera.target_angle = M_CAM_BACK_JUMP_ANGLE;
    }
    if (item->fall_speed > M_FAST_FALL_SPEED) {
        item->goal_anim_state = LS(LS_FAST_FALL);
        return;
    }

    if (item->goal_anim_state == LS(LS_RUN)) {
        item->goal_anim_state = LS(LS_STOP);
    } else if (
        g_Config.gameplay.enable_jump_twists
        && (g_Input.forward || g_Input.roll)
        && item->goal_anim_state != LS(LS_STOP)) {
        item->goal_anim_state = LS(LS_TWIST);
    }
}

static void M_SideJump(ITEM *const item, COLL_INFO *const coll)
{
    if (item->fall_speed > M_FAST_FALL_SPEED) {
        item->goal_anim_state = LS(LS_FAST_FALL);
        return;
    }

    // TODO: unused animation transition, perhaps look at restoring
    const bool twist_input = item->current_anim_state == LS(LS_JUMP_LEFT)
        ? g_Input.right
        : g_Input.left;
    if (g_Config.gameplay.enable_jump_twists && twist_input
        && item->goal_anim_state != LS(LS_STOP)) {
        item->goal_anim_state = LS(LS_TWIST);
    }
}

static void M_FallBack(ITEM *const item, COLL_INFO *const coll)
{
    if (item->fall_speed > M_FAST_FALL_SPEED) {
        item->goal_anim_state = LS(LS_FAST_FALL);
        return;
    }

    const LARA_INFO *const lara = Lara_GetLaraInfo();
    if (g_Input.action && lara->gun_status == LGS_ARMLESS) {
        item->goal_anim_state = LS(LS_REACH);
    }
}

static void M_Reach(ITEM *const item, COLL_INFO *const coll)
{
    g_Camera.target_angle = M_CAM_REACH_ANGLE;
    if (item->fall_speed > M_FAST_FALL_SPEED) {
        item->goal_anim_state = LS(LS_FAST_FALL);
    }
}

static void M_SwanDive(ITEM *const item, COLL_INFO *const coll)
{
    coll->enable_hit = 0;
    coll->enable_baddie_push = 1;
    if (item->fall_speed > M_FAST_FALL_SPEED
        && item->goal_anim_state != LS(LS_DIVE)) {
        item->goal_anim_state = LS(LS_FAST_DIVE);
    }
}

static void M_FastDive(ITEM *item, COLL_INFO *coll)
{
    if (g_Config.gameplay.enable_jump_twists && g_Input.roll
        && item->goal_anim_state == LS(LS_FAST_DIVE)) {
        item->goal_anim_state = LS(LS_TWIST);
    }
    coll->enable_hit = 0;
    coll->enable_baddie_push = 1;
    item->speed = item->speed * 95 / 100;
}

static void M_FastFall(ITEM *const item, COLL_INFO *const coll)
{
    item->speed = item->speed * 95 / 100;
    const bool scream = g_TRVersion == 1 ? (item->fall_speed >= M_SCREAM_SPEED)
                                         : (item->fall_speed == M_SCREAM_SPEED);
    if (scream && !g_Config.debug.enable_invulnerability) {
        Sound_Effect(SFX_LARA_FALL, &item->pos, SPM_NORMAL);
    }
}

static void M_Zipline(ITEM *const item, COLL_INFO *const coll)
{
    g_Camera.target_angle = M_CAM_ZIPLINE_ANGLE;

    if (!g_Input.action) {
        item->goal_anim_state = LS(LS_JUMP_FORWARD);
        Lara_Animate(item);
        item->gravity = true;
        item->speed = 100;
        item->fall_speed = 40;
        LARA_INFO *const lara = Lara_GetLaraInfo();
        lara->move_angle = item->rot.y;
    }
}

// clang-format off
REGISTER_LARA_STATE(LS_COMPRESS,     M_Compress)
REGISTER_LARA_STATE(LS_JUMP_UP,      M_UpJump)
REGISTER_LARA_STATE(LS_NEUTRAL_ROLL, M_NeutralJumpRoll)
REGISTER_LARA_STATE(LS_JUMP_FORWARD, M_ForwardJump)
REGISTER_LARA_STATE(LS_JUMP_BACK,    M_BackJump)
REGISTER_LARA_STATE(LS_JUMP_RIGHT,   M_SideJump)
REGISTER_LARA_STATE(LS_JUMP_LEFT,    M_SideJump)
REGISTER_LARA_STATE(LS_FALL_BACK,    M_FallBack)
REGISTER_LARA_STATE(LS_REACH,        M_Reach)
REGISTER_LARA_STATE(LS_SWAN_DIVE,    M_SwanDive)
REGISTER_LARA_STATE(LS_FAST_DIVE,    M_FastDive)
REGISTER_LARA_STATE(LS_FAST_FALL,    M_FastFall)
REGISTER_LARA_STATE(LS_ZIPLINE,      M_Zipline)
// clang-format on
