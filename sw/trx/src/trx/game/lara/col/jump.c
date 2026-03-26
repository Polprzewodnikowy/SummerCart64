#include <trx/config.h>
#include <trx/game/input.h>
#include <trx/game/lara.h>
#include <trx/game/lara/util.h>
#include <trx/game/rooms.h>
#include <trx/game/rooms/enum.h>
#include <trx/game/rooms/utils.h>
#include <trx/game/sound.h>
#include <trx/version.h>

// clang-format off
#define M_LF_START_HANG    12
#define M_LF_FAST_FALL     1
#define M_BAD_JUMP_CEILING ((STEP_L * 3) / 4) // = 192
#define M_HEAD_CLEARANCE   (-STEP_L / 8) // = -32
#define M_LADDER_CLEARANCE (-STEPUP_HEIGHT) // = -384
// clang-format on

EDGE_CATCH Lara_Col_TestEdgeCatch(
    const ITEM *const item, const COLL_INFO *const coll, int32_t *const edge)
{
    const BOUNDS_16 *const bounds = Item_GetBoundsAccurate(item);
    int32_t hdif1 = coll->side_front.floor - bounds->min.y;
    int32_t hdif2 = hdif1 + item->fall_speed;
    if ((hdif1 < 0 && hdif2 < 0) || (hdif1 > 0 && hdif2 > 0)) {
        hdif1 = item->pos.y + bounds->min.y;
        hdif2 = hdif1 + item->fall_speed;
        if ((hdif1 >> (WALL_SHIFT - 2)) == (hdif2 >> (WALL_SHIFT - 2))) {
            return EDGE_CATCH_NONE;
        }
        if (item->fall_speed > 0) {
            *edge = hdif2 & ~(STEP_L - 1);
        } else {
            *edge = hdif1 & ~(STEP_L - 1);
        }
        return EDGE_CATCH_NEG;
    }

    return ABS(coll->side_left2.floor - coll->side_right2.floor) < SLOPE_DIF
        ? EDGE_CATCH_POS
        : EDGE_CATCH_NONE;
}

SWING_CATCH Lara_Col_TestHangSwingIn(
    const ITEM *const item, const int16_t angle)
{
    // Tests whether a forward hang grab should transition into thin-ledge
    // swing ("swinging inwards"). The probe samples one click ahead in the
    // hang direction and requires:
    // - valid floor at probe point;
    // - probe floor above Lara;
    // - enough overhead clearance for swing-in.
    // The extra clearance guard follows TR3-5 logic: `y - ceiling - 819 > -72`
    // but uses a relaxed threshold to preserve the animation on certain slope
    // edge cases.

    XYZ_32 pos = item->pos;
    int16_t room_num = item->room_num;
    switch (angle) {
    case 0:
        pos.z += STEP_L;
        break;
    case DEG_90:
        pos.x += STEP_L;
        break;
    case -DEG_180:
        pos.z -= STEP_L;
        break;
    case -DEG_90:
        pos.x -= STEP_L;
        break;
    }

    const SECTOR *const sector = Room_GetSector(pos, &room_num);
    int32_t height = Room_GetHeight(sector, pos);
    int32_t ceiling = Room_GetCeiling(sector, pos);
    const bool has_height = height != NO_HEIGHT;
    const int32_t height_delta = height - pos.y;
    const int32_t ceiling_delta = ceiling - pos.y;
    if (!has_height || height_delta <= 0 || ceiling_delta >= -400) {
        return SWING_CATCH_NONE;
    }
    const bool thin_ledge = pos.y - ceiling - 819 > -110;
    return thin_ledge && g_Config.gameplay.enable_slow_ledge_swing
        ? SWING_CATCH_SLOW
        : SWING_CATCH_FAST;
}

static bool M_TestHangJump(ITEM *const item, COLL_INFO *const coll)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    if (!g_Input.action || lara->gun_status != LGS_ARMLESS
        || coll->hit_static) {
        return false;
    }

    if (coll->coll_type == COLL_TOP || coll->coll_type == COLL_TOP_FRONT) {
        int16_t room_num = item->room_num;
        const SECTOR *const sector = Room_GetSector(
            (XYZ_32) { item->pos.x, MAX_HEIGHT, item->pos.z }, &room_num);
        if ((sector->ladder & LADDER_CEILING) != 0) {
            Item_SwitchToAnim(item, LA(LA_SWING_IN_SLOW), 0);
            item->current_anim_state = LS(LS_MONKEY_IDLE);
            item->goal_anim_state = LS(LS_MONKEY_IDLE);
            item->gravity = false;
            item->speed = 0;
            item->fall_speed = 0;
            lara->gun_status = LGS_HANDS_BUSY;
            Lara_Col_MonkeySwingSnap(item);
            return true;
        }
    }

    if (coll->coll_type != COLL_FRONT || coll->side_mid.ceiling > -STEPUP_HEIGHT
        || coll->side_mid.floor < 200) {
        return false;
    }

    int32_t edge;
    const EDGE_CATCH edge_catch = Lara_Col_TestEdgeCatch(item, coll, &edge);
    bool ladder_hang = false;
    if (edge_catch == EDGE_CATCH_NEG) {
        ladder_hang = Lara_Col_TestLadderHang(item, coll);
    }
    if (edge_catch == EDGE_CATCH_NONE
        || (edge_catch == EDGE_CATCH_NEG && !ladder_hang)) {
        return false;
    }

    const DIRECTION dir = Math_GetDirectionCone(item->rot.y, LARA_HANG_ANGLE);
    if (dir == DIR_UNKNOWN) {
        return false;
    }
    const int16_t angle = Math_DirectionToAngle(dir);

    const SWING_CATCH swing_catch = Lara_Col_TestHangSwingIn(item, angle);
    if (swing_catch == SWING_CATCH_SLOW) {
        Item_SwitchToAnim(item, LA(LA_SWING_IN_SLOW), 0);
    } else if (swing_catch == SWING_CATCH_FAST) {
        Item_SwitchToAnim(item, LA(LA_SWING_IN_FAST), 0);
    } else {
        Item_SwitchToAnim(item, LA(LA_REACH_TO_HANG), 0);
    }
    const ANIM *const anim = Item_GetAnim(item);
    item->current_anim_state = anim->current_anim_state;
    item->goal_anim_state = anim->current_anim_state;

    const BOUNDS_16 *const bounds = Item_GetBoundsAccurate(item);
    if (edge_catch == EDGE_CATCH_POS) {
        item->pos.y += coll->side_front.floor - bounds->min.y;
        switch (dir) {
        case DIR_NORTH:
            item->pos.z = ROUND_TO_SECTOR_END(item->pos.z) - LARA_RADIUS;
            item->pos.x += coll->shift.x;
            break;

        case DIR_EAST:
            item->pos.x = ROUND_TO_SECTOR_END(item->pos.x) - LARA_RADIUS;
            item->pos.z += coll->shift.z;
            break;

        case DIR_SOUTH:
            item->pos.z = ROUND_TO_SECTOR(item->pos.z) + LARA_RADIUS;
            item->pos.x += coll->shift.x;
            break;

        case DIR_WEST:
            item->pos.x = ROUND_TO_SECTOR(item->pos.x) + LARA_RADIUS;
            item->pos.z += coll->shift.z;
            break;

        default:
            item->pos.x += coll->shift.x;
            item->pos.z += coll->shift.z;
            break;
        }
    } else {
        item->pos.y = edge - bounds->min.y;
    }

    item->rot.y = angle;
    item->speed = 2;
    item->gravity = true;
    item->fall_speed = 1;
    lara->gun_status = LGS_HANDS_BUSY;
    return true;
}

static bool M_TestHangJumpUp(ITEM *const item, COLL_INFO *const coll)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    if (!g_Input.action || lara->gun_status != LGS_ARMLESS
        || coll->hit_static) {
        return false;
    }

    if (coll->coll_type == COLL_TOP || coll->coll_type == COLL_TOP_FRONT) {
        int16_t room_num = item->room_num;
        const SECTOR *const sector = Room_GetSector(
            (XYZ_32) { item->pos.x, MAX_HEIGHT, item->pos.z }, &room_num);
        if ((sector->ladder & LADDER_CEILING) != 0) {
            Item_SwitchToAnim(item, LA(LA_MONKEY_GRAB), 0);
            item->current_anim_state = LS(LS_MONKEY_IDLE);
            item->goal_anim_state = LS(LS_MONKEY_IDLE);
            item->gravity = false;
            item->speed = 0;
            item->fall_speed = 0;
            lara->gun_status = LGS_HANDS_BUSY;

            Lara_Col_MonkeySwingSnap(item);
            return true;
        }
    }

    if (coll->coll_type != COLL_FRONT
        || coll->side_mid.ceiling
            > (lara->climb_status ? M_LADDER_CLEARANCE : M_HEAD_CLEARANCE)) {
        return false;
    }

    int32_t edge;
    const EDGE_CATCH edge_catch = Lara_Col_TestEdgeCatch(item, coll, &edge);
    if (edge_catch == EDGE_CATCH_NONE
        || (edge_catch == EDGE_CATCH_NEG
            && !Lara_Col_TestLadderHang(item, coll))) {
        return false;
    }

    const DIRECTION dir = Math_GetDirectionCone(item->rot.y, LARA_HANG_ANGLE);
    if (dir == DIR_UNKNOWN) {
        return false;
    }
    const int16_t angle = Math_DirectionToAngle(dir);

    item->goal_anim_state = LS(LS_HANG);
    item->current_anim_state = LS(LS_HANG);
    Item_SwitchToAnim(item, LA(LA_REACH_TO_HANG), M_LF_START_HANG);

    const BOUNDS_16 *const bounds = Item_GetBoundsAccurate(item);
    if (edge_catch == EDGE_CATCH_POS) {
        item->pos.y += coll->side_front.floor - bounds->min.y;
    } else {
        item->pos.y = edge - bounds->min.y + (g_TRVersion >= 3 ? 4 : 0);
    }
    item->pos.x += coll->shift.x;
    item->pos.z += coll->shift.z;
    item->rot.y = angle;
    item->speed = 0;
    item->gravity = false;
    item->fall_speed = 0;
    lara->gun_status = LGS_HANDS_BUSY;
    return true;
}

static void M_SlideEdgeJump(ITEM *const item, COLL_INFO *const coll)
{
    Lara_Col_Shift(coll);

    switch (coll->coll_type) {
    case COLL_LEFT:
        item->rot.y += LARA_DEFLECT_ANGLE;
        break;

    case COLL_RIGHT:
        item->rot.y -= LARA_DEFLECT_ANGLE;
        break;

    case COLL_TOP:
    case COLL_TOP_FRONT:
        CLAMPL(item->fall_speed, 1);
        break;

    case COLL_CLAMP:
        item->pos.z -= (Math_Cos(coll->facing) * 100) >> W2V_SHIFT;
        item->pos.x -= (Math_Sin(coll->facing) * 100) >> W2V_SHIFT;
        item->speed = 0;
        coll->side_mid.floor = 0;
        if (item->fall_speed <= 0) {
            item->fall_speed = 16;
        }
        break;
    }
}

static void M_Compress(ITEM *const item, COLL_INFO *const coll)
{
    item->gravity = false;
    item->fall_speed = 0;
    coll->bad_pos = NO_BAD_POS;
    coll->bad_neg = NO_BAD_NEG;
    coll->bad_ceiling = 0;

    Lara_Col_GetInfo(item, coll);

    if (coll->side_mid.ceiling > -100) {
        Item_SwitchToAnim(item, LA(LA_STAND_STILL), 0);
        item->goal_anim_state = LS(LS_STOP);
        item->current_anim_state = LS(LS_STOP);
        item->gravity = false;
        item->speed = 0;
        item->fall_speed = 0;
        item->pos = coll->old;
    }

    if (g_TRVersion >= 2 && coll->side_mid.floor > -STEP_L
        && coll->side_mid.floor < STEP_L) {
        item->pos.y += coll->side_mid.floor;
    }
}

static void M_NeutralJumpRoll(ITEM *const item, COLL_INFO *const coll)
{
    item->gravity = false;
    item->fall_speed = 0;
    coll->bad_pos = NO_BAD_POS;
    coll->bad_neg = NO_BAD_NEG;
    coll->bad_ceiling = 0;

    Lara_Col_GetInfo(item, coll);

    if (coll->side_mid.ceiling > -100) {
        Item_SwitchToAnim(item, LA(LA_STAND_STILL), 0);
        item->goal_anim_state = LS(LS_STOP);
        item->current_anim_state = LS(LS_STOP);
        item->speed = 0;
        item->pos = coll->old;
    }
}

static void M_UpJump(ITEM *const item, COLL_INFO *const coll)
{
    if (item->hit_points <= 0) {
        item->goal_anim_state = LS(LS_STOP);
        return;
    }

    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->move_angle = item->rot.y;
    coll->bad_pos = NO_BAD_POS;
    coll->bad_neg = -STEPUP_HEIGHT;
    coll->bad_ceiling = M_BAD_JUMP_CEILING;
    coll->facing = lara->move_angle;
    if (g_Config.gameplay.enable_lean_jumping && item->speed < 0) {
        coll->facing += DEG_180;
    }

    Collide_GetCollisionInfo(
        coll, item->pos.x, item->pos.y, item->pos.z, item->room_num, 870);
    if (M_TestHangJumpUp(item, coll)) {
        return;
    }

    M_SlideEdgeJump(item, coll);
    if (g_Config.gameplay.enable_lean_jumping) {
        if (coll->coll_type != COLL_NONE) {
            item->speed = item->speed > 0 ? 2 : -2;
        } else if (item->fall_speed < -70) {
            if (g_Input.forward && item->speed < 5) {
                item->speed++;
            } else if (g_Input.back && item->speed > -5) {
                item->speed -= 2;
            }
        }
    }

    if (item->fall_speed <= 0 || coll->side_mid.floor > 0) {
        return;
    }

    switch (Lara_Col_LandedBad(item)) {
    case LANDED_OK:
        item->goal_anim_state = LS(LS_STOP);
        break;
    case LANDED_BAD:
        item->goal_anim_state = LS(LS_DEATH);
        break;
    case LANDED_HANDLED:
        break;
    }
    item->gravity = false;
    item->fall_speed = 0;
    item->pos.y += coll->side_mid.floor;
}

static void M_ForwardJump(ITEM *const item, COLL_INFO *const coll)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    if (item->speed < 0
        && g_Config.gameplay.wall_glitch_mode != WALL_GLITCH_TR1) {
        lara->move_angle = item->rot.y + DEG_180;
    } else {
        lara->move_angle = item->rot.y;
    }
    coll->bad_pos = NO_BAD_POS;
    coll->bad_neg = -STEPUP_HEIGHT;
    coll->bad_ceiling = M_BAD_JUMP_CEILING;

    Lara_Col_GetInfo(item, coll);
    Lara_Col_DeflectEdgeJump(item, coll);
    if (item->speed < 0
        && g_Config.gameplay.wall_glitch_mode != WALL_GLITCH_TR1) {
        lara->move_angle = item->rot.y;
    }

    if (coll->side_mid.floor > 0 || item->fall_speed <= 0) {
        return;
    }

    switch (Lara_Col_LandedBad(item)) {
    case LANDED_OK:
        if (lara->water_status != LWS_WADE && g_Input.forward
            && !g_Input.slow) {
            item->goal_anim_state = LS(LS_RUN);
        } else {
            item->goal_anim_state = LS(LS_STOP);
        }
        break;
    case LANDED_BAD:
        item->goal_anim_state = LS(LS_DEATH);
        break;
    case LANDED_HANDLED:
        break;
    }

    item->gravity = false;
    item->fall_speed = 0;
    item->pos.y += coll->side_mid.floor;
    item->speed = 0;
    if (g_Config.gameplay.wall_glitch_mode != WALL_GLITCH_FIXED
        || coll->side_front.type != COLL_FRONT) {
        Lara_Animate(item);
    }
}

static void M_SideBackJump(ITEM *const item, COLL_INFO *const coll)
{
    int32_t angle = 0;
    switch (LS_U(item->current_anim_state)) {
    case LS_JUMP_BACK:
        angle = DEG_180;
        break;
    case LS_JUMP_RIGHT:
        angle = DEG_90;
        break;
    case LS_JUMP_LEFT:
        angle = -DEG_90;
        break;
    default:
        return;
    }

    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->move_angle = item->rot.y + angle;
    coll->bad_pos = NO_BAD_POS;
    coll->bad_neg = -STEPUP_HEIGHT;
    coll->bad_ceiling = M_BAD_JUMP_CEILING;

    Lara_Col_GetInfo(item, coll);
    Lara_Col_DeflectEdgeJump(item, coll);
    if (item->fall_speed <= 0 || coll->side_mid.floor > 0) {
        return;
    }

    switch (Lara_Col_LandedBad(item)) {
    case LANDED_OK:
        item->goal_anim_state = LS(LS_STOP);
        break;
    case LANDED_BAD:
        item->goal_anim_state = LS(LS_DEATH);
        break;
    case LANDED_HANDLED:
        break;
    }
    item->gravity = false;
    item->fall_speed = 0;
    item->pos.y += coll->side_mid.floor;
}

static void M_FallBack(ITEM *const item, COLL_INFO *const coll)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->move_angle = item->rot.y + DEG_180;
    coll->bad_pos = NO_BAD_POS;
    coll->bad_neg = -STEPUP_HEIGHT;
    coll->bad_ceiling = M_BAD_JUMP_CEILING;

    Lara_Col_GetInfo(item, coll);
    Lara_Col_DeflectEdgeJump(item, coll);

    if (coll->side_mid.floor > 0 || item->fall_speed <= 0) {
        return;
    }

    switch (Lara_Col_LandedBad(item)) {
    case LANDED_OK:
        item->goal_anim_state = LS(LS_STOP);
        break;
    case LANDED_BAD:
        item->goal_anim_state = LS(LS_DEATH);
        break;
    case LANDED_HANDLED:
        break;
    }

    item->gravity = false;
    item->fall_speed = 0;
    item->pos.y += coll->side_mid.floor;
}

static void M_Reach(ITEM *const item, COLL_INFO *const coll)
{
    item->gravity = true;
    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->move_angle = item->rot.y;
    coll->bad_pos = NO_BAD_POS;
    coll->bad_neg = 0;
    coll->bad_ceiling = M_BAD_JUMP_CEILING;

    Lara_Col_GetInfo(item, coll);
    if (M_TestHangJump(item, coll)) {
        return;
    }

    M_SlideEdgeJump(item, coll);
    if (item->fall_speed <= 0 || coll->side_mid.floor > 0) {
        return;
    }

    switch (Lara_Col_LandedBad(item)) {
    case LANDED_OK:
        item->goal_anim_state = LS(LS_STOP);
        break;
    case LANDED_BAD:
        item->goal_anim_state = LS(LS_DEATH);
        break;
    case LANDED_HANDLED:
        break;
    }

    item->gravity = false;
    item->fall_speed = 0;
    item->pos.y += coll->side_mid.floor;
}

static void M_SwanDive(ITEM *const item, COLL_INFO *const coll)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->move_angle = item->rot.y;
    coll->bad_pos = NO_BAD_POS;
    coll->bad_neg = -STEPUP_HEIGHT;
    coll->bad_ceiling = M_BAD_JUMP_CEILING;

    Lara_Col_GetInfo(item, coll);
    Lara_Col_DeflectEdgeJump(item, coll);
    if (coll->side_mid.floor > 0 || item->fall_speed <= 0) {
        return;
    }

    item->goal_anim_state = LS(LS_STOP);
    item->gravity = false;
    item->fall_speed = 0;
    item->pos.y += coll->side_mid.floor;
}

static void M_FastDive(ITEM *const item, COLL_INFO *const coll)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->move_angle = item->rot.y;
    coll->bad_pos = NO_BAD_POS;
    coll->bad_neg = -STEPUP_HEIGHT;
    coll->bad_ceiling = M_BAD_JUMP_CEILING;

    Lara_Col_GetInfo(item, coll);
    Lara_Col_DeflectEdgeJump(item, coll);

    if (coll->side_mid.floor > 0 || item->fall_speed <= 0) {
        return;
    }

    if (item->fall_speed > 133 && !g_Config.debug.enable_invulnerability) {
        item->goal_anim_state = LS(LS_DEATH);
    } else {
        item->goal_anim_state = LS(LS_STOP);
    }
    item->gravity = false;
    item->fall_speed = 0;
    item->pos.y += coll->side_mid.floor;
}

static void M_FastFall(ITEM *const item, COLL_INFO *const coll)
{
    item->gravity = true;
    coll->bad_pos = NO_BAD_POS;
    coll->bad_neg = -STEPUP_HEIGHT;
    coll->bad_ceiling = M_BAD_JUMP_CEILING;

    Lara_Col_GetInfo(item, coll);
    M_SlideEdgeJump(item, coll);
    if (coll->side_mid.floor > 0) {
        return;
    }

    switch (Lara_Col_LandedBad(item)) {
    case LANDED_OK:
        item->goal_anim_state = LS(LS_STOP);
        item->current_anim_state = LS(LS_STOP);
        Item_SwitchToAnim(item, LA(LA_FREEFALL_LAND), 0);
        break;
    case LANDED_BAD:
        item->goal_anim_state = LS(LS_DEATH);
        break;
    case LANDED_HANDLED:
        break;
    }

    Sound_StopEffect(SFX_LARA_FALL);
    item->gravity = false;
    item->fall_speed = 0;
    item->pos.y += coll->side_mid.floor;
}

void Lara_Col_DeflectEdgeJump(ITEM *const item, COLL_INFO *const coll)
{
    Lara_Col_Shift(coll);
    switch (coll->coll_type) {
    case COLL_FRONT:
    case COLL_TOP_FRONT:
        LARA_INFO *const lara = Lara_GetLaraInfo();
        if (lara->climb_status && item->speed == 2) {
            break;
        }

        if (g_Config.gameplay.wall_glitch_mode == WALL_GLITCH_TR1
            || coll->side_mid.floor > (STEP_L * 2)) {
            item->goal_anim_state = LS(LS_FAST_FALL);
            item->current_anim_state = LS(LS_FAST_FALL);
            Item_SwitchToAnim(item, LA(LA_SMASH_JUMP), M_LF_FAST_FALL);
        } else if (coll->side_mid.floor <= (STEP_L / 2)) {
            item->goal_anim_state = LS(LS_LAND);
            item->current_anim_state = LS(LS_LAND);
            Item_SwitchToAnim(item, LA(LA_JUMP_UP_LAND), 0);
        }
        item->speed /= 4;
        lara->move_angle += DEG_180;
        CLAMPL(item->fall_speed, 1);
        break;

    case COLL_LEFT:
        item->rot.y += LARA_DEFLECT_ANGLE;
        break;

    case COLL_RIGHT:
        item->rot.y -= LARA_DEFLECT_ANGLE;
        break;

    case COLL_TOP:
        CLAMPL(item->fall_speed, 1);
        break;

    case COLL_CLAMP:
        item->pos.z -= (Math_Cos(coll->facing) * 100) >> W2V_SHIFT;
        item->pos.x -= (Math_Sin(coll->facing) * 100) >> W2V_SHIFT;
        item->speed = 0;
        coll->side_mid.floor = 0;
        if (item->fall_speed <= 0) {
            item->fall_speed = 16;
        }
        break;
    }
}

LANDED_STATE Lara_Col_LandedBad(ITEM *const item)
{
    const XYZ_32 pos = item->pos;
    int16_t room_num = item->room_num;
    const SECTOR *const sector = Room_GetSector(pos, &room_num);
    const int32_t height =
        Room_GetHeight(sector, (XYZ_32) { pos.x, pos.y - LARA_HEIGHT, pos.z });
    item->pos.y = height;
    item->floor = height;

    LARA_INFO *const lara = Lara_GetLaraInfo();
    const bool was_alive = item->hit_points > 0;
    const bool was_extra_anim = lara->extra_anim;
    Room_TestTriggers(item);
    if (was_alive && item->hit_points <= 0 && !was_extra_anim
        && lara->extra_anim) {
        // Support rapids drown from any height
        return LANDED_HANDLED;
    }

    item->pos.y = pos.y;
    const int32_t land_speed = item->fall_speed - DAMAGE_START;
    if (land_speed <= 0) {
        return LANDED_OK;
    }

    if (g_Config.debug.enable_invulnerability) {
        return false;
    } else if (land_speed <= DAMAGE_LENGTH) {
        Lara_TakeDamage(
            LARA_MAX_HITPOINTS * SQUARE(land_speed) / SQUARE(DAMAGE_LENGTH),
            false);
    } else {
        item->hit_points = -1;
    }

    // #675: Original bug to keep. Correct operator would be <=
    return item->hit_points < 0 ? LANDED_BAD : LANDED_OK;
}

// clang-format off
REGISTER_LARA_COL(LS_COMPRESS,     M_Compress)
REGISTER_LARA_COL(LS_NEUTRAL_ROLL, M_NeutralJumpRoll)
REGISTER_LARA_COL(LS_JUMP_UP,      M_UpJump)
REGISTER_LARA_COL(LS_JUMP_FORWARD, M_ForwardJump)
REGISTER_LARA_COL(LS_JUMP_BACK,    M_SideBackJump)
REGISTER_LARA_COL(LS_JUMP_RIGHT,   M_SideBackJump)
REGISTER_LARA_COL(LS_JUMP_LEFT,    M_SideBackJump)
REGISTER_LARA_COL(LS_FALL_BACK,    M_FallBack)
REGISTER_LARA_COL(LS_REACH,        M_Reach)
REGISTER_LARA_COL(LS_SWAN_DIVE,    M_SwanDive)
REGISTER_LARA_COL(LS_FAST_DIVE,    M_FastDive)
REGISTER_LARA_COL(LS_FAST_FALL,    M_FastFall)
// clang-format on
