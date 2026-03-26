#include <trx/config.h>
#include <trx/game/input.h>
#include <trx/game/lara.h>
#include <trx/game/lara/util.h>
#include <trx/game/rooms.h>

#define M_HEIGHT_SURF 700

static bool M_TestWaterStepOut(ITEM *const item, const COLL_INFO *const coll)
{
    if (coll->coll_type == COLL_FRONT || coll->side_mid.type == HT_BIG_SLOPE
        || coll->side_mid.type == HT_DIAGONAL || coll->side_mid.floor >= 0) {
        return false;
    }

    if (coll->side_mid.floor < -STEP_L / 2) {
        item->current_anim_state = LS(LS_WATER_OUT);
        item->goal_anim_state = LS(LS_STOP);
        Item_SwitchToAnim(item, LA(LA_ONWATER_TO_WADE), 0);
    } else if (item->goal_anim_state == LS(LS_SURF_LEFT)) {
        item->goal_anim_state = LS(LS_STEP_LEFT);
    } else if (item->goal_anim_state == LS(LS_SURF_RIGHT)) {
        item->goal_anim_state = LS(LS_STEP_RIGHT);
    } else {
        item->current_anim_state = LS(LS_WADE);
        item->goal_anim_state = LS(LS_WADE);
        Item_SwitchToAnim(item, LA(LA_WADE), 0);
    }

    item->pos.y += coll->side_front.floor + M_HEIGHT_SURF - 5;
    Lara_UpdateRoomToHeight(-LARA_HEIGHT / 2);
    item->gravity = false;
    item->rot.x = 0;
    item->rot.z = 0;
    item->speed = 0;
    item->fall_speed = 0;
    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->water_status = LWS_WADE;
    return true;
}

static bool M_TestWaterClimbOut(ITEM *const item, const COLL_INFO *const coll)
{
    const int32_t coll_hdif =
        ABS(coll->side_left2.floor - coll->side_right2.floor);
    if (coll->coll_type != COLL_FRONT || !g_Input.action
        || coll_hdif >= SLOPE_DIF) {
        return false;
    }

    if (coll->side_front.ceiling > 0
        || coll->side_mid.ceiling > -STEPUP_HEIGHT) {
        return false;
    }

    LARA_INFO *const lara = Lara_GetLaraInfo();
    if (g_Config.gameplay.fix_water_exit) {
        if (coll->side_front.type == HT_BIG_SLOPE) {
            return false;
        }
    } else if (item->rot.y != lara->move_angle) {
        return false;
    }

    if (lara->gun_status != LGS_ARMLESS
        && (lara->gun_status != LGS_READY || lara->gun_type != LGT_FLARE)) {
        return false;
    }

    const int32_t lara_hdif = coll->side_front.floor + M_HEIGHT_SURF;
    if (lara_hdif <= -STEP_L * 2 || lara_hdif > M_HEIGHT_SURF - STEPUP_HEIGHT) {
        return false;
    }

    const DIRECTION dir = Math_GetDirectionCone(item->rot.y, LARA_HANG_ANGLE);
    if (dir == DIR_UNKNOWN) {
        return false;
    }

    item->pos.y += lara_hdif - 5;
    Lara_UpdateRoomToHeight(-LARA_HEIGHT / 2);

    switch (dir) {
    case DIR_NORTH:
        item->pos.z = ROUND_TO_SECTOR(item->pos.z) + WALL_L + LARA_RADIUS;
        break;
    case DIR_EAST:
        item->pos.x = ROUND_TO_SECTOR(item->pos.x) + WALL_L + LARA_RADIUS;
        break;
    case DIR_SOUTH:
        item->pos.z = ROUND_TO_SECTOR(item->pos.z) - LARA_RADIUS;
        break;
    case DIR_WEST:
        item->pos.x = ROUND_TO_SECTOR(item->pos.x) - LARA_RADIUS;
        break;
    case DIR_UNKNOWN:
        return false;
    }

    if (lara_hdif < -STEP_L / 2) {
        Item_SwitchToAnim(item, LA(LA_ONWATER_TO_STAND_HIGH), 0);
    } else if (lara_hdif < STEP_L / 2) {
        Item_SwitchToAnim(item, LA(LA_ONWATER_TO_STAND_MEDIUM), 0);
    } else {
        Item_SwitchToAnim(item, LA(LA_ONWATER_TO_WADE_LOW), 0);
    }

    item->current_anim_state = LS(LS_WATER_OUT);
    item->goal_anim_state = LS(LS_STOP);
    item->rot.y = Math_DirectionToAngle(dir);
    item->rot.x = 0;
    item->rot.z = 0;
    item->gravity = false;
    item->speed = 0;
    item->fall_speed = 0;
    lara->gun_status = LGS_HANDS_BUSY;
    lara->water_status = LWS_ABOVE_WATER;
    return true;
}

static void M_TestWaterDepth(ITEM *const item, const COLL_INFO *const coll)
{
    int16_t room_num = item->room_num;
    const SECTOR *const sector = Room_GetSector(item->pos, &room_num);
    const int32_t water_depth =
        Lara_GetWaterDepth(item->pos.x, item->pos.y, item->pos.z, room_num);

    if (g_Config.gameplay.fix_water_exit && water_depth == NO_HEIGHT) {
        item->pos = coll->old;
        item->fall_speed = 0;
        return;
    }

    if (water_depth == NO_HEIGHT || water_depth > STEP_L * 2) {
        return;
    }

    Item_SwitchToAnim(item, LA(LA_UNDERWATER_TO_STAND), 0);
    item->current_anim_state = LS(LS_WATER_OUT);
    item->goal_anim_state = LS(LS_STOP);
    item->rot.x = 0;
    item->rot.z = 0;
    item->gravity = false;
    item->speed = 0;
    item->fall_speed = 0;
    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->water_status = LWS_WADE;
    item->pos.y = Room_GetHeight(sector, item->pos);
}

static void M_CommonSurface(ITEM *const item, COLL_INFO *const coll)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    coll->facing = lara->move_angle;

    int32_t obj_height = M_HEIGHT_SURF;
    if (g_Config.gameplay.enable_wading) {
        obj_height += 100;
    }
    Collide_GetCollisionInfo(
        coll, item->pos.x, item->pos.y + M_HEIGHT_SURF, item->pos.z,
        item->room_num, obj_height);

    Lara_Col_Shift(coll);

    if (coll->coll_type == COLL_LEFT) {
        item->rot.y += 5 * DEG_1;
    } else if (coll->coll_type == COLL_RIGHT) {
        item->rot.y -= 5 * DEG_1;
    } else if (
        coll->coll_type != COLL_NONE
        || (coll->side_mid.floor < 0
            && (coll->side_mid.type == HT_BIG_SLOPE
                || coll->side_mid.type == HT_DIAGONAL))) {
        item->fall_speed = 0;
        item->pos = coll->old;
    }

    const int32_t water_height = Room_GetWaterHeight(item->pos, item->room_num);
    if (water_height - item->pos.y <= -100) {
        item->current_anim_state = LS(LS_DIVE);
        item->goal_anim_state = LS(LS_SWIM);
        Item_SwitchToAnim(item, LA(LA_ONWATER_DIVE), 0);
        item->rot.x = -45 * DEG_1;
        item->fall_speed = 80;
        lara->water_status = LWS_UNDERWATER;
        return;
    }

    if (g_Config.gameplay.enable_wading) {
        M_TestWaterStepOut(item, coll);
    } else {
        M_TestWaterClimbOut(item, coll);
    }
}

static void M_ForwardSurface(ITEM *const item, COLL_INFO *const coll)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->move_angle = item->rot.y;
    coll->bad_neg = -STEPUP_HEIGHT;
    M_CommonSurface(item, coll);
    if (g_Config.gameplay.enable_wading) {
        M_TestWaterClimbOut(item, coll);
    }
}

static void M_SideBackSurface(ITEM *const item, COLL_INFO *const coll)
{
    int32_t angle = 0;
    switch (LS_U(item->current_anim_state)) {
    case LS_SURF_BACK:
        angle = -DEG_180;
        break;
    case LS_SURF_LEFT:
        angle = -DEG_90;
        break;
    case LS_SURF_RIGHT:
        angle = DEG_90;
        break;
    default:
        break;
    }

    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->move_angle = item->rot.y + angle;
    M_CommonSurface(item, coll);
}

static void M_Swim(ITEM *const item, COLL_INFO *const coll)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    if (item->rot.x < -DEG_90 || item->rot.x > DEG_90) {
        lara->move_angle = item->rot.y + DEG_180;
    } else {
        lara->move_angle = item->rot.y;
    }

    coll->facing = lara->move_angle;

    int32_t height;
    if (g_Config.gameplay.enable_wading) {
        height = (LARA_HEIGHT * Math_Sin(item->rot.x)) >> W2V_SHIFT;
        if (height < 0) {
            height = -height;
        }
        CLAMPL(height, 200);
        coll->bad_neg = -height;
    } else {
        height = LARA_HEIGHT_UW;
    }

    Collide_GetCollisionInfo(
        coll, item->pos.x, item->pos.y + height / 2, item->pos.z,
        item->room_num, height);
    Lara_Col_Shift(coll);

    switch (coll->coll_type) {
    case COLL_FRONT:
        if (item->rot.x > 35 * DEG_1) {
            item->rot.x += LARA_UW_WALL_DEFLECT;
        } else if (item->rot.x < -35 * DEG_1) {
            item->rot.x -= LARA_UW_WALL_DEFLECT;
        } else {
            item->fall_speed = 0;
        }
        break;

    case COLL_TOP:
        if (item->rot.x >= -45 * DEG_1) {
            item->rot.x -= LARA_UW_WALL_DEFLECT;
        }
        break;

    case COLL_TOP_FRONT:
        item->fall_speed = 0;
        break;

    case COLL_LEFT:
        item->rot.y += 5 * DEG_1;
        break;

    case COLL_RIGHT:
        item->rot.y -= 5 * DEG_1;
        break;

    case COLL_CLAMP:
        item->pos = coll->old;
        item->fall_speed = 0;
        return;
    }

    if (coll->side_mid.floor < 0) {
        item->rot.x += LARA_UW_WALL_DEFLECT;
        item->pos.y = coll->side_mid.floor + item->pos.y;
    }

    if (g_Config.gameplay.enable_wading && lara->water_status != LWS_CHEAT
        && !lara->extra_anim) {
        M_TestWaterDepth(item, coll);
    }
}

static void M_UWDeath(ITEM *const item, COLL_INFO *const coll)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->air = -1;
    lara->gun_status = LGS_HANDS_BUSY;
    item->hit_points = -1;
    const int32_t water_height = Room_GetWaterHeight(item->pos, item->room_num);
    if (water_height != NO_HEIGHT && water_height < item->pos.y - 100) {
        item->pos.y -= 5;
    }
    M_Swim(item, coll);
}

// clang-format off
REGISTER_LARA_COL(LS_SURF_SWIM,  M_ForwardSurface)
REGISTER_LARA_COL(LS_SURF_TREAD, M_SideBackSurface)
REGISTER_LARA_COL(LS_SURF_BACK,  M_SideBackSurface)
REGISTER_LARA_COL(LS_SURF_LEFT,  M_SideBackSurface)
REGISTER_LARA_COL(LS_SURF_RIGHT, M_SideBackSurface)
REGISTER_LARA_COL(LS_SWIM,       M_Swim)
REGISTER_LARA_COL(LS_TREAD,      M_Swim)
REGISTER_LARA_COL(LS_GLIDE,      M_Swim)
REGISTER_LARA_COL(LS_DIVE,       M_Swim)
REGISTER_LARA_COL(LS_WATER_ROLL, M_Swim)
REGISTER_LARA_COL(LS_UW_DEATH,   M_UWDeath)
// clang-format on
