#include <trx/config.h>
#include <trx/game/input.h>
#include <trx/game/lara.h>
#include <trx/game/lara/util.h>
#include <trx/game/rooms.h>
#include <trx/version.h>

// clang-format off
#define M_CLIMB_SHIFT            70
#define M_CLIMB_HANG             900
#define M_CLIMB_WIDTH_LEFT       80
#define M_CLIMB_WIDTH_RIGHT      120
#define M_CLIMB_HEIGHT           (WALL_L / 2) // = 512
#define M_VAULT_ANGLE            (30 * DEG_1) // = 5460
#define M_VAULT_GAP              (-LARA_HEIGHT + STEP_L / 8) // = -730
#define M_LF_HANG                21
#define M_LF_STOP_HANG           9
#define M_LF_CLIMB_L_SHIFT_START 28
#define M_LF_CLIMB_L_SHIFT_END   29
#define M_LF_CLIMB_R_SHIFT       57
#define M_LEDGE_JUMP_PUSH_HEIGHT (STEP_L - 16)                    // = 240
#define M_LEDGE_JUMP_HEIGHT_UP   (LARA_HEIGHT + (STEP_L * 3) / 8) // = 858
#define M_LEDGE_JUMP_HEIGHT_BACK (LARA_HEIGHT - (STEP_L * 5) / 4) // = 442
#define M_HANG_SHIFT             (g_TRVersion >= 3 ? 4 : 2)
// clang-format on

typedef enum {
    // clang-format off
    CLIMB_RESULT_CRAWL = -2,
    CLIMB_RESULT_NEG  = -1,
    CLIMB_RESULT_NONE = 0,
    CLIMB_RESULT_POS  = 1,
    // clang-format on
} M_CLIMB_RESULT;

static M_CLIMB_RESULT M_TestClimbPos(
    const ITEM *const item, const int32_t front, const int32_t right,
    const int32_t origin, const int32_t item_height, int32_t *const shift)
{
    const int32_t y = item->pos.y + origin;
    int32_t x;
    int32_t z;
    int32_t x_front = 0;
    int32_t z_front = 0;

    switch (Math_GetDirection(item->rot.y)) {
    case DIR_NORTH:
        x = item->pos.x + right;
        z = item->pos.z + front;
        z_front = M_HANG_SHIFT;
        break;

    case DIR_EAST:
        x = item->pos.x + front;
        z = item->pos.z - right;
        x_front = M_HANG_SHIFT;
        break;

    case DIR_SOUTH:
        x = item->pos.x - right;
        z = item->pos.z - front;
        z_front = -M_HANG_SHIFT;
        break;

    case DIR_WEST:
        x = item->pos.x - front;
        z = item->pos.z + right;
        x_front = -M_HANG_SHIFT;
        break;

    default:
        x = front;
        z = front;
        break;
    }

    *shift = 0;
    bool hang = true;
    if (!Lara_GetLaraInfo()->climb_status) {
        return CLIMB_RESULT_NONE;
    }

    int16_t room_num = item->room_num;
    XYZ_32 sample_pos = { x, y - 128, z };
    const SECTOR *sector = Room_GetSector(sample_pos, &room_num);
    sample_pos.y = y;
    int32_t height = Room_GetHeight(sector, sample_pos);
    if (height == NO_HEIGHT) {
        return CLIMB_RESULT_NONE;
    }

    height -= y + item_height + STEP_L / 2;
    if (height < -M_CLIMB_SHIFT) {
        return CLIMB_RESULT_NONE;
    }
    if (height < 0) {
        *shift = height;
    }

    int32_t ceiling = Room_GetCeiling(sector, sample_pos) - y;
    if (ceiling > M_CLIMB_SHIFT) {
        return CLIMB_RESULT_NONE;
    }
    if (ceiling > 0) {
        if (*shift) {
            return CLIMB_RESULT_NONE;
        }
        *shift = ceiling;
    }

    if (item_height + height < M_CLIMB_HANG) {
        hang = false;
    }

    const int32_t x2 = x + x_front;
    const int32_t z2 = z + z_front;
    sample_pos.x = x2;
    sample_pos.y = y;
    sample_pos.z = z2;
    sector = Room_GetSector(sample_pos, &room_num);
    height = Room_GetHeight(sector, sample_pos);
    if (height != NO_HEIGHT) {
        height -= y;
    }

    if (height > M_CLIMB_SHIFT) {
        ceiling = Room_GetCeiling(sector, sample_pos) - y;
        if (ceiling >= M_CLIMB_HEIGHT) {
            return CLIMB_RESULT_POS;
        }

        if (ceiling > M_CLIMB_HEIGHT - M_CLIMB_SHIFT) {
            if (*shift > 0) {
                return hang ? CLIMB_RESULT_NEG : CLIMB_RESULT_NONE;
            }
            *shift = ceiling - M_CLIMB_HEIGHT;
            return CLIMB_RESULT_POS;
        }

        if (ceiling > 0) {
            return hang ? CLIMB_RESULT_NEG : CLIMB_RESULT_NONE;
        }

        if (ceiling > -M_CLIMB_SHIFT && hang && *shift <= 0) {
            if (*shift > ceiling) {
                *shift = ceiling;
            }

            return CLIMB_RESULT_NEG;
        }

        return CLIMB_RESULT_NONE;
    }

    if (height > 0) {
        if (*shift < 0) {
            return CLIMB_RESULT_NONE;
        }
        if (height > *shift) {
            *shift = height;
        }
    }

    room_num = item->room_num;
    sample_pos = (XYZ_32) { x, item_height + y, z };
    Room_GetSector(sample_pos, &room_num);
    sample_pos.x = x2;
    sample_pos.z = z2;
    sector = Room_GetSector(sample_pos, &room_num);
    ceiling = Room_GetCeiling(sector, sample_pos);
    if (ceiling == NO_HEIGHT) {
        return CLIMB_RESULT_POS;
    }

    ceiling -= y;
    if (ceiling <= height) {
        return CLIMB_RESULT_POS;
    }

    if (ceiling >= M_CLIMB_HEIGHT) {
        return CLIMB_RESULT_POS;
    }

    if (ceiling > M_CLIMB_HEIGHT - M_CLIMB_SHIFT) {
        if (*shift > 0) {
            return hang ? CLIMB_RESULT_NEG : CLIMB_RESULT_NONE;
        }
        *shift = ceiling - M_CLIMB_HEIGHT;
        return CLIMB_RESULT_POS;
    }

    return hang ? CLIMB_RESULT_NEG : CLIMB_RESULT_NONE;
}

static bool M_TestClimbStance(ITEM *const item, COLL_INFO *const coll)
{
    int32_t shift_r;
    const M_CLIMB_RESULT result_r = M_TestClimbPos(
        item, coll->radius, coll->radius + M_CLIMB_WIDTH_RIGHT, -700,
        STEP_L * 2, &shift_r);
    if (result_r != CLIMB_RESULT_POS) {
        return false;
    }

    int32_t shift_l;
    const M_CLIMB_RESULT result_l = M_TestClimbPos(
        item, coll->radius, -(coll->radius + M_CLIMB_WIDTH_LEFT), -700,
        STEP_L * 2, &shift_l);
    if (result_l != CLIMB_RESULT_POS) {
        return false;
    }

    int32_t shift = 0;
    if (shift_r) {
        if (shift_l) {
            if ((shift_r < 0) != (shift_l < 0)) {
                return false;
            }
            if (shift_r < 0 && shift_l < shift_r) {
                shift = shift_l;
            } else if (shift_r > 0 && shift_l > shift_r) {
                shift = shift_l;
            } else {
                shift = shift_r;
            }
        } else {
            shift = shift_r;
        }
    } else if (shift_l) {
        shift = shift_l;
    }

    item->pos.y += shift;
    return true;
}

void Lara_Col_HangTest(ITEM *const item, COLL_INFO *const coll)
{
    coll->bad_pos = NO_BAD_POS;
    coll->bad_neg = NO_BAD_NEG;
    coll->bad_ceiling = 0;
    Lara_Col_GetInfo(item, coll);
    const bool flag = coll->side_front.floor < 200;

    item->gravity = false;
    item->fall_speed = 0;
    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->move_angle = item->rot.y;

    const DIRECTION dir = Math_GetDirection(item->rot.y);
    switch (dir) {
    case DIR_NORTH:
        item->pos.z += M_HANG_SHIFT;
        break;
    case DIR_EAST:
        item->pos.x += M_HANG_SHIFT;
        break;
    case DIR_SOUTH:
        item->pos.z -= M_HANG_SHIFT;
        break;
    case DIR_WEST:
        item->pos.x -= M_HANG_SHIFT;
        break;
    default:
        break;
    }

    coll->bad_pos = NO_BAD_POS;
    coll->bad_neg = -STEPUP_HEIGHT;
    coll->bad_ceiling = 0;
    Lara_Col_GetInfo(item, coll);

    if (lara->climb_status) {
        if (!g_Input.action || item->hit_points <= 0) {
            XYZ_32 pos = {
                .x = 0,
                .y = 0,
                .z = 0,
            };
            Collide_GetJointAbsPosition(item, &pos, 0);
            if (dir == DIR_NORTH || dir == DIR_SOUTH) {
                item->pos.x = pos.x;
            } else {
                item->pos.z = pos.z;
            }

            item->goal_anim_state = LS(LS_JUMP_FORWARD);
            item->current_anim_state = LS(LS_JUMP_FORWARD);
            Item_SwitchToAnim(item, LA(LA_FALL_START), 0);
            item->pos.y += STEP_L;
            item->gravity = true;
            item->speed = 2;
            item->fall_speed = 1;
            lara->gun_status = LGS_ARMLESS;
            return;
        }

        if (!Lara_Col_TestLadderHang(item, coll)) {
            item->pos.x = coll->old.x;
            item->pos.y = coll->old.y;
            item->pos.z = coll->old.z;
            item->goal_anim_state = LS(LS_HANG);
            item->current_anim_state = LS(LS_HANG);
            Item_SwitchToAnim(item, LA(LA_REACH_TO_HANG), M_LF_HANG);
            return;
        }

        if (Item_TestAnimEqual(item, LA(LA_REACH_TO_HANG))
            && Item_TestFrameEqual(item, M_LF_HANG)
            && M_TestClimbStance(item, coll)) {
            item->goal_anim_state = LS(LS_CLIMB_STANCE);
        }
        return;
    }

    if (!g_Input.action || item->hit_points <= 0
        || coll->side_front.floor > 0) {
        item->goal_anim_state = LS(LS_JUMP_UP);
        item->current_anim_state = LS(LS_JUMP_UP);
        Item_SwitchToAnim(item, LA(LA_JUMP_UP), M_LF_STOP_HANG);
        const BOUNDS_16 *const bounds = Item_GetBoundsAccurate(item);
        if (g_Config.gameplay.enable_swing_cancel && item->hit_points > 0) {
            item->pos.y += bounds->max.y;
        } else {
            item->pos.y += coll->side_front.floor - bounds->min.y + 2;
        }
        item->pos.x += coll->shift.x;
        item->pos.z += coll->shift.z;
        item->gravity = true;
        item->speed = 2;
        item->fall_speed = 1;
        lara->gun_status = LGS_ARMLESS;
        return;
    }

    const BOUNDS_16 *const bounds = Item_GetBoundsAccurate(item);
    const int32_t hdif = coll->side_front.floor - bounds->min.y;

    if (ABS(coll->side_left2.floor - coll->side_right2.floor) >= SLOPE_DIF
        || coll->side_mid.ceiling >= 0 || coll->coll_type != COLL_FRONT || flag
        || coll->hit_static || hdif < -SLOPE_DIF || hdif > SLOPE_DIF) {
        item->pos = coll->old;
        if (item->current_anim_state == LS(LS_SHIMMY_LEFT)
            || item->current_anim_state == LS(LS_SHIMMY_RIGHT)) {
            item->goal_anim_state = LS(LS_HANG);
            item->current_anim_state = LS(LS_HANG);
            Item_SwitchToAnim(item, LA(LA_REACH_TO_HANG), M_LF_HANG);
        }
        return;
    }

    switch (dir) {
    case DIR_NORTH:
    case DIR_SOUTH:
        item->pos.z += coll->shift.z;
        break;

    case DIR_EAST:
    case DIR_WEST:
        item->pos.x += coll->shift.x;
        break;

    default:
        break;
    }

    if (g_TRVersion >= 2 || (hdif >= -STEP_L && hdif <= STEP_L)) {
        item->pos.y += hdif;
    }
}

static bool M_TestLadderRelease(ITEM *const item)
{
    item->gravity = false;
    item->fall_speed = 0;

    if (g_Input.action && item->hit_points > 0) {
        return false;
    }

    item->goal_anim_state = LS(LS_JUMP_FORWARD);
    item->current_anim_state = LS(LS_JUMP_FORWARD);
    Item_SwitchToAnim(item, LA(LA_FALL_START), 0);
    item->gravity = true;
    item->speed = 2;
    item->fall_speed = 1;
    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->gun_status = LGS_ARMLESS;
    return true;
}

static M_CLIMB_RESULT M_TestClimbUpPos(
    const ITEM *const item, const int32_t front, const int32_t right,
    int32_t *const shift, int32_t *const ledge)
{
    const int32_t y = item->pos.y - M_CLIMB_HEIGHT - STEP_L;
    int32_t x;
    int32_t z;
    int32_t x_front = 0;
    int32_t z_front = 0;

    switch (Math_GetDirection(item->rot.y)) {
    case DIR_NORTH:
        x = item->pos.x + right;
        z = item->pos.z + front;
        z_front = M_HANG_SHIFT;
        break;

    case DIR_EAST:
        x = item->pos.x + front;
        z = item->pos.z - right;
        x_front = M_HANG_SHIFT;
        break;

    case DIR_SOUTH:
        x = item->pos.x - right;
        z = item->pos.z - front;
        z_front = -M_HANG_SHIFT;
        break;

    case DIR_WEST:
        z = item->pos.z + right;
        x = item->pos.x - front;
        x_front = -M_HANG_SHIFT;
        break;

    default:
        x = front;
        z = front;
        break;
    }

    *shift = 0;

    const SECTOR *sector;
    int32_t height;
    int32_t ceiling;

    int16_t room_num = item->room_num;
    XYZ_32 sample_pos = { x, y, z };
    sector = Room_GetSector(sample_pos, &room_num);
    ceiling = Room_GetCeiling(sector, sample_pos) + STEP_L - y;
    if (ceiling > M_CLIMB_SHIFT) {
        return CLIMB_RESULT_NONE;
    }

    if (ceiling > 0) {
        *shift = ceiling;
    }

    const int32_t x2 = x + x_front;
    const int32_t z2 = z + z_front;
    sample_pos.x = x2;
    sample_pos.z = z2;
    sector = Room_GetSector(sample_pos, &room_num);
    height = Room_GetHeight(sector, sample_pos);
    if (height == NO_HEIGHT) {
        *ledge = NO_HEIGHT;
        return CLIMB_RESULT_POS;
    }

    height -= y;
    *ledge = height;
    if (height > STEP_L / 2) {
        ceiling = Room_GetCeiling(sector, sample_pos) - y;
        if (ceiling >= M_CLIMB_HEIGHT) {
            return CLIMB_RESULT_POS;
        }

        if (height - ceiling > LARA_HEIGHT) {
            *shift = height;
            return CLIMB_RESULT_NEG;
        }

        if (g_Config.gameplay.enable_crawling
            && height - ceiling >= M_CLIMB_HEIGHT) {
            return CLIMB_RESULT_CRAWL;
        }

        return CLIMB_RESULT_NONE;
    }

    if (height > 0 && height > *shift) {
        *shift = height;
    }

    room_num = item->room_num;
    sample_pos = (XYZ_32) { x, y + M_CLIMB_HEIGHT, z };
    Room_GetSector(sample_pos, &room_num);
    sample_pos.x = x2;
    sample_pos.z = z2;
    sector = Room_GetSector(sample_pos, &room_num);
    ceiling = Room_GetCeiling(sector, sample_pos) - y;
    if (ceiling <= height) {
        return CLIMB_RESULT_POS;
    }

    if (ceiling >= M_CLIMB_HEIGHT) {
        return CLIMB_RESULT_POS;
    }
    return CLIMB_RESULT_NONE;
}

static bool M_TestLedgeJump(const ITEM *const item, const COLL_INFO *const coll)
{
    if (!g_Input.jump || !(g_Input.forward ^ g_Input.back)
        || (g_Input.forward && g_Input.slow)
        || !g_Config.gameplay.enable_ledge_jumps
        || !Lara_State_IsResponsive(LA_REACH_TO_HANG)) {
        return false;
    }

    // Lara needs sufficient space above to avoid the animation pushing her into
    // the ceiling.
    const int32_t jump_height =
        g_Input.forward ? M_LEDGE_JUMP_HEIGHT_UP : M_LEDGE_JUMP_HEIGHT_BACK;
    if (coll->side_mid.ceiling >= -jump_height) {
        return false;
    }

    // Test for a solid surface in front of Lara to push against.
    const XYZ_32 pos = {
        .x = item->pos.x + ((Math_Sin(item->rot.y) * STEP_L) >> W2V_SHIFT),
        .z = item->pos.z + ((Math_Cos(item->rot.y) * STEP_L) >> W2V_SHIFT),
        .y = item->pos.y,
    };
    int16_t room_num = item->room_num;
    const SECTOR *const sector = Room_GetSector(pos, &room_num);
    const int32_t height = Room_GetHeight(sector, pos);
    const int32_t ceiling = Room_GetCeiling(sector, pos);
    return height == NO_HEIGHT || height < pos.y
        || (ceiling - pos.y) >= -M_LEDGE_JUMP_PUSH_HEIGHT;
}

static void M_Hang(ITEM *const item, COLL_INFO *const coll)
{
    Lara_Col_HangTest(item, coll);
    if (item->goal_anim_state != LS(LS_HANG)) {
        return;
    }

    LARA_INFO *const lara = Lara_GetLaraInfo();
    if (!lara->climb_status && M_TestLedgeJump(item, coll)) {
        item->goal_anim_state = LS(g_Input.forward ? LS_JUMP_UP : LS_JUMP_BACK);
        return;
    }

    if (g_Input.forward) {
        if (coll->side_front.floor > -850 && coll->side_front.floor < -650
            && coll->side_front.floor - coll->side_front.ceiling >= 0
            && coll->side_left2.floor - coll->side_left2.ceiling >= 0
            && coll->side_right2.floor - coll->side_right2.ceiling >= 0
            && !coll->hit_static) {
            item->goal_anim_state = LS(g_Input.slow ? LS_GYMNAST : LS_PULL_UP);
            return;
        } else if (
            lara->climb_status && Item_TestAnimEqual(item, LA(LA_REACH_TO_HANG))
            && Item_TestFrameEqual(item, M_LF_HANG)
            && coll->side_mid.ceiling <= -256) {
            item->goal_anim_state = LS(LS_HANG);
            item->current_anim_state = LS(LS_HANG);
            Item_SwitchToAnim(item, LA(LA_LADDER_UP_HANGING), 0);
            return;
        }
    }

    if (g_Config.gameplay.enable_crawling && (g_Input.forward || g_Input.crouch)
        && coll->side_front.floor > -850 && coll->side_front.floor < -650
        && coll->side_front.floor - coll->side_front.ceiling >= -256
        && coll->side_left2.floor - coll->side_left2.ceiling >= -256
        && coll->side_right2.floor - coll->side_right2.ceiling >= -256
        && !coll->hit_static) {
        item->goal_anim_state = LS(LS_CLIMB_TO_CRAWL);
        item->required_anim_state = LS(LS_CROUCH_IDLE);
        lara->crouching = true;
    } else if (
        g_Input.back && lara->climb_status
        && Item_TestAnimEqual(item, LA(LA_REACH_TO_HANG))
        && Item_TestFrameEqual(item, M_LF_HANG)) {
        item->goal_anim_state = LS(LS_HANG);
        item->current_anim_state = LS(LS_HANG);
        Item_SwitchToAnim(item, LA(LA_LADDER_DOWN_HANGING), 0);
    }
}

static void M_Shimmy(ITEM *const item, COLL_INFO *const coll)
{
    const int32_t angle =
        item->current_anim_state == LS(LS_SHIMMY_LEFT) ? -DEG_90 : DEG_90;
    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->move_angle = item->rot.y + angle;
    Lara_Col_HangTest(item, coll);
    lara->move_angle = item->rot.y + angle;
}

static void M_StanceLadder(ITEM *const item, COLL_INFO *const coll)
{
    if (M_TestLadderRelease(item)
        || !Item_TestAnimEqual(item, LA(LA_LADDER_IDLE))) {
        return;
    }

    LARA_INFO *const lara = Lara_GetLaraInfo();
    if (g_Input.forward) {
        if (item->goal_anim_state == LS(LS_PULL_UP)) {
            return;
        }

        item->goal_anim_state = LS(LS_CLIMB_STANCE);

        int32_t shift_r = 0;
        int32_t ledge_r = 0;
        M_CLIMB_RESULT result_r = M_TestClimbUpPos(
            item, coll->radius, coll->radius + M_CLIMB_WIDTH_RIGHT, &shift_r,
            &ledge_r);

        int32_t shift_l = 0;
        int32_t ledge_l = 0;
        M_CLIMB_RESULT result_l = M_TestClimbUpPos(
            item, coll->radius, -(coll->radius + M_CLIMB_WIDTH_LEFT), &shift_l,
            &ledge_l);

        if (result_r == CLIMB_RESULT_NONE || result_l == CLIMB_RESULT_NONE) {
            return;
        }

        if (result_r == CLIMB_RESULT_NEG || result_l == CLIMB_RESULT_NEG
            || result_r == CLIMB_RESULT_CRAWL
            || result_l == CLIMB_RESULT_CRAWL) {
            if (ABS(ledge_l - ledge_r) > 120) {
                return;
            }
            if (result_r == CLIMB_RESULT_NEG && result_l == CLIMB_RESULT_NEG) {
                item->goal_anim_state = LS(LS_PULL_UP);
                item->pos.y += (ledge_l + ledge_r) / 2 - STEP_L;
            } else {
                item->goal_anim_state = LS(LS_CLIMB_TO_CRAWL);
                item->required_anim_state = LS(LS_CROUCH_IDLE);
                lara->crouching = true;
            }
            return;
        }

        int32_t shift = shift_l;
        if (shift_r) {
            if (shift_l) {
                if ((shift_r < 0) != (shift_l < 0)) {
                    return;
                }
                if (shift_r > 0 && shift_r > shift_l) {
                    shift = shift_r;
                } else if (shift_r < 0 && shift_r < shift_l) {
                    shift = shift_r;
                }
            } else {
                shift = shift_r;
            }
        }

        item->goal_anim_state = LS(LS_CLIMBING);
        item->pos.y += shift;
    } else if (g_Input.back) {
        if (item->goal_anim_state == LS(LS_HANG)) {
            return;
        }

        item->goal_anim_state = LS(LS_CLIMB_STANCE);
        item->pos.y += STEP_L;

        int32_t shift_r = 0;
        const M_CLIMB_RESULT result_r = M_TestClimbPos(
            item, coll->radius, coll->radius + M_CLIMB_WIDTH_RIGHT,
            -M_CLIMB_HEIGHT, M_CLIMB_HEIGHT, &shift_r);

        int32_t shift_l = 0;
        const M_CLIMB_RESULT result_l = M_TestClimbPos(
            item, coll->radius, -(coll->radius + M_CLIMB_WIDTH_LEFT),
            -M_CLIMB_HEIGHT, M_CLIMB_HEIGHT, &shift_l);

        item->pos.y -= STEP_L;
        if (result_r == CLIMB_RESULT_NONE || result_l == CLIMB_RESULT_NONE) {
            return;
        }

        int32_t shift = shift_l;
        if (shift_r && shift_l) {
            if ((shift_r < 0) != (shift_l < 0)) {
                return;
            }
            if (shift_r < 0 && shift_r < shift_l) {
                shift = shift_r;
            } else if (shift_r > 0 && shift_r > shift_l) {
                shift = shift_r;
            }
        }

        if (result_r == CLIMB_RESULT_POS && result_l == CLIMB_RESULT_POS) {
            item->goal_anim_state = LS(LS_CLIMB_DOWN);
            item->pos.y += shift;
        } else {
            item->goal_anim_state = LS(LS_HANG);
        }
    }
}

static void M_SideLadder(ITEM *const item, COLL_INFO *const coll)
{
    if (M_TestLadderRelease(item)) {
        return;
    }

    LARA_INFO *const lara = Lara_GetLaraInfo();
    int32_t right;
    if (item->current_anim_state == LS(LS_CLIMB_LEFT)) {
        lara->move_angle = item->rot.y - DEG_90;
        right = -(coll->radius + M_CLIMB_WIDTH_LEFT);
    } else {
        lara->move_angle = item->rot.y + DEG_90;
        right = coll->radius + M_CLIMB_WIDTH_RIGHT;
    }

    int32_t shift;
    const M_CLIMB_RESULT result = M_TestClimbPos(
        item, coll->radius, right, -M_CLIMB_HEIGHT, M_CLIMB_HEIGHT, &shift);

    if (result == CLIMB_RESULT_POS) {
        if (g_Input.left) {
            item->goal_anim_state = LS(LS_CLIMB_LEFT);
        } else if (g_Input.right) {
            item->goal_anim_state = LS(LS_CLIMB_RIGHT);
        } else {
            item->goal_anim_state = LS(LS_CLIMB_STANCE);
        }
        item->pos.y += shift;
        return;
    }

    if (result != CLIMB_RESULT_NONE) {
        item->goal_anim_state = LS(LS_HANG);
        do {
            Item_Animate(item);
        } while (item->current_anim_state != LS(LS_HANG));
        item->pos.x = coll->old.x;
        item->pos.z = coll->old.z;
        return;
    }

    item->pos.x = coll->old.x;
    item->pos.z = coll->old.z;
    item->goal_anim_state = LS(LS_CLIMB_STANCE);
    item->current_anim_state = LS(LS_CLIMB_STANCE);
    if (coll->old_anim_state == LS(LS_CLIMB_STANCE)) {
        item->frame_num = coll->old_frame_num;
        item->anim_num = coll->old_anim_num;
        Lara_Animate(item);
    } else {
        Item_SwitchToAnim(item, LA(LA_LADDER_IDLE), 0);
    }
}

static void M_UpLadder(ITEM *const item, COLL_INFO *const coll)
{
    if (M_TestLadderRelease(item)
        || !Item_TestAnimEqual(item, LA(LA_LADDER_UP))) {
        return;
    }

    int32_t yshift;
    if (Item_TestFrameEqual(item, 0)) {
        yshift = 0;
    } else if (
        Item_TestFrameRange(
            item, M_LF_CLIMB_L_SHIFT_START, M_LF_CLIMB_L_SHIFT_END)) {
        yshift = -STEP_L;
    } else if (Item_TestFrameEqual(item, M_LF_CLIMB_R_SHIFT)) {
        yshift = -STEP_L * 2;
    } else {
        return;
    }

    item->pos.y += yshift - STEP_L;

    int32_t shift_r = 0;
    int32_t ledge_r = 0;
    M_CLIMB_RESULT result_r = M_TestClimbUpPos(
        item, coll->radius, coll->radius + M_CLIMB_WIDTH_RIGHT, &shift_r,
        &ledge_r);

    int32_t shift_l = 0;
    int32_t ledge_l = 0;
    M_CLIMB_RESULT result_l = M_TestClimbUpPos(
        item, coll->radius, -(coll->radius + M_CLIMB_WIDTH_LEFT), &shift_l,
        &ledge_l);

    item->pos.y += STEP_L;

    if (result_r == CLIMB_RESULT_NONE || result_l == CLIMB_RESULT_NONE
        || !g_Input.forward) {
        item->goal_anim_state = LS(LS_CLIMB_STANCE);
        if (yshift) {
            Lara_Animate(item);
        }
        return;
    }

    LARA_INFO *const lara = Lara_GetLaraInfo();
    if (result_r == CLIMB_RESULT_NEG || result_l == CLIMB_RESULT_NEG
        || result_r == CLIMB_RESULT_CRAWL || result_l == CLIMB_RESULT_CRAWL) {
        item->goal_anim_state = LS(LS_CLIMB_STANCE);
        Lara_Animate(item);
        if (ABS(ledge_l - ledge_r) <= 120) {
            if (result_r == CLIMB_RESULT_NEG || result_l == CLIMB_RESULT_NEG) {
                item->goal_anim_state = LS(LS_PULL_UP);
                item->pos.y += (ledge_r + ledge_l) / 2 - STEP_L;
            } else {
                item->goal_anim_state = LS(LS_CLIMB_TO_CRAWL);
                item->required_anim_state = LS(LS_CROUCH_IDLE);
                lara->crouching = true;
            }
        }
        return;
    }

    item->goal_anim_state = LS(LS_CLIMBING);
    item->pos.y -= yshift;
}

static void M_DownLadder(ITEM *const item, COLL_INFO *const coll)
{
    if (M_TestLadderRelease(item)
        || !Item_TestAnimEqual(item, LA(LA_LADDER_DOWN))) {
        return;
    }

    int32_t yshift;
    if (Item_TestFrameEqual(item, 0)) {
        yshift = 0;
    } else if (
        Item_TestFrameRange(
            item, M_LF_CLIMB_L_SHIFT_START, M_LF_CLIMB_L_SHIFT_END)) {
        yshift = STEP_L;
    } else if (Item_TestFrameEqual(item, M_LF_CLIMB_R_SHIFT)) {
        yshift = STEP_L * 2;
    } else {
        return;
    }

    item->pos.y += yshift + STEP_L;

    int32_t shift_r = 0;
    const M_CLIMB_RESULT result_r = M_TestClimbPos(
        item, coll->radius, coll->radius + M_CLIMB_WIDTH_RIGHT, -M_CLIMB_HEIGHT,
        M_CLIMB_HEIGHT, &shift_r);

    int32_t shift_l = 0;
    const M_CLIMB_RESULT result_l = M_TestClimbPos(
        item, coll->radius, -(coll->radius + M_CLIMB_WIDTH_LEFT),
        -M_CLIMB_HEIGHT, M_CLIMB_HEIGHT, &shift_l);

    item->pos.y -= STEP_L;

    if (result_r == CLIMB_RESULT_NONE || result_l == CLIMB_RESULT_NONE
        || !g_Input.back) {
        item->goal_anim_state = LS(LS_CLIMB_STANCE);
        if (yshift != 0) {
            Lara_Animate(item);
        }
        return;
    }

#if 0
    int32_t shift = shift_l;
#endif
    if (shift_r && shift_l) {
        if ((shift_r < 0) != (shift_l < 0)) {
            item->goal_anim_state = LS(LS_CLIMB_STANCE);
            Lara_Animate(item);
            return;
        }
#if 0
        if (shift_r < 0 && shift_r < shift_l) {
            shift = shift_r;
        } else if (shift_r > 0 && shift_r > shift_l) {
            shift = shift_r;
        }
#endif
    }

    if (result_r == CLIMB_RESULT_NEG || result_l == CLIMB_RESULT_NEG) {
        Item_SwitchToAnim(item, LA(LA_LADDER_IDLE), 0);
        item->current_anim_state = LS(LS_CLIMB_STANCE);
        item->goal_anim_state = LS(LS_HANG);
        Lara_Animate(item);
        return;
    }

    item->goal_anim_state = LS(LS_CLIMB_DOWN);
    item->pos.y -= yshift;
}

bool Lara_Col_TestVault(ITEM *const item, COLL_INFO *const coll)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    if (coll->coll_type != COLL_FRONT || !g_Input.action
        || lara->gun_status != LGS_ARMLESS) {
        return false;
    }

    const DIRECTION dir = Math_GetDirectionCone(item->rot.y, M_VAULT_ANGLE);
    if (dir == DIR_UNKNOWN) {
        return false;
    }
    const int16_t angle = Math_DirectionToAngle(dir);

    const int32_t left_floor = coll->side_left2.floor;
    const int32_t left_ceiling = coll->side_left2.ceiling;
    const int32_t right_floor = coll->side_right2.floor;
    const int32_t right_ceiling = coll->side_right2.ceiling;
    const int32_t front_floor = coll->side_front.floor;
    const int32_t front_ceiling = coll->side_front.ceiling;
    const bool slope = ABS(left_floor - right_floor) >= SLOPE_DIF;
    const int32_t mid = STEP_L / 2;
    const ROOM *const room = Room_Get(item->room_num);

    if (front_floor >= -STEP_L * 2 - mid && front_floor <= -STEP_L * 2 + mid) {
        if (slope || front_floor - front_ceiling < 0
            || left_floor - left_ceiling < 0 || right_floor - right_ceiling < 0
            || (room->flags.swamp && lara->water_surface_dist < -768)) {
            return false;
        }
        item->goal_anim_state = LS(LS_STOP);
        item->current_anim_state = LS(LS_PULL_UP);
        Item_SwitchToAnim(item, LA(LA_CLIMB_2CLICK), 0);
        item->pos.y += front_floor + STEP_L * 2;
        lara->gun_status = LGS_HANDS_BUSY;
    } else if (
        front_floor >= -STEP_L * 3 - mid && front_floor <= -STEP_L * 3 + mid) {
        if (slope || front_floor - front_ceiling < 0
            || left_floor - left_ceiling < 0 || right_floor - right_ceiling < 0
            || (room->flags.swamp && lara->water_surface_dist < -768)) {
            return false;
        }
        item->goal_anim_state = LS(LS_STOP);
        item->current_anim_state = LS(LS_PULL_UP);
        Item_SwitchToAnim(item, LA(LA_CLIMB_3CLICK), 0);
        item->pos.y += front_floor + STEP_L * 3;
        lara->gun_status = LGS_HANDS_BUSY;
    } else if (
        !lara->climb_status
        && front_floor - coll->side_mid.ceiling < M_VAULT_GAP) {
        return false;
    } else if (
        !slope && front_floor >= -STEP_L * 7 - mid
        && front_floor <= -STEP_L * 4 + mid) {
        if (room->flags.swamp) {
            return false;
        }
        item->goal_anim_state = LS(LS_JUMP_UP);
        item->current_anim_state = LS(LS_STOP);
        Item_SwitchToAnim(item, LA(LA_STAND_STILL), 0);
        lara->calc_fall_speed =
            -(Math_Sqrt(-2 * GRAVITY * (front_floor + 800)) + 3);
        Lara_Animate(item);
    } else if (
        lara->climb_status && front_floor <= -1920
        && lara->water_status != LWS_WADE && left_floor <= -STEP_L * 8 + mid
        && right_floor <= -STEP_L * 8
        && coll->side_mid.ceiling <= -STEP_L * 8 + mid + LARA_HEIGHT) {
        item->goal_anim_state = LS(LS_JUMP_UP);
        item->current_anim_state = LS(LS_STOP);
        Item_SwitchToAnim(item, LA(LA_STAND_STILL), 0);
        lara->calc_fall_speed = -116;
        Lara_Animate(item);
    } else if (
        lara->climb_status
        && (front_floor < -STEP_L * 4 || front_ceiling >= LARA_HEIGHT - STEP_L)
        && coll->side_mid.ceiling <= -STEP_L * 5 + LARA_HEIGHT) {
        Lara_Col_Shift(coll);
        if (M_TestClimbStance(item, coll)) {
            item->goal_anim_state = LS(LS_CLIMB_STANCE);
            item->current_anim_state = LS(LS_STOP);
            Item_SwitchToAnim(item, LA(LA_STAND_STILL), 0);
            Lara_Animate(item);
            item->rot.y = angle;
            lara->gun_status = LGS_HANDS_BUSY;
            lara->sprinting = false;
            return true;
        }
        return false;
    } else {
        return false;
    }

    item->rot.y = angle;
    Lara_Col_Shift(coll);
    lara->sprinting = false;
    lara->crouching = false;
    return true;
}

bool Lara_Col_TestLadderHang(ITEM *const item, const COLL_INFO *const coll)
{
    const LARA_INFO *const lara = Lara_GetLaraInfo();
    if (!lara->climb_status || item->fall_speed < 0) {
        return false;
    }

    const DIRECTION dir = Math_GetDirection(item->rot.y);
    switch (dir) {
    case DIR_NORTH:
    case DIR_SOUTH:
        item->pos.z += coll->shift.z;
        break;

    case DIR_EAST:
    case DIR_WEST:
        item->pos.x += coll->shift.x;
        break;

    default:
        break;
    }

    const BOUNDS_16 *const bounds = Item_GetBoundsAccurate(item);
    const int32_t y = bounds->min.y;
    const int32_t h = bounds->max.y - y;

    int32_t shift;
    if (M_TestClimbPos(item, coll->radius, coll->radius, y, h, &shift)
        == CLIMB_RESULT_NONE) {
        return false;
    }

    if (M_TestClimbPos(item, coll->radius, -coll->radius, y, h, &shift)
        == CLIMB_RESULT_NONE) {
        return false;
    }

    const M_CLIMB_RESULT result =
        M_TestClimbPos(item, coll->radius, 0, y, h, &shift);
    if (result == CLIMB_RESULT_NEG) {
        item->pos.y += shift;
    }
    return result != CLIMB_RESULT_NONE;
}

// clang-format off
REGISTER_LARA_COL(LS_HANG,         M_Hang)
REGISTER_LARA_COL(LS_SHIMMY_LEFT,  M_Shimmy)
REGISTER_LARA_COL(LS_SHIMMY_RIGHT, M_Shimmy)
REGISTER_LARA_COL(LS_CLIMB_STANCE, M_StanceLadder)
REGISTER_LARA_COL(LS_CLIMB_LEFT,   M_SideLadder)
REGISTER_LARA_COL(LS_CLIMB_RIGHT,  M_SideLadder)
REGISTER_LARA_COL(LS_CLIMBING,     M_UpLadder)
REGISTER_LARA_COL(LS_CLIMB_DOWN,   M_DownLadder)
// clang-format on
