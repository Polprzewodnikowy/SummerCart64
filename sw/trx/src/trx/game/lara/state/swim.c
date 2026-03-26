#include <trx/config.h>
#include <trx/game/camera.h>
#include <trx/game/input.h>
#include <trx/game/lara.h>
#include <trx/game/lara/util.h>

// clang-format off
#define M_FRICTION       6
#define M_LEAN_RATE      (2 * LARA_LEAN_RATE) // = 546
#define M_TURN_RATE      (2 * DEG_1)          // = 364
#define M_MAX_SURF_SPEED 60
#define M_MAX_SWIM_SPEED 200
// clang-format on

static void M_SwimTurn(ITEM *const item)
{
    if (g_Input.forward) {
        item->rot.x -= M_TURN_RATE;
    } else if (g_Input.back) {
        item->rot.x += M_TURN_RATE;
    }

    if (g_Config.gameplay.enable_tr2_swimming) {
        LARA_INFO *const lara = Lara_GetLaraInfo();
        if (g_Input.left) {
            lara->turn_rate -= LARA_TURN_RATE;
            CLAMPL(lara->turn_rate, -LARA_MED_TURN);
            item->rot.z -= M_LEAN_RATE;
        } else if (g_Input.right) {
            lara->turn_rate += LARA_TURN_RATE;
            CLAMPG(lara->turn_rate, LARA_MED_TURN);
            item->rot.z += M_LEAN_RATE;
        }
    } else {
        if (g_Input.left) {
            item->rot.y -= LARA_MED_TURN;
            item->rot.z -= M_LEAN_RATE;
        } else if (g_Input.right) {
            item->rot.y += LARA_MED_TURN;
            item->rot.z += M_LEAN_RATE;
        }
    }
}

static void M_Tread(ITEM *const item, COLL_INFO *const coll)
{
    if (item->hit_points <= 0) {
        item->goal_anim_state = LS(LS_UW_DEATH);
        return;
    }

    coll->enable_hit = 0;

    if (g_Config.gameplay.enable_uw_roll && g_Input.roll) {
        item->current_anim_state = LS(LS_WATER_ROLL);
        Item_SwitchToAnim(item, LA(LA_UNDERWATER_ROLL_START), 0);
        return;
    }

    if (g_Config.gameplay.look_mode != LOOK_MODE_RESTRICTED && g_Input.look) {
        Lara_Look_UpDown();
    }

    M_SwimTurn(item);
    if (g_Input.jump) {
        item->goal_anim_state = LS(LS_SWIM);
    }
    item->fall_speed -= M_FRICTION;
    CLAMPL(item->fall_speed, 0);

    LARA_INFO *const lara = Lara_GetLaraInfo();
    if (lara->gun_status == LGS_HANDS_BUSY) {
        lara->gun_status = LGS_ARMLESS;
    }
}

static void M_Swim(ITEM *const item, COLL_INFO *const coll)
{
    if (item->hit_points <= 0) {
        item->goal_anim_state = LS(LS_UW_DEATH);
        return;
    }

    coll->enable_hit = 0;

    if (g_Config.gameplay.enable_uw_roll && g_Input.roll) {
        item->current_anim_state = LS(LS_WATER_ROLL);
        Item_SwitchToAnim(item, LA(LA_UNDERWATER_ROLL_START), 0);
        return;
    }

    M_SwimTurn(item);
    item->fall_speed += 8;
    LARA_INFO *const lara = Lara_GetLaraInfo();
    if (lara->water_status == LWS_CHEAT) {
        CLAMPG(item->fall_speed, M_MAX_SWIM_SPEED * 2);
    } else {
        CLAMPG(item->fall_speed, M_MAX_SWIM_SPEED);
    }

    if (!g_Input.jump) {
        item->goal_anim_state =
            LS(g_Config.gameplay.enable_tr2_swim_cancel
                       && Lara_State_IsResponsive(LA_UNDERWATER_SWIM_FORWARD)
                   ? LS_RESPONSIVE
                   : LS_GLIDE);
    }
}

static void M_Glide(ITEM *item, COLL_INFO *coll)
{
    if (item->hit_points <= 0) {
        item->goal_anim_state = LS(LS_UW_DEATH);
        return;
    }

    coll->enable_hit = 0;

    if (g_Config.gameplay.enable_uw_roll && g_Input.roll) {
        item->current_anim_state = LS(LS_WATER_ROLL);
        Item_SwitchToAnim(item, LA(LA_UNDERWATER_ROLL_START), 0);
        return;
    }

    M_SwimTurn(item);
    if (g_Input.jump) {
        item->goal_anim_state = LS(LS_SWIM);
    }
    item->fall_speed -= M_FRICTION;
    CLAMPL(item->fall_speed, 0);
    if (item->fall_speed <= M_MAX_SWIM_SPEED * 2 / 3) {
        item->goal_anim_state = LS(LS_TREAD);
    }
}

static void M_TreadSurface(ITEM *const item, COLL_INFO *const coll)
{
    item->fall_speed -= 4;
    CLAMPL(item->fall_speed, 0);

    if (item->hit_points <= 0) {
        item->goal_anim_state = LS(LS_UW_DEATH);
        return;
    }

    coll->enable_hit = 0;

    if (g_Input.look) {
        Lara_Look_UpDown();
        return;
    }

    if (g_Input.left) {
        item->rot.y -= LARA_SLOW_TURN;
    } else if (g_Input.right) {
        item->rot.y += LARA_SLOW_TURN;
    }

    if (g_Input.forward) {
        item->goal_anim_state = LS(LS_SURF_SWIM);
    } else if (g_Input.back) {
        item->goal_anim_state = LS(LS_SURF_BACK);
    }

    if (g_Input.step_left) {
        item->goal_anim_state = LS(LS_SURF_LEFT);
    } else if (g_Input.step_right) {
        item->goal_anim_state = LS(LS_SURF_RIGHT);
    }

    LARA_INFO *const lara = Lara_GetLaraInfo();
    if (g_Input.jump) {
        lara->dive_timer++;
        if (lara->dive_timer == LARA_DIVE_WAIT) {
            Item_SwitchToAnim(item, LA(LA_ONWATER_DIVE), 0);
            item->goal_anim_state = LS(LS_SWIM);
            item->current_anim_state = LS(LS_DIVE);
            item->rot.x = -45 * DEG_1;
            item->fall_speed = 80;
            lara->water_status = LWS_UNDERWATER;
        }
    } else {
        lara->dive_timer = 0;
    }
}

static void M_ForwardSurface(ITEM *const item, COLL_INFO *const coll)
{
    if (item->hit_points <= 0) {
        item->goal_anim_state = LS(LS_UW_DEATH);
        return;
    }

    coll->enable_hit = 0;

    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->dive_timer = 0;
    if (g_Input.left) {
        item->rot.y -= LARA_SLOW_TURN;
    } else if (g_Input.right) {
        item->rot.y += LARA_SLOW_TURN;
    }
    if (!g_Input.forward || g_Input.jump) {
        item->goal_anim_state = LS(LS_SURF_TREAD);
    }
    item->fall_speed += 8;
    CLAMPG(item->fall_speed, M_MAX_SURF_SPEED);
}

static void M_SideBackSurface(ITEM *const item, COLL_INFO *const coll)
{
    if (item->hit_points <= 0) {
        item->goal_anim_state = LS(LS_UW_DEATH);
        return;
    }

    coll->enable_hit = 0;

    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->dive_timer = 0;

    if (g_Input.left) {
        item->rot.y -= M_TURN_RATE;
    } else if (g_Input.right) {
        item->rot.y += M_TURN_RATE;
    }

    bool stop = false;
    switch (LS_U(item->current_anim_state)) {
    case LS_SURF_BACK:
        stop = !g_Input.back;
        break;
    case LS_SURF_LEFT:
        stop = !g_Input.step_left;
        break;
    case LS_SURF_RIGHT:
        stop = !g_Input.step_right;
        break;
    default:
        break;
    }

    if (stop) {
        item->goal_anim_state = LS(LS_SURF_TREAD);
    }

    item->fall_speed += 8;
    CLAMPG(item->fall_speed, M_MAX_SURF_SPEED);
}

static void M_Dive(ITEM *const item, COLL_INFO *const coll)
{
    if (g_Input.forward) {
        item->rot.x -= DEG_1;
    }
}

static void M_UWDeath(ITEM *const item, COLL_INFO *const coll)
{
    coll->enable_hit = 0;
    item->gravity = false;
    item->fall_speed -= 8;
    CLAMPL(item->fall_speed, 0);

    if (item->rot.x >= -M_TURN_RATE && item->rot.x <= M_TURN_RATE) {
        item->rot.x = 0;
    } else if (item->rot.x >= 0) {
        item->rot.x -= M_TURN_RATE;
    } else {
        item->rot.x += M_TURN_RATE;
    }
}

static void M_WaterOut(ITEM *const item, COLL_INFO *const coll)
{
    coll->enable_hit = 0;
    coll->enable_baddie_push = 0;
    g_Camera.flags = CF_FOLLOW_CENTRE;
}

static void M_UWTwist(ITEM *const item, COLL_INFO *const coll)
{
    item->fall_speed = 0;
    item->goal_anim_state = LS(LS_TREAD);
}

// clang-format off
REGISTER_LARA_STATE(LS_TREAD,      M_Tread)
REGISTER_LARA_STATE(LS_SWIM,       M_Swim)
REGISTER_LARA_STATE(LS_GLIDE,      M_Glide)
REGISTER_LARA_STATE(LS_SURF_TREAD, M_TreadSurface)
REGISTER_LARA_STATE(LS_SURF_SWIM,  M_ForwardSurface)
REGISTER_LARA_STATE(LS_DIVE,       M_Dive)
REGISTER_LARA_STATE(LS_UW_DEATH,   M_UWDeath)
REGISTER_LARA_STATE(LS_SURF_BACK,  M_SideBackSurface)
REGISTER_LARA_STATE(LS_SURF_LEFT,  M_SideBackSurface)
REGISTER_LARA_STATE(LS_SURF_RIGHT, M_SideBackSurface)
REGISTER_LARA_STATE(LS_WATER_OUT,  M_WaterOut)
REGISTER_LARA_STATE(LS_WATER_ROLL, M_UWTwist)
// clang-format on
