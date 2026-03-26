#include <trx/game/photo_mode.h>

#include <trx/config.h>
#include <trx/core/math.h>
#include <trx/core/memory.h>
#include <trx/game/camera.h>
#include <trx/game/collision.h>
#include <trx/game/console/common.h>
#include <trx/game/const.h>
#include <trx/game/game.h>
#include <trx/game/game_strings/entries.h>
#include <trx/game/input.h>
#include <trx/game/interpolation.h>
#include <trx/game/lara/common.h>
#include <trx/game/lara/hair.h>
#include <trx/game/lara/pose.h>
#include <trx/game/matrix.h>
#include <trx/game/music.h>
#include <trx/game/output.h>
#include <trx/game/overlay.h>
#include <trx/game/phase/executor.h>
#include <trx/game/shell.h>
#include <trx/game/sound.h>
#include <trx/game/ui.h>

#define M_INTERPOLATION_STEP 0.25

typedef struct {
    PHOTO_MODE current_mode;
    bool show_fps_counter;
    double rate;
    bool lara_pos_touched;
    XYZ_32 orig_lara_pos;
    XYZ_16 orig_lara_rot;
} M_PRIV;

static M_PRIV m_Priv = {};

static void M_ApplyInterpolation(void)
{
    Interpolation_CommitLara();
    Lara_Hair_Control(true);
    Interpolation_CommitBraid();
}

static void M_RememberLaraPos(M_PRIV *const p)
{
    const ITEM *const lara_item = Lara_GetItem();
    if (lara_item != nullptr) {
        p->orig_lara_pos = lara_item->pos;
        p->orig_lara_rot = lara_item->rot;
    }
}

static void M_RestoreLaraPos(M_PRIV *const p)
{
    ITEM *const lara_item = Lara_GetItem();
    if (lara_item != nullptr) {
        lara_item->pos = p->orig_lara_pos;
        lara_item->rot = p->orig_lara_rot;
    }
}

static PHASE_CONTROL M_AdvanceFrame(M_PRIV *const p)
{
    PHASE_CONTROL result = (PHASE_CONTROL) { .action = PHASE_ACTION_CONTINUE };
    PHASE *const phase = PhaseExecutor_GetOuterPhase();
    if (phase == nullptr || phase->control == nullptr
        || phase->draw == nullptr) {
        return result;
    }

    XYZ_32 prev_lara_pos;
    XYZ_16 prev_lara_rot;
    ITEM *const lara_item = Lara_GetItem();
    if (lara_item != nullptr) {
        prev_lara_pos = lara_item->pos;
        prev_lara_rot = lara_item->rot;
    }
    M_RestoreLaraPos(p);

    Camera_PhotoMode_Pause();
    const bool is_enabled = Interpolation_IsEnabled();
    const bool slow = g_Input.slow;
    Interpolation_Enable();

    if (phase->resume != nullptr) {
        phase->resume(phase);
    }
    if (p->rate >= 1.0) {
        InputState_Clear(&g_Input);
        InputState_Clear(&g_InputDB);
        result = phase->control(phase);
        InputState_Clear(&g_Input);
        InputState_Clear(&g_InputDB);
        p->rate = 0.0;
    }
    p->rate += slow ? M_INTERPOLATION_STEP : 1.0f;
    Interpolation_SetRate(p->rate);
    phase->draw(phase);
    if (phase->suspend != nullptr) {
        phase->suspend(phase);
    }
    if (!is_enabled) {
        Interpolation_Disable();
    }

    Camera_PhotoMode_Resume();

    M_RememberLaraPos(p);
    if (lara_item != nullptr && p->lara_pos_touched) {
        lara_item->pos = prev_lara_pos;
        lara_item->rot = prev_lara_rot;
        Lara_Hair_Initialise();
        M_ApplyInterpolation();
    }
    return result;
}

static bool M_HandleItemPositionInputs(ITEM *const item)
{
    const int32_t trans_speed = STEP_L / 10;

    XYZ_32 delta = {};
    if (g_Input.camera_left) {
        delta.x -= trans_speed;
    } else if (g_Input.camera_right) {
        delta.x += trans_speed;
    }
    if (g_Input.camera_forward) {
        delta.z += trans_speed;
    } else if (g_Input.camera_back) {
        delta.z -= trans_speed;
    }
    if (!g_Input.slow && g_Input.camera_up) {
        delta.y -= trans_speed;
    } else if (!g_Input.slow && g_Input.camera_down) {
        delta.y += trans_speed;
    }

    if (delta.x == 0 && delta.y == 0 && delta.z == 0) {
        return false;
    }

    const int32_t pitch = item->rot.x;
    const int32_t cos_p = Math_Cos(pitch);
    const int32_t sin_p = Math_Sin(pitch);
    const int32_t local_y = delta.y - TRIGMULT2(sin_p, delta.z);
    const int32_t local_z = TRIGMULT2(cos_p, delta.z);
    const int32_t local_x = delta.x;

    const int32_t yaw = item->rot.y;
    const int32_t cos_y = Math_Cos(yaw);
    const int32_t sin_y = Math_Sin(yaw);
    const int32_t world_x =
        TRIGMULT2(cos_y, local_x) + TRIGMULT2(sin_y, local_z);
    const int32_t world_z =
        -TRIGMULT2(sin_y, local_x) + TRIGMULT2(cos_y, local_z);

    item->pos.x += world_x;
    item->pos.y += local_y;
    item->pos.z += world_z;
    return true;
}

static bool M_HandleItemRotationInputs(ITEM *const item)
{
    const int32_t rot_speed = DEG_1 * 2;
    XYZ_32 delta = {};
    if (g_Input.left) {
        delta.y = -rot_speed;
    } else if (g_Input.right) {
        delta.y = +rot_speed;
    }
    if (g_Input.forward) {
        delta.x = -rot_speed;
    } else if (g_Input.back) {
        delta.x = +rot_speed;
    }
    if (g_Input.slow && g_Input.camera_up) {
        delta.z = -rot_speed;
    } else if (g_Input.slow && g_Input.camera_down) {
        delta.z = +rot_speed;
    }
    if (g_InputDB.roll) {
        delta.y = DEG_90;
    }
    if (delta.x == 0 && delta.y == 0 && delta.z == 0) {
        return false;
    }

    // Keep the item's root joint anchored while rotating, so off-center
    // animation origins (especially in cutscenes) do not cause huge offsets.
    // Use live item transforms; interpolation may still contain previous-frame
    // values and would make both samples identical.
    const bool was_item_interp_enabled = item->enable_interpolation;
    item->enable_interpolation = false;

    XYZ_32 old_root_pos = {};
    Collide_GetJointAbsPosition(item, &old_root_pos, 0);

    item->rot.x += delta.x;
    item->rot.y += delta.y;
    item->rot.z += delta.z;

    XYZ_32 new_root_pos = {};
    Collide_GetJointAbsPosition(item, &new_root_pos, 0);
    item->enable_interpolation = was_item_interp_enabled;
    item->pos.x += old_root_pos.x - new_root_pos.x;
    item->pos.y += old_root_pos.y - new_root_pos.y;
    item->pos.z += old_root_pos.z - new_root_pos.z;
    return true;
}

static void M_HandleEditLaraMode(M_PRIV *const p)
{
    Camera_PhotoMode_UpdateFOV();

    ITEM *const lara_item = Lara_GetItem();
    if (lara_item == nullptr) {
        return;
    }

    bool changed = false;
    changed |= M_HandleItemPositionInputs(lara_item);
    changed |= M_HandleItemRotationInputs(lara_item);

    if (changed) {
        p->lara_pos_touched = true;
    }

    if (g_InputDB.look) {
        M_RestoreLaraPos(p);
        changed = true;
        p->lara_pos_touched = false;
    }

    if (changed) {
        M_ApplyInterpolation();
    }
}

static void M_HandleCameraMode(M_PRIV *const p)
{
    Camera_PhotoMode_Update();
}

void PhotoMode_Start(void)
{
    M_PRIV *const p = &m_Priv;
    p->show_fps_counter = g_Config.ui.enable_fps_counter;
    p->rate = 1.0;
    p->current_mode = PHOTO_MODE_CAMERA;
    p->lara_pos_touched = false;
    g_Config.ui.enable_fps_counter = false;

    M_RememberLaraPos(p);
    Camera_PhotoMode_Enter();
    Music_Pause();
    Sound_PauseAll();
}

void PhotoMode_End(void)
{
    M_PRIV *const p = &m_Priv;
    Camera_PhotoMode_Exit();
    M_RestoreLaraPos(p);

    g_Config.ui.enable_fps_counter = p->show_fps_counter;
    Music_Unpause();
    Sound_UnpauseAll();
}

PHASE_CONTROL PhotoMode_Control(void)
{
    M_PRIV *const p = &m_Priv;
    Interpolation_Remember();

    if (g_InputDB.pause) {
        return M_AdvanceFrame(p);
    } else if (g_InputDB.toggle_ui) {
        UI_ToggleState(&g_Config.ui.enable_photo_mode_ui);
    } else if (g_InputDB.step_left) {
        if (p->current_mode == 0) {
            p->current_mode = PHOTO_MODE_LAST;
        } else {
            p->current_mode--;
        }
    } else if (g_InputDB.step_right) {
        if (p->current_mode == PHOTO_MODE_LAST) {
            p->current_mode = 0;
        } else {
            p->current_mode++;
        }
    } else if (g_InputDB.fly_cheat) {
        Lara_Pose_Cycle(g_Input.slow ? -1 : 1);
    } else {
        switch (p->current_mode) {
        case PHOTO_MODE_LARA_POS:
            M_HandleEditLaraMode(p);
            break;
        case PHOTO_MODE_CAMERA:
            M_HandleCameraMode(p);
            break;
        }
    }

    return (PHASE_CONTROL) { .action = PHASE_ACTION_CONTINUE };
}

PHOTO_MODE PhotoMode_GetCurrentMode(void)
{
    M_PRIV *const p = &m_Priv;
    return p->current_mode;
}
