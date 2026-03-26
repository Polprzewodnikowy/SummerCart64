#include <trx/config.h>
#include <trx/game/input.h>
#include <trx/game/lara.h>
#include <trx/game/lara/util.h>
#include <trx/game/rooms.h>
#include <trx/game/sound.h>
#include <trx/version.h>

#define M_LF_WALK_STEP_L_START 0
#define M_LF_WALK_STEP_L_NEAR_END 5
#define M_LF_WALK_STEP_L_END 6
#define M_LF_WALK_STEP_R_START 7
#define M_LF_WALK_STEP_R_MID 22
#define M_LF_WALK_STEP_R_NEAR_END 23
#define M_LF_WALK_STEP_R_END 25
#define M_LF_WALK_STEP_L_2_START 26
#define M_LF_WALK_STEP_L_2_END 35
#define M_LF_WALK_BACK_R_START 26
#define M_LF_WALK_BACK_R_END 55

#define M_LF_RUN_L_START 0
#define M_LF_RUN_L_HEEL_GROUND 3
#define M_LF_RUN_L_END 9
#define M_LF_RUN_R_START 10
#define M_LF_RUN_R_FOOT_GROUND 14
#define M_LF_RUN_R_END 21

#define M_LF_WADE_L_START 0
#define M_LF_WADE_L_END 9
#define M_LF_WADE_R_START 10
#define M_LF_WADE_R_END 21
#define M_LF_WADE_STEP_L_START 3
#define M_LF_WADE_STEP_L_END 14

#define M_LF_SPRINT_STEP_L_START 4
#define M_LF_SPRINT_STEP_L_END 13

#define M_CONTROLLED_DROP_MIN_HEIGHT (LARA_HEIGHT + (STEP_L * 3) / 4) // 954

static int16_t m_OldSlideAngle = 1;

static bool M_TestWall(
    const ITEM *const item, const int32_t front, const int32_t right,
    const int32_t down)
{
    XYZ_32 pos = item->pos;
    pos.y += down;

    const DIRECTION dir = Math_GetDirection(item->rot.y);
    switch (dir) {
    case DIR_NORTH:
        pos.x -= right;
        break;
    case DIR_EAST:
        pos.z -= right;
        break;
    case DIR_SOUTH:
        pos.x += right;
        break;
    case DIR_WEST:
        pos.z += right;
        break;
    default:
        break;
    }

    int16_t room_num = item->room_num;
    Room_GetSector(pos, &room_num);

    switch (dir) {
    case DIR_NORTH:
        pos.z += front;
        break;
    case DIR_EAST:
        pos.x += front;
        break;
    case DIR_SOUTH:
        pos.z -= front;
        break;
    case DIR_WEST:
        pos.x -= front;
        break;
    default:
        break;
    }

    const SECTOR *const sector = Room_GetSector(pos, &room_num);
    const int32_t height = Room_GetHeight(sector, pos);
    const int32_t ceiling = Room_GetCeiling(sector, pos);
    if (height != NO_HEIGHT && height - pos.y > 0 && ceiling - pos.y < 0) {
        return false;
    }
    return true;
}

static bool M_CanControlDrop(
    const ITEM *const item, const COLL_INFO *const coll)
{
    const LARA_INFO *const lara = Lara_GetLaraInfo();
    if (!g_Input.action || lara->gun_status != LGS_ARMLESS
        || !g_Config.gameplay.enable_controlled_drops
        || coll->side_mid.floor < M_CONTROLLED_DROP_MIN_HEIGHT) {
        return false;
    }

    COLL_INFO old_coll = {
        .facing = lara->move_angle,
        .bad_pos = STEPUP_HEIGHT,
        .bad_neg = -STEPUP_HEIGHT,
        .slopes_are_pits = 1,
        .slopes_are_walls = 1,
    };
    Collide_GetCollisionInfo(
        &old_coll, coll->old.x, coll->old.y, coll->old.z, item->room_num,
        LARA_HEIGHT);

    if (old_coll.side_mid.floor != 0) {
        return false;
    }

    const DIRECTION dir =
        Math_GetDirectionCone(item->rot.y + DEG_180, LARA_HANG_ANGLE);
    if (dir == DIR_UNKNOWN) {
        return false;
    }

    switch (old_coll.quadrant) {
    case DIR_NORTH:
    case DIR_SOUTH:
        return ABS(old_coll.tilt_x) < 2;
    case DIR_EAST:
    case DIR_WEST:
        return ABS(old_coll.tilt_z) < 2;
    default:
        return false;
    }
}

bool Lara_Col_Fallen(ITEM *const item, const COLL_INFO *const coll)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    if (coll->side_mid.floor <= STEPUP_HEIGHT
        || lara->water_status == LWS_WADE) {
        return false;
    }
    if (M_CanControlDrop(item, coll)) {
        item->current_anim_state = LS(LS_REACH);
        item->goal_anim_state = LS(LS_REACH);
        Item_SwitchToAnim(item, LA(LA_CONTROLLED_DROP), 0);
        item->speed = 2;
    } else {
        item->current_anim_state = LS(LS_JUMP_FORWARD);
        item->goal_anim_state = LS(LS_JUMP_FORWARD);
        Item_SwitchToAnim(item, LA(LA_FALL_START), 0);
    }
    item->gravity = true;
    item->fall_speed = 0;
    lara->sprinting = false;
    lara->crouching = false;
    return true;
}

bool Lara_Col_TestSlide(ITEM *const item, COLL_INFO *const coll)
{
    if (ABS(coll->tilt_x) <= 2 && ABS(coll->tilt_z) <= 2) {
        return false;
    }

    const ROOM *const room = Room_Get(item->room_num);
    if (room->flags.swamp) {
        return false;
    }

    int16_t angle = 0;
    if (coll->tilt_x > 2) {
        angle = -DEG_90;
    } else if (coll->tilt_x < -2) {
        angle = DEG_90;
    }

    if (coll->tilt_z > 2 && coll->tilt_z > ABS(coll->tilt_x)) {
        angle = -DEG_180;
    } else if (coll->tilt_z < -2 && -coll->tilt_z > ABS(coll->tilt_x)) {
        angle = 0;
    }

    const int16_t angle_dif = angle - item->rot.y;
    Lara_Col_Shift(coll);

    LARA_INFO *const lara = Lara_GetLaraInfo();
    if (angle_dif >= -DEG_90 && angle_dif <= DEG_90) {
        if (item->current_anim_state == LS(LS_SLIDE)
            && m_OldSlideAngle == angle) {
            lara->sprinting = false;
            lara->crouching = false;
            return true;
        }
        item->goal_anim_state = LS(LS_SLIDE);
        item->current_anim_state = LS(LS_SLIDE);
        Item_SwitchToAnim(item, LA(LA_SLIDE_FORWARD), 0);
        item->rot.y = angle;
    } else {
        if (item->current_anim_state == LS(LS_SLIDE_BACK)
            && m_OldSlideAngle == angle) {
            lara->sprinting = false;
            lara->crouching = false;
            return true;
        }
        item->goal_anim_state = LS(LS_SLIDE_BACK);
        item->current_anim_state = LS(LS_SLIDE_BACK);
        Item_SwitchToAnim(item, LA(LA_SLIDE_BACKWARD_START), 0);
        item->rot.y = angle + DEG_180;
    }

    lara->move_angle = angle;
    lara->sprinting = false;
    lara->crouching = false;
    m_OldSlideAngle = angle;
    return true;
}

static bool M_DeflectEdge(ITEM *const item, COLL_INFO *const coll)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();

    switch (coll->coll_type) {
    case COLL_FRONT:
    case COLL_TOP_FRONT:
        Lara_Col_Shift(coll);
        item->goal_anim_state = LS(LS_STOP);
        item->current_anim_state = LS(LS_STOP);
        item->gravity = false;
        item->speed = 0;
        return true;

    case COLL_LEFT:
        Lara_Col_Shift(coll);
        item->rot.y += LARA_DEFLECT_ANGLE;
        return false;

    case COLL_RIGHT:
        Lara_Col_Shift(coll);
        item->rot.y -= LARA_DEFLECT_ANGLE;
        return false;

    default:
        return false;
    }
}

bool Lara_Col_TestCeiling(ITEM *const item, const COLL_INFO *const coll)
{
    if (coll->coll_type != COLL_TOP && coll->coll_type != COLL_CLAMP) {
        return false;
    }

    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->sprinting = false;
    lara->crouching = false;

    item->pos = coll->old;
    item->goal_anim_state = LS(LS_STOP);
    item->current_anim_state = LS(LS_STOP);
    Item_SwitchToAnim(item, LA(LA_STAND_STILL), 0);
    item->speed = 0;
    item->gravity = false;
    item->fall_speed = 0;
    return true;
}

static void M_CollideStop(ITEM *const item, const COLL_INFO *const coll)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->sprinting = false;
    lara->crouching = false;

    if (g_Config.gameplay.enable_smooth_wall_deflect) {
        switch (LS_U(coll->old_anim_state)) {
        case LS_STOP:
        case LS_TURN_RIGHT:
        case LS_TURN_LEFT:
        case LS_FAST_TURN:
            item->current_anim_state = coll->old_anim_state;
            item->anim_num = coll->old_anim_num;
            item->frame_num = coll->old_frame_num;
            if (g_Input.left) {
                item->goal_anim_state = LS(LS_TURN_LEFT);
            } else if (g_Input.right) {
                item->goal_anim_state = LS(LS_TURN_RIGHT);
            } else {
                item->goal_anim_state = LS(LS_STOP);
            }
            Lara_Animate(item);
            return;

        default:
            break;
        }
    }

    Item_SwitchToAnim(item, LA(LA_STAND_STILL), 0);
}

static void M_Default(ITEM *const item, COLL_INFO *const coll)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->move_angle = item->rot.y;
    coll->bad_pos = STEPUP_HEIGHT;
    coll->bad_neg = -STEPUP_HEIGHT;
    coll->bad_ceiling = 0;
    coll->slopes_are_pits = 1;
    coll->slopes_are_walls = 1;
    Lara_Col_GetInfo(item, coll);
}

static void M_PullUp(ITEM *const item, COLL_INFO *const coll)
{
    M_Default(item, coll);
    if (Item_TestAnimEqual(item, LA(LA_CLIMB_2CLICK))
        && Item_TestFrameEqual(item, -1)) {
        Lara_UpdateRoomToHeight(-WALL_L);
        Lara_Animate(item);
    }
}

static void M_Walk(ITEM *const item, COLL_INFO *const coll)
{
    item->gravity = false;
    item->fall_speed = 0;
    coll->lava_is_pit = 1;
    M_Default(item, coll);

    if (Lara_Col_TestCeiling(item, coll) || Lara_Col_TestVault(item, coll)) {
        return;
    }

    if (M_DeflectEdge(item, coll)) {
        if (Item_TestAnimEqual(item, LA(LA_WALK_FORWARD))
            && Item_TestFrameRange(
                item, M_LF_WALK_STEP_R_START, M_LF_WALK_STEP_R_END)) {
            Item_SwitchToAnim(item, LA(LA_WALK_STOP_RIGHT), 0);
        } else if (
            Item_TestAnimEqual(item, LA(LA_WALK_FORWARD))
            && (Item_TestFrameRange(
                    item, M_LF_WALK_STEP_L_START, M_LF_WALK_STEP_L_END)
                || Item_TestFrameRange(
                    item, M_LF_WALK_STEP_L_2_START, M_LF_WALK_STEP_L_2_END))) {
            Item_SwitchToAnim(item, LA(LA_WALK_STOP_LEFT), 0);
        } else {
            M_CollideStop(item, coll);
        }
    }

    if (Lara_Col_Fallen(item, coll)) {
        return;
    }

    if (coll->side_mid.floor > STEP_L / 2) {
        if (Item_TestAnimEqual(item, LA(LA_WALK_FORWARD))
            && Item_TestFrameRange(
                item, M_LF_WALK_STEP_L_END, M_LF_WALK_STEP_R_NEAR_END)) {
            Item_SwitchToAnim(item, LA(LA_WALK_DOWN_LEFT), 0);
        } else {
            Item_SwitchToAnim(item, LA(LA_WALK_DOWN_RIGHT), 0);
        }
    }

    if (coll->side_mid.floor >= -STEPUP_HEIGHT
        && coll->side_mid.floor < -STEP_L / 2) {
        if (Item_TestAnimEqual(item, LA(LA_WALK_FORWARD))
            && Item_TestFrameRange(
                item, M_LF_WALK_STEP_L_NEAR_END, M_LF_WALK_STEP_R_MID)) {
            Item_SwitchToAnim(item, LA(LA_WALK_UP_STEP_LEFT), 0);
        } else {
            Item_SwitchToAnim(item, LA(LA_WALK_UP_STEP_RIGHT), 0);
        }
    }

    if (Lara_Col_TestSlide(item, coll)) {
        return;
    }

    item->pos.y += coll->side_mid.floor;
}

static void M_WalkBack(ITEM *const item, COLL_INFO *const coll)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->move_angle = item->rot.y + DEG_180;
    item->gravity = false;
    item->fall_speed = 0;
    if (lara->water_status == LWS_WADE) {
        coll->bad_pos = NO_BAD_POS;
    } else {
        coll->bad_pos = STEPUP_HEIGHT;
    }
    coll->slopes_are_pits = 1;
    coll->slopes_are_walls = 1;
    coll->bad_neg = -STEPUP_HEIGHT;
    coll->bad_ceiling = 0;
    coll->lava_is_pit = 1;

    Lara_Col_GetInfo(item, coll);
    if (Lara_Col_TestCeiling(item, coll)) {
        return;
    }

    if (M_DeflectEdge(item, coll)) {
        M_CollideStop(item, coll);
    }

    if (g_Config.gameplay.fix_descending_glitch
        && Lara_Col_Fallen(item, coll)) {
        return;
    }

    if (coll->side_mid.floor > STEP_L / 2
        && coll->side_mid.floor < STEPUP_HEIGHT) {
        if (Item_TestFrameRange(
                item, M_LF_WALK_BACK_R_START, M_LF_WALK_BACK_R_END)) {
            Item_SwitchToAnim(item, LA(LA_WALK_DOWN_BACK_RIGHT), 0);
        } else {
            Item_SwitchToAnim(item, LA(LA_WALK_DOWN_BACK_LEFT), 0);
        }
    }

    if (Lara_Col_TestSlide(item, coll)) {
        return;
    }

    const ROOM *const room = Room_Get(item->room_num);
    if (coll->side_mid.floor >= 0 && room->flags.swamp) {
        item->pos.y += 2;
    } else if (lara->water_status == LWS_WADE && coll->side_mid.floor >= 50) {
        item->pos.y += 50;
    } else {
        item->pos.y += coll->side_mid.floor;
    }
}

static void M_SideStep(ITEM *const item, COLL_INFO *const coll)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    if (item->current_anim_state == LS(LS_STEP_RIGHT)) {
        lara->move_angle = item->rot.y + DEG_90;
    } else {
        lara->move_angle = item->rot.y - DEG_90;
    }

    item->gravity = false;
    item->fall_speed = 0;
    if (lara->water_status == LWS_WADE) {
        coll->bad_pos = NO_BAD_POS;
    } else {
        coll->bad_pos = STEP_L / 2;
    }
    coll->slopes_are_pits = 1;
    coll->slopes_are_walls = 1;
    coll->bad_neg = -STEP_L / 2;
    coll->bad_ceiling = 0;
    coll->lava_is_pit = 1;

    Lara_Col_GetInfo(item, coll);
    if (Lara_Col_TestCeiling(item, coll)) {
        return;
    }

    if (M_DeflectEdge(item, coll)) {
        M_CollideStop(item, coll);
    }

    if (g_Config.gameplay.fix_descending_glitch
        && Lara_Col_Fallen(item, coll)) {
        return;
    }

    if (!Lara_Col_TestSlide(item, coll)) {
        item->pos.y += coll->side_mid.floor;
    }
}

static void M_Run(ITEM *const item, COLL_INFO *const coll)
{
    if (g_Config.gameplay.fix_qwop_glitch) {
        item->gravity = false;
        item->fall_speed = 0;
    }

    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->move_angle = item->rot.y;
    coll->slopes_are_walls = 1;
    coll->bad_pos = NO_BAD_POS;
    coll->bad_neg = -STEPUP_HEIGHT;
    coll->bad_ceiling = 0;
    Lara_Col_GetInfo(item, coll);

    if (Lara_Col_TestCeiling(item, coll) || Lara_Col_TestVault(item, coll)) {
        return;
    }

    if (M_DeflectEdge(item, coll)) {
        item->rot.z = 0;
        if (M_TestWall(item, STEP_L, 0, -STEP_L * 5 / 2)) {
            item->current_anim_state = LS(LS_SPLAT);
            const bool is_run_anim = Item_TestAnimEqual(item, LA(LA_RUN));
            if (is_run_anim
                && Item_TestFrameRange(
                    item, M_LF_RUN_L_START, M_LF_RUN_L_END)) {
                Item_SwitchToAnim(item, LA(LA_WALL_SMASH_LEFT), 0);
                return;
            }
            if (is_run_anim
                && Item_TestFrameRange(
                    item, M_LF_RUN_R_START, M_LF_RUN_R_END)) {
                Item_SwitchToAnim(item, LA(LA_WALL_SMASH_RIGHT), 0);
                return;
            }
        }
        M_CollideStop(item, coll);
    }

    if (Lara_Col_Fallen(item, coll)) {
        return;
    }

    if (coll->side_mid.floor >= -STEPUP_HEIGHT
        && coll->side_mid.floor < -STEP_L / 2) {
        if (g_Config.gameplay.fix_step_glitch
            && (coll->side_front.floor < -STEPUP_HEIGHT
                || coll->side_front.floor >= -STEP_L / 2)) {
            coll->side_mid.floor = 0;
        } else {
            if (Item_TestFrameRange(
                    item, M_LF_RUN_L_HEEL_GROUND, M_LF_RUN_R_FOOT_GROUND)) {
                Item_SwitchToAnim(item, LA(LA_RUN_UP_STEP_LEFT), 0);
            } else {
                Item_SwitchToAnim(item, LA(LA_RUN_UP_STEP_RIGHT), 0);
            }
        }
    }

    if (Lara_Col_TestSlide(item, coll)) {
        return;
    }

    item->pos.y += MIN(coll->side_mid.floor, 50);
}

static void M_Stop(ITEM *const item, COLL_INFO *const coll)
{
    item->gravity = false;
    item->fall_speed = 0;
    M_Default(item, coll);

    if (Lara_Col_TestCeiling(item, coll) || Lara_Col_Fallen(item, coll)
        || Lara_Col_TestSlide(item, coll)) {
        return;
    }

    const ROOM *const room = Room_Get(item->room_num);
    if (!room->flags.swamp && g_Config.gameplay.fix_step_glitch
        && coll->side_mid.floor > 100) {
        item->current_anim_state = LS(LS_JUMP_FORWARD);
        item->goal_anim_state = LS(LS_JUMP_FORWARD);
        Item_SwitchToAnim(item, LA(LA_FALL_START), 0);
        item->gravity = true;
        item->fall_speed = 0;
        return;
    }

    Lara_Col_Shift(coll);
    if (room->flags.swamp && coll->side_mid.floor >= 0) {
        item->pos.y += 2;
        CLAMPG(item->pos.y, item->floor);
    } else {
        item->pos.y += coll->side_mid.floor;
    }
}

static void M_FastBack(ITEM *const item, COLL_INFO *const coll)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->move_angle = item->rot.y + DEG_180;
    item->gravity = false;
    item->fall_speed = 0;
    coll->slopes_are_pits = 1;
    coll->slopes_are_walls = !g_Config.gameplay.enable_back_slope_stumble;
    coll->bad_pos = NO_BAD_POS;
    coll->bad_neg = -STEPUP_HEIGHT;
    coll->bad_ceiling = 0;

    Lara_Col_GetInfo(item, coll);
    if (Lara_Col_TestCeiling(item, coll)) {
        return;
    }

    if (coll->side_mid.floor <= 200) {
        if (!g_Config.gameplay.enable_back_slope_stumble
            || !Lara_Col_TestSlide(item, coll)) {
            if (M_DeflectEdge(item, coll)) {
                M_CollideStop(item, coll);
            }
            item->pos.y += coll->side_mid.floor;
        }
    } else {
        Item_SwitchToAnim(item, LA(LA_FALL_BACK), 0);
        item->current_anim_state = LS(LS_FALL_BACK);
        item->goal_anim_state = LS(LS_FALL_BACK);
        item->gravity = true;
        item->fall_speed = 0;
    }
}

static void M_Turn(ITEM *const item, COLL_INFO *const coll)
{
    item->gravity = false;
    item->fall_speed = 0;
    M_Default(item, coll);

    const ROOM *const room = Room_Get(item->room_num);
    if (coll->side_mid.floor > 100 && !room->flags.swamp) {
        Item_SwitchToAnim(item, LA(LA_FALL_START), 0);
        item->current_anim_state = LS(LS_JUMP_FORWARD);
        item->goal_anim_state = LS(LS_JUMP_FORWARD);
        item->gravity = true;
        item->fall_speed = 0;
        return;
    }

    if (Lara_Col_TestSlide(item, coll)) {
        return;
    }

    if (coll->side_mid.floor < 0 || !room->flags.swamp) {
        item->pos.y += coll->side_mid.floor;
    } else {
        item->pos.y += 2;
    }
}

static void M_Death(ITEM *const item, COLL_INFO *const coll)
{
    if (g_TRVersion >= 2) {
        Sound_StopEffect(SFX_LARA_FALL);
    }
    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->move_angle = item->rot.y;
    coll->bad_pos = STEPUP_HEIGHT;
    coll->bad_neg = -STEPUP_HEIGHT;
    coll->bad_ceiling = 0;
    coll->radius = LARA_RADIUS * 4;

    Lara_Col_GetInfo(item, coll);
    Lara_Col_Shift(coll);

    item->pos.y += coll->side_mid.floor;
    item->hit_points = -1;
    lara->air = -1;
}

static void M_Splat(ITEM *const item, COLL_INFO *const coll)
{
    M_Default(item, coll);
    Lara_Col_Shift(coll);
    if (!g_Config.gameplay.fix_step_glitch && coll->side_mid.floor > -STEP_L
        && coll->side_mid.floor < STEP_L) {
        item->pos.y += coll->side_mid.floor;
    }
}

static void M_Slide(ITEM *const item, COLL_INFO *const coll)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->move_angle = item->rot.y;
    if (item->current_anim_state == LS(LS_SLIDE_BACK)) {
        lara->move_angle += DEG_180;
    }

    coll->bad_pos = NO_BAD_POS;
    coll->bad_neg = -STEP_L * 2;
    coll->bad_ceiling = 0;
    Lara_Col_GetInfo(item, coll);

    if (Lara_Col_TestCeiling(item, coll)) {
        return;
    }

    M_DeflectEdge(item, coll);

    if (coll->side_mid.floor > 200) {
        if (item->current_anim_state == LS(LS_SLIDE)) {
            if (M_CanControlDrop(item, coll)) {
                item->current_anim_state = LS(LS_REACH);
                item->goal_anim_state = LS(LS_REACH);
                Item_SwitchToAnim(item, LA(LA_CONTROLLED_DROP), 2);
                item->speed = 2;
            } else {
                item->goal_anim_state = LS(LS_JUMP_FORWARD);
                item->current_anim_state = LS(LS_JUMP_FORWARD);
                Item_SwitchToAnim(item, LA(LA_FALL_START), 0);
            }
        } else {
            item->goal_anim_state = LS(LS_FALL_BACK);
            item->current_anim_state = LS(LS_FALL_BACK);
            Item_SwitchToAnim(item, LA(LA_FALL_BACK), 0);
        }
        item->gravity = true;
        item->fall_speed = 0;
        return;
    }

    Lara_Col_TestSlide(item, coll);
    item->pos.y += coll->side_mid.floor;
    if (ABS(coll->tilt_x) <= 2 && ABS(coll->tilt_z) <= 2) {
        item->goal_anim_state = LS(LS_STOP);
    }
}

static void M_Roll(ITEM *const item, COLL_INFO *const coll)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->move_angle = item->rot.y;
    item->gravity = false;
    item->fall_speed = 0;
    coll->bad_pos = NO_BAD_POS;
    coll->bad_neg = -STEPUP_HEIGHT;
    coll->bad_ceiling = 0;
    coll->slopes_are_walls = 1;

    Lara_Col_GetInfo(item, coll);
    if (Lara_Col_TestCeiling(item, coll) || Lara_Col_TestSlide(item, coll)) {
        return;
    }

    if (g_Config.gameplay.enable_step_roll_boost) {
        if (coll->side_mid.floor > 200) {
            item->current_anim_state = LS(LS_JUMP_FORWARD);
            item->goal_anim_state = LS(LS_JUMP_FORWARD);
            Item_SwitchToAnim(item, LA(LA_FALL_START), 0);
            item->gravity = true;
            item->fall_speed = 0;
            return;
        }
    } else if (Lara_Col_Fallen(item, coll)) {
        return;
    }

    Lara_Col_Shift(coll);
    item->pos.y += coll->side_mid.floor;
}

static void M_RollContinue(ITEM *const item, COLL_INFO *const coll)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    item->gravity = false;
    item->fall_speed = 0;
    lara->move_angle = item->rot.y + DEG_180;
    coll->slopes_are_walls = 1;
    coll->bad_pos = NO_BAD_POS;
    coll->bad_neg = -STEPUP_HEIGHT;
    coll->bad_ceiling = 0;

    Lara_Col_GetInfo(item, coll);
    if (Lara_Col_TestCeiling(item, coll) || Lara_Col_TestSlide(item, coll)) {
        return;
    }

    if (coll->side_mid.floor > 200) {
        Item_SwitchToAnim(item, LA(LA_FALL_BACK), 0);
        item->current_anim_state = LS(LS_FALL_BACK);
        item->goal_anim_state = LS(LS_FALL_BACK);
        item->gravity = true;
        item->fall_speed = 0;
    } else {
        Lara_Col_Shift(coll);
        item->pos.y += coll->side_mid.floor;
    }
}

static void M_Wade(ITEM *const item, COLL_INFO *const coll)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->move_angle = item->rot.y;
    coll->slopes_are_walls = 1;
    coll->bad_pos = NO_BAD_POS;
    coll->bad_neg = -STEPUP_HEIGHT;
    coll->bad_ceiling = 0;

    Lara_Col_GetInfo(item, coll);
    if (Lara_Col_TestCeiling(item, coll) || Lara_Col_TestVault(item, coll)) {
        return;
    }

    const ROOM *const room = Room_Get(item->room_num);
    if (M_DeflectEdge(item, coll)) {
        item->rot.z = 0;
        if (g_Config.gameplay.fix_wade_wall_hit
            && (coll->side_front.type == HT_WALL
                || coll->side_front.type == HT_SPLIT_TRI)
            && coll->side_front.floor < -STEP_L * 5 / 2
            && coll->old_anim_state == LS(LS_WADE)
            && Item_TestAnimEqual(item, LA(LA_WADE)) && !room->flags.swamp) {
            item->current_anim_state = LS(LS_SPLAT);
            if (Item_TestFrameRange(item, M_LF_WADE_L_START, M_LF_WADE_L_END)) {
                Item_SwitchToAnim(item, LA(LA_WALL_SMASH_LEFT), 0);
                return;
            }
            if (Item_TestFrameRange(item, M_LF_WADE_R_START, M_LF_WADE_R_END)) {
                Item_SwitchToAnim(item, LA(LA_WALL_SMASH_RIGHT), 0);
                return;
            }
        }
        M_CollideStop(item, coll);
    }

    if (!room->flags.swamp && Lara_Col_Fallen(item, coll)) {
        return;
    }

    if (coll->side_mid.floor >= -STEPUP_HEIGHT
        && coll->side_mid.floor < -STEP_L / 2 && !room->flags.swamp) {
        if (Item_TestFrameRange(
                item, M_LF_WADE_STEP_L_START, M_LF_WADE_STEP_L_END)) {
            Item_SwitchToAnim(item, LA(LA_RUN_UP_STEP_LEFT), 0);
        } else {
            Item_SwitchToAnim(item, LA(LA_RUN_UP_STEP_RIGHT), 0);
        }
    }

    if (Lara_Col_TestSlide(item, coll)) {
        return;
    }

    if (coll->side_mid.floor >= 50 && !room->flags.swamp) {
        item->pos.y += 50;
    } else if (coll->side_mid.floor < 0 || !room->flags.swamp) {
        item->pos.y += coll->side_mid.floor;
    } else {
        item->pos.y += 2;
    }
}

static void M_Sprint(ITEM *const item, COLL_INFO *const coll)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->move_angle = item->rot.y;
    coll->bad_pos = NO_BAD_POS;
    coll->bad_neg = -STEPUP_HEIGHT;
    coll->bad_ceiling = 0;
    coll->slopes_are_walls = 1;

    Lara_Col_GetInfo(item, coll);
    if (Lara_Col_TestCeiling(item, coll) || Lara_Col_TestVault(item, coll)) {
        return;
    }

    if (M_DeflectEdge(item, coll)) {
        item->rot.z = 0;
        if (M_TestWall(item, STEP_L, 0, -STEP_L * 5 / 2)) {
            Item_SwitchToAnim(item, LA(LA_WALL_SMASH_LEFT), 0);
            lara->sprinting = false;
            return;
        }

        M_CollideStop(item, coll);
    }

    if (Lara_Col_Fallen(item, coll)) {
        return;
    }

    if (!g_Config.gameplay.enable_responsive_sprint
        && coll->side_mid.floor >= -STEPUP_HEIGHT
        && coll->side_mid.floor < -STEP_L / 2) {
        if (Item_TestFrameRange(
                item, M_LF_SPRINT_STEP_L_START, M_LF_SPRINT_STEP_L_END)) {
            Item_SwitchToAnim(item, LA(LA_RUN_UP_STEP_LEFT), 0);
        } else {
            Item_SwitchToAnim(item, LA(LA_RUN_UP_STEP_RIGHT), 0);
        }
    }

    if (Lara_Col_TestSlide(item, coll)) {
        return;
    }

    item->pos.y += MIN(coll->side_mid.floor, 50);
}

static void M_SprintRoll(ITEM *const item, COLL_INFO *const coll)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->move_angle = item->rot.y;
    if (item->speed < 0) {
        lara->move_angle += DEG_180;
    }
    coll->bad_pos = NO_BAD_POS;
    coll->bad_neg = -STEP_L;
    coll->bad_ceiling = STEPUP_HEIGHT / 2;
    coll->slopes_are_walls = 1;

    Lara_Col_GetInfo(item, coll);
    Lara_Col_DeflectEdgeJump(item, coll);
    if (Lara_Col_Fallen(item, coll)) {
        return;
    }

    if (item->speed < 0) {
        lara->move_angle = item->rot.y;
    }

    if (coll->side_mid.floor <= 0 && item->fall_speed > 0) {
        if (Lara_Col_LandedBad(item)) {
            item->goal_anim_state = LS(LS_DEATH);
        } else if (
            lara->water_status == LWS_WADE || !g_Input.forward
            || g_Input.slow) {
            item->goal_anim_state = LS(LS_STOP);
        } else {
            item->goal_anim_state = LS(LS_RUN);
        }

        item->fall_speed = 0;
        item->gravity = false;
        item->speed = 0;
        item->pos.y += coll->side_mid.floor;
        Lara_Animate(item);
    }

    Lara_Col_Shift(coll);
    item->pos.y += coll->side_mid.floor;
}

// clang-format off
REGISTER_LARA_COL(LS_PUSH_BLOCK,   M_Default)
REGISTER_LARA_COL(LS_PULL_BLOCK,   M_Default)
REGISTER_LARA_COL(LS_PP_READY,     M_Default)
REGISTER_LARA_COL(LS_PICKUP,       M_Default)
REGISTER_LARA_COL(LS_SWITCH_ON,    M_Default)
REGISTER_LARA_COL(LS_SWITCH_OFF,   M_Default)
REGISTER_LARA_COL(LS_USE_KEY,      M_Default)
REGISTER_LARA_COL(LS_USE_PUZZLE,   M_Default)
REGISTER_LARA_COL(LS_USE_MIDAS,    M_Default)
REGISTER_LARA_COL(LS_DIE_MIDAS,    M_Default)
REGISTER_LARA_COL(LS_GYMNAST,      M_Default)
REGISTER_LARA_COL(LS_WATER_OUT,    M_Default)
REGISTER_LARA_COL(LS_PULL_UP,      M_PullUp)
REGISTER_LARA_COL(LS_CONTROLLED,   M_Default)
REGISTER_LARA_COL(LS_FLARE_PICKUP, M_Default)
REGISTER_LARA_COL(LS_WALK,         M_Walk)
REGISTER_LARA_COL(LS_WALK_BACK,    M_WalkBack)
REGISTER_LARA_COL(LS_STEP_RIGHT,   M_SideStep)
REGISTER_LARA_COL(LS_STEP_LEFT,    M_SideStep)
REGISTER_LARA_COL(LS_RUN,          M_Run)
REGISTER_LARA_COL(LS_STOP,         M_Stop)
REGISTER_LARA_COL(LS_POSE,         M_Stop)
REGISTER_LARA_COL(LS_POSE_START,   M_Stop)
REGISTER_LARA_COL(LS_POSE_END,     M_Stop)
REGISTER_LARA_COL(LS_LAND,         M_Stop)
REGISTER_LARA_COL(LS_FAST_TURN,    M_Stop)
REGISTER_LARA_COL(LS_FAST_BACK,    M_FastBack)
REGISTER_LARA_COL(LS_TURN_RIGHT,   M_Turn)
REGISTER_LARA_COL(LS_TURN_LEFT,    M_Turn)
REGISTER_LARA_COL(LS_DEATH,        M_Death)
REGISTER_LARA_COL(LS_SPLAT,        M_Splat)
REGISTER_LARA_COL(LS_SLIDE,        M_Slide)
REGISTER_LARA_COL(LS_SLIDE_BACK,   M_Slide)
REGISTER_LARA_COL(LS_ROLL,         M_Roll)
REGISTER_LARA_COL(LS_ROLL_CONT,    M_RollContinue)
REGISTER_LARA_COL(LS_WADE,         M_Wade)
REGISTER_LARA_COL(LS_SPRINT,       M_Sprint)
REGISTER_LARA_COL(LS_SPRINT_ROLL,  M_SprintRoll)
// clang-format on
