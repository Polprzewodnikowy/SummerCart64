#include <trx/config.h>
#include <trx/game/camera.h>
#include <trx/game/gun.h>
#include <trx/game/input.h>
#include <trx/game/lara.h>
#include <trx/game/lara/util.h>
#include <trx/game/random.h>
#include <trx/game/rooms.h>
#include <trx/version.h>

// clang-format off
#define M_WALK_DIST                104
#define M_WALK_BACK_DIST           140
#define M_LF_ROLL                  2
#define M_CANCEL_POSE_TIME         (10 * LOGIC_FPS)               // = 300
#define M_CANCEL_POSE_CHANCE       0x40                           // = 64
#define M_FAST_TURN                ((DEG_1 * 6) + LARA_TURN_UNDO) // = 1456
#define M_FAST_FALL_SPEED          (FAST_FALL_SPEED + 3)          // = 131
#define M_SPRINT_TURN_RATE         ((DEG_1 * 2) + 45)             // = 409
#define M_SPRINT_TURN_MAX          (DEG_1 * 4)                    // = 728
#define M_SPRINT_LEAN_MAX          (DEG_1 * 16)                   // = 2192
#define M_CAM_SLIDE_ELEVATION      (-45 * DEG_1)                  // = -8190
#define M_CAM_PUSH_BLOCK_ANGLE     (35 * DEG_1)                   // = 6370
#define M_CAM_PUSH_BLOCK_ELEVATION (-25 * DEG_1)                  // = -4550
#define M_CAM_PP_READY_ANGLE       (75 * DEG_1)                   // = 13650
#define M_CAM_PICKUP_ANGLE         (-130 * DEG_1)                 // = -23660
#define M_CAM_PICKUP_ELEVATION     (-15 * DEG_1)                  // = -2730
#define M_CAM_PICKUP_DISTANCE      WALL_L                         // = 1024
#define M_CAM_SWITCH_ON_ANGLE      (80 * DEG_1)                   // = 14560
#define M_CAM_SWITCH_ON_ELEVATION  (-25 * DEG_1)                  // = -4550
#define M_CAM_SWITCH_ON_DISTANCE   WALL_L                         // = 1024
#define M_CAM_SWITCH_ON_SPEED      6
#define M_CAM_USE_KEY_ANGLE        (-M_CAM_SWITCH_ON_ANGLE)       // = -14560
#define M_CAM_USE_KEY_ELEVATION    M_CAM_SWITCH_ON_ELEVATION      // = -4550
#define M_CAM_USE_KEY_DISTANCE     WALL_L                         // = 1024
#define M_CAM_SPECIAL_ANGLE        (170 * DEG_1)                  // = 30940
#define M_CAM_SPECIAL_ELEVATION    (-25 * DEG_1)                  // = -4550
#define M_CAM_SPECIAL_DISTANCE     (2 * WALL_L)                   // = 2048
#define M_CAM_POSE_RIGHT_ANGLE     M_CAM_SPECIAL_ANGLE            // = 30940
#define M_CAM_POSE_LEFT_ANGLE     -M_CAM_SPECIAL_ANGLE            // = -30940
// clang-format on

static bool m_JumpPermitted = true;
static const int16_t m_JumpLockFrames[JUMP_LOCK_NUMBER_OF] = {
    // clang-format off
    [JUMP_LOCK_LEGACY]   = 4,
    [JUMP_LOCK_TUNED]    = 2,
    [JUMP_LOCK_DISABLED] = 19,
    // clang-format on
};

static bool M_CanPose(void)
{
    if (g_Config.gameplay.idle_pose_timeout == 0) {
        return false;
    }

    const LARA_INFO *const lara = Lara_GetLaraInfo();
    return !g_Input.draw && !g_Input.look && lara->hit_direction == DIR_UNKNOWN
        && lara->gun_status == LGS_ARMLESS
        && lara->water_status == LWS_ABOVE_WATER && !g_Input.use_flare
        && !lara->flare.control && !Lara_Vehicle_IsMounted();
}

static bool M_ShouldStopPosing(void)
{
    return !M_CanPose() || g_Input.forward || g_Input.back || g_Input.left
        || g_Input.right || g_Input.step_left || g_Input.step_right
        || g_Input.jump || g_Input.action;
}

static void M_Default(ITEM *const item, COLL_INFO *const coll)
{
    coll->enable_hit = 0;
    coll->enable_baddie_push = 0;
}

static void M_Walk(ITEM *const item, COLL_INFO *const coll)
{
    if (item->hit_points <= 0) {
        item->goal_anim_state = LS(LS_STOP);
        return;
    }

    LARA_INFO *const lara = Lara_GetLaraInfo();
    if (g_Input.left) {
        lara->turn_rate -= LARA_TURN_RATE;
        CLAMPL(lara->turn_rate, -LARA_SLOW_TURN);
    } else if (g_Input.right) {
        lara->turn_rate += LARA_TURN_RATE;
        CLAMPG(lara->turn_rate, +LARA_SLOW_TURN);
    }

    if (g_Input.forward) {
        if (lara->water_status == LWS_WADE) {
            item->goal_anim_state = LS(LS_WADE);
        } else if (g_Input.slow) {
            item->goal_anim_state = LS(LS_WALK);
        } else {
            if (g_Config.gameplay.fix_walk_run_jump) {
                m_JumpPermitted = true;
            }
            item->goal_anim_state = LS(LS_RUN);
        }
    } else {
        item->goal_anim_state = LS(LS_STOP);
    }
}

static LARA_STATE M_GetRunToCrouchState(void)
{
    return LS(
        g_Config.gameplay.enable_responsive_crawl ? LS_CROUCH_IDLE : LS_STOP);
}

static bool M_RequestSprint(LARA_INFO *const lara)
{
    if (g_Config.gameplay.enable_toggle_sprint) {
        if (g_InputDB.sprint) {
            lara->sprinting = !lara->sprinting;
        }
        return lara->sprinting;
    } else {
        lara->sprinting = false;
        return g_Input.sprint;
    }
}

static bool M_RequestDuck(LARA_INFO *const lara)
{
    if (g_Config.gameplay.enable_toggle_crouch) {
        if (g_InputDB.crouch) {
            lara->crouching = true;
        }
        return lara->crouching;
    } else {
        lara->crouching = false;
        return g_Input.crouch;
    }
}

static void M_Run(ITEM *const item, COLL_INFO *const coll)
{
    if (item->hit_points <= 0) {
        item->goal_anim_state = LS(LS_DEATH);
        return;
    }

    if (g_Input.roll) {
        Lara_Col_WadeSplash(item);
        item->current_anim_state = LS(LS_ROLL);
        item->goal_anim_state = LS(LS_STOP);
        Item_SwitchToAnim(item, LA(LA_ROLL_START), M_LF_ROLL);
        return;
    }

    LARA_INFO *const lara = Lara_GetLaraInfo();
    const bool sprint_requested = M_RequestSprint(lara);
    if ((item->hit_points <= 0 || lara->sprint_timer <= 0
         || lara->water_status == LWS_WADE || !g_Config.gameplay.enable_sprint)
        && g_Config.gameplay.enable_toggle_sprint) {
        lara->sprinting = false;
    }

    if (sprint_requested && g_Config.gameplay.enable_sprint
        && lara->water_status != LWS_WADE
        && item->current_anim_state == LS(LS_RUN) && lara->sprint_timer > 0
        && (g_Config.gameplay.enable_responsive_sprint
            || lara->sprint_timer == LARA_MAX_SPRINT)) {
        item->goal_anim_state = LS(LS_SPRINT);
        return;
    }

    if (M_RequestDuck(lara)) {
        item->goal_anim_state = M_GetRunToCrouchState();
        return;
    }

    if (g_Input.left) {
        lara->turn_rate -= LARA_TURN_RATE;
        CLAMPL(lara->turn_rate, -M_FAST_TURN);
        item->rot.z -= LARA_LEAN_RATE;
        CLAMPL(item->rot.z, -LARA_LEAN_MAX);
    } else if (g_Input.right) {
        lara->turn_rate += LARA_TURN_RATE;
        CLAMPG(lara->turn_rate, +M_FAST_TURN);
        item->rot.z += LARA_LEAN_RATE;
        CLAMPG(item->rot.z, +LARA_LEAN_MAX);
    }

    const bool responsive_jumping =
        g_Config.gameplay.enable_tr2_jumping && Lara_State_IsResponsive(LA_RUN);
    if (responsive_jumping) {
        const int16_t unlock_frame =
            m_JumpLockFrames[g_Config.gameplay.jump_lock_mode];
        if (Item_TestAnimEqual(item, LA(LA_RUN_START))) {
            m_JumpPermitted =
                g_Config.gameplay.jump_lock_mode == JUMP_LOCK_DISABLED;
        } else if (
            !Item_TestAnimEqual(item, LA(LA_RUN))
            || Item_TestFrameEqual(item, unlock_frame)) {
            m_JumpPermitted = true;
        }
    } else {
        m_JumpPermitted = true;
    }

    if (g_Input.jump && m_JumpPermitted && !item->gravity) {
        item->goal_anim_state =
            LS(responsive_jumping ? LS_RESPONSIVE : LS_JUMP_FORWARD);
    } else if (g_Input.forward) {
        if (lara->water_status == LWS_WADE) {
            item->goal_anim_state = LS(LS_WADE);
        } else if (g_Input.slow) {
            item->goal_anim_state = LS(LS_WALK);
        } else {
            item->goal_anim_state = LS(LS_RUN);
        }
    } else {
        item->goal_anim_state = LS(LS_STOP);
    }
}

static void M_Wade(ITEM *const item, COLL_INFO *const coll)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->sprinting = false;

    if (item->hit_points <= 0) {
        item->goal_anim_state = LS(LS_STOP);
        return;
    }

    g_Camera.target_elevation = CAM_WADE_ELEVATION;

    const ROOM *const room = Room_Get(item->room_num);
    if (room->flags.swamp) {
        if (g_Input.left) {
            lara->turn_rate -= LARA_TURN_RATE;
            CLAMPL(lara->turn_rate, -LARA_SLOW_TURN);
            item->rot.z -= LARA_LEAN_RATE;
            CLAMPL(item->rot.z, -LARA_LEAN_MAX / 2);
        } else if (g_Input.right) {
            lara->turn_rate += LARA_TURN_RATE;
            CLAMPG(lara->turn_rate, LARA_SLOW_TURN);
            item->rot.z += LARA_LEAN_RATE;
            CLAMPG(item->rot.z, LARA_LEAN_MAX / 2);
        }

        if (g_Input.forward) {
            item->goal_anim_state = LS(LS_WADE);
        } else {
            item->goal_anim_state = LS(LS_STOP);
        }
    } else {
        if (g_Input.left) {
            lara->turn_rate -= LARA_TURN_RATE;
            CLAMPL(lara->turn_rate, -M_FAST_TURN);
            item->rot.z -= LARA_LEAN_RATE;
            CLAMPL(item->rot.z, -LARA_LEAN_MAX);
        } else if (g_Input.right) {
            lara->turn_rate += LARA_TURN_RATE;
            CLAMPG(lara->turn_rate, M_FAST_TURN);
            item->rot.z += LARA_LEAN_RATE;
            CLAMPG(item->rot.z, LARA_LEAN_MAX);
        }

        if (g_Input.forward) {
            if (lara->water_status != LWS_ABOVE_WATER) {
                item->goal_anim_state = LS(LS_WADE);
            } else {
                item->goal_anim_state = LS(LS_RUN);
            }
        } else {
            item->goal_anim_state = LS(LS_STOP);
        }
    }
}

static void M_WalkBack(ITEM *const item, COLL_INFO *const coll)
{
    if (item->hit_points <= 0) {
        item->goal_anim_state = LS(LS_STOP);
        return;
    }

    LARA_INFO *const lara = Lara_GetLaraInfo();
    if (g_Input.back && (g_Input.slow || lara->water_status == LWS_WADE)) {
        item->goal_anim_state = LS(LS_WALK_BACK);
    } else {
        item->goal_anim_state = LS(LS_STOP);
    }

    if (g_Input.left) {
        lara->turn_rate -= LARA_TURN_RATE;
        CLAMPL(lara->turn_rate, -LARA_SLOW_TURN);
    } else if (g_Input.right) {
        lara->turn_rate += LARA_TURN_RATE;
        CLAMPG(lara->turn_rate, LARA_SLOW_TURN);
    }
}

static void M_Stop(ITEM *const item, COLL_INFO *const coll)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->sprinting = false;

    if (item->hit_points <= 0) {
        item->goal_anim_state = LS(LS_DEATH);
        return;
    }

    if (lara->interact_target.is_moving) {
        return;
    }

    if (M_RequestDuck(lara) && lara->water_status != LWS_WADE
        && item->current_anim_state == LS(LS_STOP)
        && (lara->gun_status == LGS_ARMLESS || !Gun_IsRifleType(lara->gun_type))
        && !Lara_Vehicle_IsMounted()) {
        item->goal_anim_state = LS(LS_CROUCH_IDLE);
        return;
    }

    if (g_Input.roll && lara->water_status != LWS_WADE) {
        if (g_Input.jump && g_Config.gameplay.enable_neutral_twists
            && Item_TestAnimEqual(item, LA(LA_STAND_IDLE))
            && Lara_State_IsResponsive(LA_STAND_TO_JUMP)) {
            item->current_anim_state = LS(LS_NEUTRAL_ROLL);
            item->goal_anim_state = LS(LS_STOP);
            Item_SwitchToAnim(item, LA(LA_JUMP_NEUTRAL_ROLL), 0);
        } else if (!g_Input.jump || !g_Config.gameplay.enable_neutral_twists) {
            Lara_Col_WadeSplash(item);
            item->current_anim_state = LS(LS_ROLL);
            item->goal_anim_state = LS(LS_STOP);
            Item_SwitchToAnim(item, LA(LA_ROLL_START), M_LF_ROLL);
        }
        return;
    }

    lara->crouching = false;
    item->goal_anim_state = LS(LS_STOP);
    if (g_Input.look) {
        Lara_Look_UpDown();
        if (g_Config.gameplay.look_mode == LOOK_MODE_RESTRICTED) {
            Lara_Look_LeftRight();
            return;
        }
    }

    int32_t fheight = NO_HEIGHT;
    int32_t rheight = NO_HEIGHT;
    if (g_Input.forward) {
        fheight = Lara_FloorFront(item, item->rot.y, M_WALK_DIST);
    } else if (g_Input.back) {
        rheight =
            Lara_FloorFront(item, item->rot.y + DEG_180, M_WALK_BACK_DIST);
    }

    const ROOM *const room = Room_Get(item->room_num);
    if (room->flags.swamp) {
        if (g_Input.left) {
            item->goal_anim_state = LS(LS_TURN_LEFT);
        } else if (g_Input.right) {
            item->goal_anim_state = LS(LS_TURN_RIGHT);
        }
    } else if (g_Input.step_left) {
        const int32_t h = Lara_FloorFront(item, item->rot.y - DEG_90, 148);
        const int32_t c =
            Lara_CeilingFront(item, item->rot.y - DEG_90, 148, LARA_HEIGHT);
        if (g_TRVersion < 3
            || (h < 128 && h > -128 && Room_GetHeightType() != HT_BIG_SLOPE
                && c <= 0)) {
            item->goal_anim_state = LS(LS_STEP_LEFT);
        }
    } else if (g_Input.step_right) {
        const int32_t h = Lara_FloorFront(item, item->rot.y + DEG_90, 148);
        const int32_t c =
            Lara_CeilingFront(item, item->rot.y + DEG_90, 148, LARA_HEIGHT);
        if (g_TRVersion < 3
            || (h < 128 && h > -128 && Room_GetHeightType() != HT_BIG_SLOPE
                && c <= 0)) {
            item->goal_anim_state = LS(LS_STEP_RIGHT);
        }
    } else if (g_Input.left) {
        item->goal_anim_state = LS(LS_TURN_LEFT);
    } else if (g_Input.right) {
        item->goal_anim_state = LS(LS_TURN_RIGHT);
    }

    if (lara->water_status == LWS_WADE) {
        if (g_Input.jump && !room->flags.swamp) {
            item->goal_anim_state = LS(LS_COMPRESS);
        }

        if (g_Input.forward) {
            if (room->flags.swamp || g_Input.slow) {
                M_Wade(item, coll);
            } else {
                M_Walk(item, coll);
            }
        } else if (g_Input.back) {
            M_WalkBack(item, coll);
        }
    } else if (g_Input.jump) {
        item->goal_anim_state = LS(LS_COMPRESS);
    } else if (g_Input.forward) {
        bool bad_floor = false;
        bool bad_ceiling = false;
        if (g_Config.gameplay.wall_glitch_mode == WALL_GLITCH_FIXED) {
            const int32_t h = Lara_FloorFront(item, item->rot.y, M_WALK_DIST);
            const int32_t c =
                Lara_CeilingFront(item, item->rot.y, M_WALK_DIST, LARA_HEIGHT);
            const HEIGHT_TYPE height_type = Room_GetHeightType();
            bad_floor = height_type == HT_BIG_SLOPE && h < 0;
            bad_ceiling = c > 0 && !g_Input.action;
        }

        if (bad_floor || bad_ceiling) {
            item->goal_anim_state = LS_STOP;
        } else if (g_Input.slow) {
            M_Walk(item, coll);
        } else {
            M_Run(item, coll);
        }
    } else if (g_Input.back) {
        if (g_Input.slow) {
            if (g_TRVersion < 3
                || (rheight < (STEPUP_HEIGHT - 1)
                    && rheight > -(STEPUP_HEIGHT - 1)
                    && Room_GetHeightType() != HT_BIG_SLOPE)) {
                M_WalkBack(item, coll);
            }
        } else if (g_TRVersion < 3 || rheight > -(STEPUP_HEIGHT - 1)) {
            item->goal_anim_state = LS(LS_FAST_BACK);
        }
    }

    if (item->goal_anim_state == LS(LS_STOP) && M_CanPose()) {
        lara->idle_timer++;
        const int32_t timeout = g_Config.gameplay.idle_pose_timeout * LOGIC_FPS;
        CLAMPG(lara->idle_timer, timeout);
        if (lara->idle_timer == timeout) {
            lara->idle_timer = 0;
            item->goal_anim_state =
                LS(Random_GetControl() < 0x4000 ? LS_POSE_LEFT : LS_POSE_RIGHT);
        }
    } else {
        lara->idle_timer = 0;
    }
}

static void M_Pose(ITEM *const item, COLL_INFO *const coll)
{
    if (item->hit_points <= 0) {
        item->goal_anim_state = LS(LS_DEATH);
        return;
    }

    if (g_Input.look) {
        Lara_Look_UpDown();
        if (g_Config.gameplay.look_mode == LOOK_MODE_RESTRICTED) {
            Lara_Look_LeftRight();
            return;
        }
    }

    bool cancel_camera = false;
    if (g_Input.roll && !g_Input.jump) {
        item->goal_anim_state = LS(LS_ROLL);
        cancel_camera = true;
    } else if (item->current_anim_state == LS(LS_POSE)) {
        LARA_INFO *const lara = Lara_GetLaraInfo();
        lara->idle_timer++;
        if (M_ShouldStopPosing()
            || (lara->idle_timer >= M_CANCEL_POSE_TIME
                && Random_GetControl() < M_CANCEL_POSE_CHANCE)) {
            item->goal_anim_state = LS(LS_STOP);
            cancel_camera = true;
        }
    }

    if (g_Config.gameplay.enable_idle_pose_camera) {
        if (item->current_anim_state == LS(LS_POSE_START)
            && Item_TestFrameEqual(item, -1) && g_Camera.type == CAM_CHASE) {
            g_Camera.additional_angle =
                Item_TestAnimEqual(item, LA(LA_POSE_RIGHT_START))
                ? M_CAM_POSE_RIGHT_ANGLE
                : M_CAM_POSE_LEFT_ANGLE;
        } else if (cancel_camera) {
            g_Camera.additional_angle = 0;
        }
    }
}

static void M_FastBack(ITEM *const item, COLL_INFO *const coll)
{
    item->goal_anim_state = LS(LS_STOP);
    LARA_INFO *const lara = Lara_GetLaraInfo();
    if (g_Input.left) {
        lara->turn_rate -= LARA_TURN_RATE;
        CLAMPL(lara->turn_rate, -LARA_MED_TURN);
    } else if (g_Input.right) {
        lara->turn_rate += LARA_TURN_RATE;
        CLAMPG(lara->turn_rate, LARA_MED_TURN);
    }
}

static void M_Turn(ITEM *const item, COLL_INFO *const coll)
{
    if (item->hit_points <= 0) {
        item->goal_anim_state = LS(LS_STOP);
        return;
    }

    if (g_Config.gameplay.look_mode != LOOK_MODE_RESTRICTED && g_Input.look) {
        item->goal_anim_state = LS(LS_STOP);
        return;
    }

    const bool left_turn = item->current_anim_state == LS(LS_TURN_LEFT);
    const bool turn_input = left_turn ? g_Input.left : g_Input.right;

    LARA_INFO *const lara = Lara_GetLaraInfo();
    if (left_turn) {
        lara->turn_rate -= LARA_TURN_RATE;
    } else {
        lara->turn_rate += LARA_TURN_RATE;
    }

    if (lara->gun_status == LGS_READY) {
        item->goal_anim_state = LS(LS_FAST_TURN);
    } else if (left_turn && lara->turn_rate < -LARA_SLOW_TURN) {
        if (g_Input.slow) {
            lara->turn_rate = -LARA_SLOW_TURN;
        } else {
            item->goal_anim_state = LS(LS_FAST_TURN);
        }
    } else if (!left_turn && lara->turn_rate > LARA_SLOW_TURN) {
        if (g_Input.slow) {
            lara->turn_rate = LARA_SLOW_TURN;
        } else {
            item->goal_anim_state = LS(LS_FAST_TURN);
        }
    }

    if (g_Input.forward) {
        if (lara->water_status == LWS_WADE) {
            item->goal_anim_state = LS(LS_WADE);
        } else if (g_Input.slow) {
            item->goal_anim_state = LS(LS_WALK);
        } else {
            item->goal_anim_state = LS(LS_RUN);
        }
    } else if (!turn_input) {
        item->goal_anim_state = LS(LS_STOP);
    }
}

static void M_FastTurn(ITEM *const item, COLL_INFO *const coll)
{
    if (item->hit_points <= 0) {
        item->goal_anim_state = LS(LS_STOP);
        return;
    }

    if (g_Config.gameplay.look_mode != LOOK_MODE_RESTRICTED && g_Input.look) {
        item->goal_anim_state = LS(LS_STOP);
        return;
    }

    LARA_INFO *const lara = Lara_GetLaraInfo();
    if (lara->turn_rate >= 0) {
        lara->turn_rate = M_FAST_TURN;
        if (!g_Input.right) {
            item->goal_anim_state = LS(LS_STOP);
        }
    } else {
        lara->turn_rate = -M_FAST_TURN;
        if (!g_Input.left) {
            item->goal_anim_state = LS(LS_STOP);
        }
    }
}

static void M_SideStep(ITEM *const item, COLL_INFO *const coll)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    if (item->hit_points <= 0) {
        item->goal_anim_state = LS(LS_STOP);
        return;
    }

    const bool step_input = item->current_anim_state == LS(LS_STEP_LEFT)
        ? g_Input.step_left
        : g_Input.step_right;
    if (!step_input) {
        item->goal_anim_state = LS(LS_STOP);
    }

    if (g_Input.left) {
        lara->turn_rate -= LARA_TURN_RATE;
        CLAMPL(lara->turn_rate, -LARA_SLOW_TURN);
    } else if (g_Input.right) {
        lara->turn_rate += LARA_TURN_RATE;
        CLAMPG(lara->turn_rate, LARA_SLOW_TURN);
    }
}

static void M_Slide(ITEM *const item, COLL_INFO *const coll)
{
    const bool sliding_forward = item->current_anim_state == LS(LS_SLIDE);
    bool opposite_input;
    if (sliding_forward) {
        g_Camera.flags = CF_NO_CHUNKY;
        g_Camera.target_elevation = M_CAM_SLIDE_ELEVATION;
        opposite_input = g_Input.back;
    } else {
        opposite_input = g_Input.forward;
    }

    if (Item_TestAnimEqual(item, LA(LA_SLIDE_FORWARD_TO_RUN))) {
        item->goal_anim_state =
            LS(g_Input.sprint && g_Config.gameplay.enable_sprint ? LS_SPRINT
                                                                 : LS_RUN);
    } else if (
        sliding_forward && g_Config.gameplay.enable_slide_to_run
        && item->goal_anim_state == LS(LS_STOP) && g_Input.forward
        && Lara_State_IsResponsive(LA_SLIDE_FORWARD)) {
        item->goal_anim_state = LS(LS_RESPONSIVE);
    } else if (
        g_Input.jump
        && (!g_Config.gameplay.enable_jump_twists || !opposite_input)) {
        item->goal_anim_state =
            LS(sliding_forward ? LS_JUMP_FORWARD : LS_JUMP_BACK);
    }
}

static void M_Roll(ITEM *const item, COLL_INFO *const coll)
{
    coll->enable_hit = 0;
}

static void M_PushBlock(ITEM *const item, COLL_INFO *const coll)
{
    M_Default(item, coll);
    g_Camera.flags = CF_FOLLOW_CENTRE;
    g_Camera.target_angle = M_CAM_PUSH_BLOCK_ANGLE;
    g_Camera.target_elevation = M_CAM_PUSH_BLOCK_ELEVATION;
}

static void M_PPReady(ITEM *const item, COLL_INFO *const coll)
{
    M_Default(item, coll);
    g_Camera.target_angle = M_CAM_PP_READY_ANGLE;
    if (!g_Input.action) {
        item->goal_anim_state = LS(LS_STOP);
    }
}

static void M_Pickup(ITEM *const item, COLL_INFO *const coll)
{
    M_Default(item, coll);
    g_Camera.target_angle = M_CAM_PICKUP_ANGLE;
    g_Camera.target_elevation = M_CAM_PICKUP_ELEVATION;
    g_Camera.target_distance = M_CAM_PICKUP_DISTANCE;

    if (item->current_anim_state == LS(LS_FLARE_PICKUP)
        && Item_TestFrameEqual(item, -1)) {
        LARA_INFO *const lara = Lara_GetLaraInfo();
        lara->gun_status = LGS_ARMLESS;
    }
}

static void M_SwitchOn(ITEM *const item, COLL_INFO *const coll)
{
    M_Default(item, coll);
    g_Camera.target_angle = M_CAM_SWITCH_ON_ANGLE;
    g_Camera.target_elevation = M_CAM_SWITCH_ON_ELEVATION;
    g_Camera.target_distance = M_CAM_SWITCH_ON_DISTANCE;
    g_Camera.speed = M_CAM_SWITCH_ON_SPEED;
}

static void M_UseKey(ITEM *const item, COLL_INFO *const coll)
{
    M_Default(item, coll);
    g_Camera.target_angle = M_CAM_USE_KEY_ANGLE;
    g_Camera.target_elevation = M_CAM_USE_KEY_ELEVATION;
    g_Camera.target_distance = M_CAM_USE_KEY_DISTANCE;
}

static void M_Special(ITEM *const item, COLL_INFO *const coll)
{
    ITEM *const target_item = Lara_GetDeathCameraTarget();
    if (target_item != nullptr) {
        g_Camera.item = target_item;
        g_Camera.flags = CF_CHASE_OBJECT;
        g_Camera.type = CAM_FIXED;
        g_Camera.target_angle = item->rot.y;
        g_Camera.target_distance = M_CAM_SPECIAL_DISTANCE;
    } else {
        g_Camera.flags = CF_FOLLOW_CENTRE;
        g_Camera.target_angle = M_CAM_SPECIAL_ANGLE;
    }
    g_Camera.target_elevation = M_CAM_SPECIAL_ELEVATION;
}

static void M_Sprint(ITEM *const item, COLL_INFO *const coll)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();

    if (item->hit_points <= 0 || lara->sprint_timer <= 0
        || lara->water_status == LWS_WADE) {
        lara->sprinting = false;
        item->goal_anim_state = LS(LS_RUN);
        return;
    }

    if (g_Config.gameplay.enable_toggle_sprint
            ? (!lara->sprinting || g_InputDB.sprint)
            : !g_Input.sprint) {
        lara->sprinting = false;
        item->goal_anim_state = LS(LS_RUN);
        return;
    }

    if (!g_Config.debug.enable_endless_sprint) {
        lara->sprint_timer--;
    }

    if (M_RequestDuck(lara)) {
        item->goal_anim_state = M_GetRunToCrouchState();
        return;
    }
    if (g_Input.left) {
        lara->turn_rate -= M_SPRINT_TURN_RATE;
        CLAMPL(lara->turn_rate, -M_SPRINT_TURN_MAX);
        item->rot.z -= LARA_LEAN_RATE;
        CLAMPL(item->rot.z, -M_SPRINT_LEAN_MAX);
    } else if (g_Input.right) {
        lara->turn_rate += M_SPRINT_TURN_RATE;
        CLAMPG(lara->turn_rate, M_SPRINT_TURN_MAX);
        item->rot.z += LARA_LEAN_RATE;
        CLAMPG(item->rot.z, M_SPRINT_LEAN_MAX);
    }

    if (g_Input.jump && !item->gravity) {
        item->goal_anim_state = LS(LS_SPRINT_ROLL);
    } else if (g_Input.forward) {
        item->goal_anim_state = LS(g_Input.slow ? LS_WALK : LS_SPRINT);
    } else if (!g_Input.left && !g_Input.right) {
        item->goal_anim_state = LS(LS_STOP);
    }
}

static void M_SprintRoll(ITEM *const item, COLL_INFO *const coll)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->sprinting = false;

    if (item->goal_anim_state != LS(LS_DEATH)
        && item->goal_anim_state != LS(LS_STOP)
        && item->goal_anim_state != LS(LS_RUN)
        && item->fall_speed > M_FAST_FALL_SPEED) {
        item->goal_anim_state = LS(LS_FAST_FALL);
    }
}

// clang-format off
REGISTER_LARA_STATE(LS_PULL_UP,      M_Default)
REGISTER_LARA_STATE(LS_GYMNAST,      M_Default)
REGISTER_LARA_STATE(LS_WALK,         M_Walk)
REGISTER_LARA_STATE(LS_RUN,          M_Run)
REGISTER_LARA_STATE(LS_STOP,         M_Stop)
REGISTER_LARA_STATE(LS_POSE,         M_Pose)
REGISTER_LARA_STATE(LS_POSE_START,   M_Pose)
REGISTER_LARA_STATE(LS_POSE_END,     M_Pose)
REGISTER_LARA_STATE(LS_FAST_BACK,    M_FastBack)
REGISTER_LARA_STATE(LS_TURN_RIGHT,   M_Turn)
REGISTER_LARA_STATE(LS_TURN_LEFT,    M_Turn)
REGISTER_LARA_STATE(LS_FAST_TURN,    M_FastTurn)
REGISTER_LARA_STATE(LS_DEATH,        M_Default)
REGISTER_LARA_STATE(LS_WALK_BACK,    M_WalkBack)
REGISTER_LARA_STATE(LS_STEP_RIGHT,   M_SideStep)
REGISTER_LARA_STATE(LS_STEP_LEFT,    M_SideStep)
REGISTER_LARA_STATE(LS_SLIDE,        M_Slide)
REGISTER_LARA_STATE(LS_SLIDE_BACK,   M_Slide)
REGISTER_LARA_STATE(LS_ROLL,         M_Roll)
REGISTER_LARA_STATE(LS_ROLL_CONT,    M_Roll)
REGISTER_LARA_STATE(LS_PUSH_BLOCK,   M_PushBlock)
REGISTER_LARA_STATE(LS_PULL_BLOCK,   M_PushBlock)
REGISTER_LARA_STATE(LS_PP_READY,     M_PPReady)
REGISTER_LARA_STATE(LS_PICKUP,       M_Pickup)
REGISTER_LARA_STATE(LS_SWITCH_ON,    M_SwitchOn)
REGISTER_LARA_STATE(LS_SWITCH_OFF,   M_SwitchOn)
REGISTER_LARA_STATE(LS_USE_KEY,      M_UseKey)
REGISTER_LARA_STATE(LS_USE_PUZZLE,   M_UseKey)
REGISTER_LARA_STATE(LS_SPECIAL,      M_Special)
REGISTER_LARA_STATE(LS_WADE,         M_Wade)
REGISTER_LARA_STATE(LS_SPRINT,       M_Sprint)
REGISTER_LARA_STATE(LS_SPRINT_ROLL,  M_SprintRoll)
REGISTER_LARA_STATE(LS_CONTROLLED,   M_Default)
REGISTER_LARA_STATE(LS_FLARE_PICKUP, M_Pickup)
// clang-format on
