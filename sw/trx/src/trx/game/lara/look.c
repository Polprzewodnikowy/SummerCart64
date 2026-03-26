#include <trx/game/lara/look.h>

#include <trx/config.h>
#include <trx/core/utils.h>
#include <trx/game/camera.h>
#include <trx/game/input.h>
#include <trx/game/lara.h>

static const LARA_TRX_STATE m_StopStates[] = {
    LS_STOP,
    LS_SURF_TREAD,
    LS_POSE,
    LS_TRX_INVALID,
};

static const LARA_TRX_STATE m_BlockingStates[] = {
    // clang-format off
    LS_JUMP_RIGHT,
    LS_JUMP_LEFT,
    LS_SPLAT,
    LS_STEP_RIGHT,
    LS_STEP_LEFT,
    LS_PUSH_BLOCK,
    LS_PULL_BLOCK,
    LS_PICKUP,
    LS_FLARE_PICKUP,
    LS_SWITCH_ON,
    LS_SWITCH_OFF,
    LS_USE_KEY,
    LS_USE_PUZZLE,
    LS_NEUTRAL_ROLL,
    LS_TRX_INVALID,
    // clang-format on
};

static const LARA_EXTRA_STATE m_PermittedExtraStates[] = {
    // clang-format off
    LS_EXTRA_BREATH,
    LS_EXTRA_AIRLOCK,
    (LARA_EXTRA_STATE)-1,
    // clang-format on
};

static void M_Reset(void)
{
    if (g_Camera.type == CAM_LOOK) {
        return;
    }

    LARA_INFO *const lara = Lara_GetLaraInfo();
    const CAMERA_LOOK_SETTINGS *const look = Camera_GetLookSettings(false);

    if (lara->head_rot.x <= -look->head_turn
        || lara->head_rot.x >= look->head_turn) {
        lara->head_rot.x -= lara->head_rot.x / 8;
    } else {
        lara->head_rot.x = 0;
    }

    if (lara->head_rot.y <= -look->head_turn
        || lara->head_rot.y >= look->head_turn) {
        lara->head_rot.y += lara->head_rot.y / -8;
    } else {
        lara->head_rot.y = 0;
    }

    lara->torso_rot.x = lara->head_rot.x;
    lara->torso_rot.y = lara->head_rot.y;
}

static bool M_IsLaraIdle(void)
{
    const ITEM *const vehicle = Lara_Vehicle_GetItem();
    if (vehicle != nullptr) {
        return vehicle->speed == 0;
    }
    return Lara_HasState(m_StopStates);
}

static bool M_IsStatePermitted(void)
{
    const ITEM *const lara_item = Lara_GetItem();
    if (lara_item->hit_points <= 0) {
        return false;
    }

    const LARA_INFO *const lara_info = Lara_GetLaraInfo();
    if (lara_info->extra_anim) {
        return g_Config.gameplay.look_mode == LOOK_MODE_UNRESTRICTED
            && Lara_HasExtraState(m_PermittedExtraStates);
    }

    return g_Config.gameplay.look_mode == LOOK_MODE_UNRESTRICTED
        || !Lara_HasState(m_BlockingStates);
}

void Lara_Look_LeftRight(void)
{
    g_Camera.type = CAM_LOOK;

    LARA_INFO *const lara = Lara_GetLaraInfo();
    const CAMERA_LOOK_SETTINGS *const look =
        Camera_GetLookSettings(lara->water_status == LWS_SURFACE);

    if (g_Input.left) {
        g_Input.left = 0;
        if (lara->head_rot.y > look->min_head_rotation) {
            lara->head_rot.y -= look->head_turn;
        }
    } else if (g_Input.right) {
        g_Input.right = 0;
        if (lara->head_rot.y < look->max_head_rotation) {
            lara->head_rot.y += look->head_turn;
        }
    }

    if (lara->gun_status != LGS_HANDS_BUSY && !Lara_Vehicle_IsMounted()) {
        lara->torso_rot.y = lara->head_rot.y * look->torso_head_rot_y;
    }
}

void Lara_Look_UpDown(void)
{
    g_Camera.type = CAM_LOOK;

    if (g_Config.gameplay.enable_inverted_look) {
        bool temp_forward;
        SWAP2(g_Input.forward, g_Input.back, temp_forward);
    }

    LARA_INFO *const lara = Lara_GetLaraInfo();
    const CAMERA_LOOK_SETTINGS *const look =
        Camera_GetLookSettings(lara->water_status == LWS_SURFACE);

    if (g_Input.forward) {
        g_Input.forward = 0;
        if (lara->head_rot.x > look->min_head_tilt) {
            lara->head_rot.x -= look->head_turn;
        }
    } else if (g_Input.back) {
        g_Input.back = 0;
        if (lara->head_rot.x < look->max_head_tilt) {
            lara->head_rot.x += look->head_turn;
        }
    }

    if (lara->gun_status != LGS_HANDS_BUSY) {
        lara->torso_rot.x = lara->head_rot.x * look->torso_head_rot_x;
    }
}

void Lara_Look_Update(void)
{
    if (g_Input.look && g_Config.gameplay.look_mode == LOOK_MODE_RESTRICTED
        && !M_IsLaraIdle()) {
        if (g_Camera.type == CAM_LOOK) {
            g_Camera.type = CAM_CHASE;
        }
        M_Reset();
        return;
    }

    if (g_Input.look && M_IsStatePermitted()) {
        Lara_Look_LeftRight();
    } else {
        M_Reset();
    }
}
