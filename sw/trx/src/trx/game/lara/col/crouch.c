#include <trx/config.h>
#include <trx/game/input.h>
#include <trx/game/items/anim.h>
#include <trx/game/lara.h>
#include <trx/game/lara/misc.h>
#include <trx/game/lara/util.h>
#include <trx/game/rooms.h>
#include <trx/game/rooms/geometry.h>
#include <trx/game/rooms/utils.h>

// clang-format off
#define M_CROUCH_RADIUS            200
#define M_CRAWL_BACK_RADIUS        250
#define M_CRAWL_BAD_POS            255
#define M_CRAWL_BAD_NEG           -255
#define M_CRAWL_BAD_CEILING        400
#define M_CROUCH_CEILING_THRESHOLD -362
#define M_CRAWL_TO_HANG_RADIUS     200
#define M_CRAWL_TO_HANG_HEIGHT     870
#define M_CRAWL_TO_HANG_XZ_OFFSET  100
#define M_CRAWL_TO_HANG_FALL_SPEED 512
#define M_CRAWL_TO_HANG_BAD_CEILING ((STEP_L * 3) / 4) // = 192
#define M_CRAWL_TO_HANG_FALL_FRAME 9
// clang-format on

static bool M_DeflectEdgeCrawl(ITEM *const item, COLL_INFO *const coll)
{
    switch (coll->coll_type) {
    case COLL_FRONT:
    case COLL_TOP_FRONT:
        Lara_Col_Shift(coll);
        item->speed = 0;
        item->gravity = false;
        return true;

    case COLL_LEFT:
        Lara_Col_Shift(coll);
        item->rot.y += LARA_TURN_UNDO;
        break;

    case COLL_RIGHT:
        Lara_Col_Shift(coll);
        item->rot.y -= LARA_TURN_UNDO;
        break;

    default:
        break;
    }

    return false;
}

static bool M_HasStaticBehind(const ITEM *const item, const int16_t angle)
{
    COLL_INFO test = {
        .radius = 50,
    };

    const int32_t x = item->pos.x + ((Math_Sin(angle) * 512) >> W2V_SHIFT);
    const int32_t z = item->pos.z + ((Math_Cos(angle) * 512) >> W2V_SHIFT);
    return Collide_CollideStaticObjects(
        &test, x, item->pos.y, z, item->room_num, 300);
}

static void M_Crouch(ITEM *const item, COLL_INFO *const coll)
{
    item->gravity = false;
    item->fall_speed = 0;

    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->is_crouched = true;
    lara->move_angle = item->rot.y;

    coll->facing = lara->move_angle;
    coll->radius = M_CROUCH_RADIUS;
    coll->bad_pos = STEPUP_HEIGHT;
    coll->bad_neg = -STEPUP_HEIGHT;
    coll->bad_ceiling = 0;
    coll->slopes_are_walls = 1;

    Collide_GetCollisionInfo(
        coll, item->pos.x, item->pos.y, item->pos.z, item->room_num,
        -LARA_HEIGHT_CROUCH);

    if (Lara_Col_Fallen(item, coll)) {
        lara->gun_status = LGS_ARMLESS;
        return;
    }

    lara->keep_crouched = coll->side_mid.ceiling >= M_CROUCH_CEILING_THRESHOLD;
    Lara_Col_Shift(coll);
    item->pos.y += coll->side_mid.floor;

    const bool crouch_active = g_Config.gameplay.enable_toggle_crouch
        ? !(lara->crouching && g_InputDB.crouch)
        : g_Input.crouch;

    if ((!crouch_active || lara->water_status == LWS_WADE)
        && !lara->keep_crouched
        && Item_TestAnimEqual(item, LA(LA_CROUCH_IDLE))) {
        lara->crouching = false;
        item->goal_anim_state = LS(LS_STOP);
    } else if (g_Config.gameplay.enable_responsive_crawl) {
        if (g_Input.left) {
            item->goal_anim_state = LS(LS_CROUCH_TURN_LEFT);
        } else if (g_Input.right) {
            item->goal_anim_state = LS(LS_CROUCH_TURN_RIGHT);
        }
    }
}

static void M_CrouchRoll(ITEM *const item, COLL_INFO *const coll)
{
    item->gravity = false;
    item->fall_speed = 0;

    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->move_angle = item->rot.y;

    coll->facing = lara->move_angle;
    coll->radius = M_CROUCH_RADIUS;
    coll->bad_pos = STEPUP_HEIGHT;
    coll->bad_neg = -STEPUP_HEIGHT;
    coll->bad_ceiling = 0;
    coll->slopes_are_walls = 1;

    Collide_GetCollisionInfo(
        coll, item->pos.x, item->pos.y, item->pos.z, item->room_num,
        -LARA_HEIGHT_CROUCH);

    if (Lara_Col_Fallen(item, coll)) {
        lara->gun_status = LGS_ARMLESS;
    } else if (!Lara_Col_TestSlide(item, coll)) {
        lara->keep_crouched =
            coll->side_mid.ceiling >= M_CROUCH_CEILING_THRESHOLD;

        if (coll->side_mid.floor < coll->bad_neg
            || coll->side_front.floor > coll->bad_pos) {
            item->pos = coll->old;
            return;
        }

        Lara_Col_Shift(coll);

        if (!Lara_Col_TestCeiling(item, coll)) {
            item->pos.y += coll->side_mid.floor;
        }
    }
}

static void M_CrouchTurn(ITEM *const item, COLL_INFO *const coll)
{
    item->gravity = false;
    item->fall_speed = 0;

    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->is_crouched = true;
    lara->move_angle = item->rot.y;

    coll->facing = lara->move_angle;
    coll->radius = M_CROUCH_RADIUS;
    coll->bad_pos = STEPUP_HEIGHT;
    coll->bad_neg = -STEPUP_HEIGHT;
    coll->bad_ceiling = 0;
    coll->slopes_are_walls = 1;

    Collide_GetCollisionInfo(
        coll, item->pos.x, item->pos.y, item->pos.z, item->room_num,
        LARA_HEIGHT_CROUCH);

    if (Lara_Col_Fallen(item, coll)) {
        lara->gun_status = LGS_ARMLESS;
        return;
    }

    if (Lara_Col_TestSlide(item, coll)) {
        return;
    }

    lara->keep_crouched = coll->side_mid.ceiling >= M_CROUCH_CEILING_THRESHOLD;

    Lara_Col_Shift(coll);
    item->pos.y += coll->side_mid.floor;
}

static void M_CrawlIdle(ITEM *const item, COLL_INFO *const coll)
{
    item->gravity = false;
    item->fall_speed = 0;

    if (item->goal_anim_state == LS(LS_CRAWL_TO_CLIMB)) {
        return;
    }

    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->is_crouched = true;
    lara->move_angle = item->rot.y;

    coll->facing = lara->move_angle;
    coll->radius = M_CROUCH_RADIUS;
    coll->bad_pos = M_CRAWL_BAD_POS;
    coll->bad_neg = M_CRAWL_BAD_NEG;
    coll->bad_ceiling = M_CRAWL_BAD_CEILING;
    coll->slopes_are_walls = 1;
    coll->slopes_are_pits = 1;

    Collide_GetCollisionInfo(
        coll, item->pos.x, item->pos.y, item->pos.z, item->room_num,
        LARA_HEIGHT_CROUCH);

    if (Lara_Col_Fallen(item, coll)) {
        lara->gun_status = LGS_ARMLESS;
        return;
    }

    lara->keep_crouched = coll->side_mid.ceiling >= M_CROUCH_CEILING_THRESHOLD;

    Lara_Col_Shift(coll);
    item->pos.y += coll->side_mid.floor;

    const bool crouch_active = g_Config.gameplay.enable_toggle_crouch
        ? lara->crouching
        : g_Input.crouch;
    if ((!crouch_active && !lara->keep_crouched) || g_Input.draw
        || (g_Config.gameplay.enable_toggle_crouch && !g_Input.forward
            && g_InputDB.crouch)) {
        item->goal_anim_state = LS(LS_CROUCH_IDLE);
        return;
    }

    bool allow_movement = Item_TestAnimEqual(item, LA(LA_CRAWL_IDLE))
        || Item_TestAnimEqual(item, LA(LA_CROUCH_TO_CRAWL_END));
    if (g_Config.gameplay.enable_responsive_crawl) {
        allow_movement |=
            Item_TestAnimEqual(item, LA(LA_CRAWL_FORWARD_TO_IDLE_END_LEFT))
            || Item_TestAnimEqual(item, LA(LA_CRAWL_FORWARD_TO_IDLE_END_RIGHT));
    }

    if (!allow_movement) {
        return;
    }

    if (g_Input.forward) {
        const int16_t h = Lara_FloorFront(item, item->rot.y, 256);
        if (h < 255 && h > -255 && Room_GetHeightType() != HT_BIG_SLOPE) {
            item->goal_anim_state = LS(LS_CRAWL_FORWARD);
        }
    } else if (g_Input.back) {
        int32_t h = Lara_CeilingFront(item, item->rot.y, -300, LARA_HEIGHT);
        if (h == NO_HEIGHT || h > 256) {
            return;
        }

        h = Lara_FloorFront(item, item->rot.y, -300);
        if (h < 255 && h > -255 && Room_GetHeightType() != HT_BIG_SLOPE) {
            item->goal_anim_state = LS(LS_CRAWL_BACK);
            return;
        }

        if (g_Input.action && h > 768
            && !M_HasStaticBehind(item, item->rot.y + DEG_180)) {
            const XYZ_32 old_pos = item->pos;
            const XYZ_16 old_rot = item->rot;

            const DIRECTION dir = Math_GetDirection(item->rot.y);
            switch (dir) {
            case DIR_NORTH:
                item->rot.y = 0;
                item->pos.z = ROUND_TO_SECTOR(item->pos.z) + 225;
                break;

            case DIR_EAST:
                item->rot.y = DEG_90;
                item->pos.x = ROUND_TO_SECTOR(item->pos.x) + 225;
                break;

            case DIR_SOUTH:
                item->rot.y = -DEG_180;
                item->pos.z = ROUND_TO_SECTOR_END(item->pos.z) - 225;
                break;

            case DIR_WEST:
                item->rot.y = -DEG_90;
                item->pos.x = ROUND_TO_SECTOR_END(item->pos.x) - 225;
                break;
            default:
                break;
            }

            h = Lara_FloorFront(item, item->rot.y, 0);
            if (h > 255 || h < -255 || Room_GetHeightType() == HT_BIG_SLOPE) {
                item->pos = old_pos;
                item->rot = old_rot;
            } else {
                item->goal_anim_state = LS(LS_CRAWL_TO_CLIMB);
            }
        }
    } else if (g_Input.left) {
        Item_SwitchToAnim(item, LA(LA_CRAWL_TURN_LEFT), 0);
        item->goal_anim_state = LS(LS_CRAWL_TURN_LEFT);
        item->current_anim_state = LS(LS_CRAWL_TURN_LEFT);
    } else if (g_Input.right) {
        Item_SwitchToAnim(item, LA(LA_CRAWL_TURN_RIGHT), 0);
        item->goal_anim_state = LS(LS_CRAWL_TURN_RIGHT);
        item->current_anim_state = LS(LS_CRAWL_TURN_RIGHT);
    }
}

static void M_CrawlForward(ITEM *const item, COLL_INFO *const coll)
{
    item->gravity = false;
    item->fall_speed = 0;

    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->is_crouched = true;
    lara->move_angle = item->rot.y;

    coll->radius = M_CROUCH_RADIUS;
    coll->bad_pos = M_CRAWL_BAD_POS;
    coll->bad_neg = M_CRAWL_BAD_NEG;
    coll->bad_ceiling = M_CRAWL_BAD_CEILING;
    coll->slopes_are_walls = 1;
    coll->slopes_are_pits = 1;
    coll->facing = lara->move_angle;

    Collide_GetCollisionInfo(
        coll, item->pos.x, item->pos.y, item->pos.z, item->room_num,
        -LARA_HEIGHT_CROUCH);

    if (M_DeflectEdgeCrawl(item, coll)) {
        item->current_anim_state = LS(LS_CRAWL_IDLE);
        item->goal_anim_state = LS(LS_CRAWL_IDLE);
        if (!Item_TestAnimEqual(item, LA(LA_CRAWL_IDLE))) {
            Item_SwitchToAnim(item, LA(LA_CRAWL_IDLE), 0);
        }
    } else if (Lara_Col_Fallen(item, coll)) {
        lara->gun_status = LGS_ARMLESS;
    } else {
        Lara_Col_Shift(coll);
        item->pos.y += coll->side_mid.floor;
    }
}

static void M_CrawlTurn(ITEM *const item, COLL_INFO *const coll)
{
    Collide_GetCollisionInfo(
        coll, item->pos.x, item->pos.y, item->pos.z, item->room_num,
        LARA_HEIGHT_CROUCH);
}

static void M_CrawlBack(ITEM *const item, COLL_INFO *const coll)
{
    item->gravity = false;
    item->fall_speed = 0;

    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->is_crouched = true;
    lara->move_angle = item->rot.y + DEG_180;

    coll->radius = M_CRAWL_BACK_RADIUS;
    coll->bad_pos = M_CRAWL_BAD_POS;
    coll->bad_neg = M_CRAWL_BAD_NEG;
    coll->bad_ceiling = M_CRAWL_BAD_CEILING;
    coll->slopes_are_walls = 1;
    coll->slopes_are_pits = 1;
    coll->facing = lara->move_angle;

    Collide_GetCollisionInfo(
        coll, item->pos.x, item->pos.y, item->pos.z, item->room_num,
        -LARA_HEIGHT_CROUCH);

    if (M_DeflectEdgeCrawl(item, coll)) {
        item->current_anim_state = LS(LS_CRAWL_IDLE);
        item->goal_anim_state = LS(LS_CRAWL_IDLE);
        if (!Item_TestAnimEqual(item, LA(LA_CRAWL_IDLE))) {
            Item_SwitchToAnim(item, LA(LA_CRAWL_IDLE), 0);
        }
    } else if (Lara_Col_Fallen(item, coll)) {
        lara->gun_status = LGS_ARMLESS;
    } else {
        Lara_Col_Shift(coll);
        item->pos.y += coll->side_mid.floor;
        lara->move_angle = item->rot.y;
    }
}

static void M_CrawlToClimb(ITEM *const item, COLL_INFO *const coll)
{
    if (!Item_TestAnimEqual(item, LA(LA_CRAWL_TO_HANG_END))) {
        return;
    }

    item->fall_speed = M_CRAWL_TO_HANG_FALL_SPEED;
    item->pos.y |= 255;

    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->move_angle = item->rot.y;

    coll->radius = M_CRAWL_TO_HANG_RADIUS;
    coll->bad_pos = NO_BAD_POS;
    coll->bad_neg = -STEPUP_HEIGHT;
    coll->bad_ceiling = M_CRAWL_TO_HANG_BAD_CEILING;
    coll->facing = lara->move_angle;

    Collide_GetCollisionInfo(
        coll, item->pos.x, item->pos.y, item->pos.z, item->room_num,
        M_CRAWL_TO_HANG_HEIGHT);

    int32_t edge = 0;
    const EDGE_CATCH edge_catch = Lara_Col_TestEdgeCatch(item, coll, &edge);
    if (edge_catch == EDGE_CATCH_NONE
        || (edge_catch == EDGE_CATCH_NEG
            && !Lara_Col_TestLadderHang(item, coll))) {
        // LA_CRAWL_TO_HANG_END will loop indefinitely, so in cases where Lara
        // cannot grab the edge, make her fall and she will then either re-grab
        // it on a better position, or continue falling if the ledge is a slope.
        Item_SwitchToAnim(item, LA(LA_JUMP_UP), M_CRAWL_TO_HANG_FALL_FRAME);
        item->current_anim_state = LS(LS_JUMP_UP);
        item->goal_anim_state = LS(LS_JUMP_UP);
        item->gravity = true;
        item->speed = 2;
        item->fall_speed = 1;
        lara->gun_status = LGS_ARMLESS;
        return;
    }

    const DIRECTION dir = Math_GetDirectionCone(item->rot.y, LARA_HANG_ANGLE);
    if (dir == DIR_UNKNOWN) {
        return;
    }
    const int16_t angle = Math_DirectionToAngle(dir);

    const SWING_CATCH swing_catch = Lara_Col_TestHangSwingIn(item, angle);
    if (swing_catch == SWING_CATCH_SLOW) {
        lara->head_rot.x = 0;
        lara->head_rot.y = 0;
        lara->torso_rot.x = 0;
        lara->torso_rot.y = 0;
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
            item->pos.z =
                ROUND_TO_SECTOR_END(item->pos.z) - M_CRAWL_TO_HANG_XZ_OFFSET;
            item->pos.x += coll->shift.x;
            break;

        case DIR_EAST:
            item->pos.x =
                ROUND_TO_SECTOR_END(item->pos.x) - M_CRAWL_TO_HANG_XZ_OFFSET;
            item->pos.z += coll->shift.z;
            break;

        case DIR_SOUTH:
            item->pos.z =
                ROUND_TO_SECTOR(item->pos.z) + M_CRAWL_TO_HANG_XZ_OFFSET;
            item->pos.x += coll->shift.x;
            break;

        case DIR_WEST:
            item->pos.x =
                ROUND_TO_SECTOR(item->pos.x) + M_CRAWL_TO_HANG_XZ_OFFSET;
            item->pos.z += coll->shift.z;
            break;

        default:
            break;
        }
    } else {
        item->pos.y = edge - bounds->min.y;
    }

    item->rot.y = angle;
    item->speed = 2;
    item->fall_speed = 1;
    item->gravity = true;
    lara->gun_status = LGS_HANDS_BUSY;
}

// clang-format off
REGISTER_LARA_COL(LS_CROUCH_IDLE,       M_Crouch)
REGISTER_LARA_COL(LS_CROUCH_ROLL,       M_CrouchRoll)
REGISTER_LARA_COL(LS_CROUCH_TURN_LEFT,  M_CrouchTurn)
REGISTER_LARA_COL(LS_CROUCH_TURN_RIGHT, M_CrouchTurn)
REGISTER_LARA_COL(LS_CRAWL_IDLE,        M_CrawlIdle)
REGISTER_LARA_COL(LS_CRAWL_FORWARD,     M_CrawlForward)
REGISTER_LARA_COL(LS_CRAWL_TURN_LEFT,   M_CrawlTurn)
REGISTER_LARA_COL(LS_CRAWL_TURN_RIGHT,  M_CrawlTurn)
REGISTER_LARA_COL(LS_CRAWL_BACK,        M_CrawlBack)
REGISTER_LARA_COL(LS_CRAWL_TO_CLIMB,    M_CrawlToClimb)
// clang-format on
