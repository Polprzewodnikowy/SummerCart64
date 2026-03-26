#include <trx/game/ui/dialogs/photo_mode.h>

#include <trx/config.h>
#include <trx/game/game_strings/entries.h>
#include <trx/game/input.h>
#include <trx/game/lara/pose.h>
#include <trx/game/output/draw.h>
#include <trx/game/ui/elements/frame.h>
#include <trx/game/ui/elements/label.h>
#include <trx/game/ui/elements/modal.h>
#include <trx/game/ui/elements/pad.h>
#include <trx/game/ui/elements/spacer.h>
#include <trx/game/ui/elements/stack.h>
#include <trx/game/ui/scaler.h>

#include <stdio.h>

static bool M_HasIcon(const INPUT_ROLE role)
{
    return Input_GetKeyName(
               g_Config.input.backend,
               g_Config.input.layout[g_Config.input.backend], role)
        != nullptr;
}

static void M_Title(const PHOTO_MODE current_mode)
{
    UI_BeginStackEx((UI_STACK_SETTINGS) {
        .orientation = UI_STACK_HORIZONTAL,
        .align = { .h = UI_STACK_H_ALIGN_DISTRIBUTE },
        .spacing = { .v = 8.0f },
    });
    switch (current_mode) {
    case PHOTO_MODE_CAMERA:
        UI_Label(GS("general/photo_mode/title_camera_pos"));
        break;
    case PHOTO_MODE_LARA_POS:
        UI_Label(GS("general/photo_mode/title_lara_pos"));
        break;
    }
    UI_Label("\\{input step_left}\\{input step_right}");
    UI_EndStack();
}

static void M_Inputs(const PHOTO_MODE current_mode)
{
    UI_Label(
        "\\{input camera_up}\\{input camera_down}"
        "\\{input camera_forward}\\{input camera_back}"
        "\\{input camera_left}\\{input camera_right}");
    UI_Label(
        "\\{input left}\\{input forward}"
        "\\{input back}\\{input right}");
    UI_Label("\\{input slow}+\\{input camera_up}/\\{input camera_down}");
    UI_Label("\\{input roll}");
    UI_Label("\\{input look}");

    UI_Label("[\\{input slow}+]\\{input draw}");
    if (Lara_Pose_IsAvailable()) {
        UI_Label("[\\{input slow}+]\\{input fly_cheat}");
    }
    UI_Label("[\\{input slow}+]\\{input pause}");
    UI_Label("\\{input toggle_ui}");
    UI_Label("\\{input action}");

    if (M_HasIcon(INPUT_ROLE_TOGGLE_PHOTO_MODE)
        && M_HasIcon(INPUT_ROLE_INVENTORY)) {
        UI_Label("\\{input toggle_photo_mode}/\\{input option}");
    } else if (M_HasIcon(INPUT_ROLE_TOGGLE_PHOTO_MODE)) {
        UI_Label("\\{input toggle_photo_mode}");
    } else if (M_HasIcon(INPUT_ROLE_INVENTORY)) {
        UI_Label("\\{input option}");
    }
}

static void M_Actions(const PHOTO_MODE current_mode)
{
    switch (current_mode) {
    case PHOTO_MODE_CAMERA:
        UI_Label(GS("general/photo_mode/camera_move_prompt"));
        UI_Label(GS("general/photo_mode/camera_rotate_prompt"));
        UI_Label(GS("general/photo_mode/camera_roll_prompt"));
        UI_Label(GS("general/photo_mode/camera_rotate_90_prompt"));
        UI_Label(GS("general/photo_mode/camera_reset_prompt"));
        break;
    case PHOTO_MODE_LARA_POS:
        UI_Label(GS("general/photo_mode/lara_move_prompt"));
        UI_Label(GS("general/photo_mode/lara_rotate_prompt"));
        UI_Label(GS("general/photo_mode/lara_roll_prompt"));
        UI_Label(GS("general/photo_mode/lara_rotate_90_prompt"));
        UI_Label(GS("general/photo_mode/lara_reset_prompt"));
        break;
    }
    UI_Label(GS("general/photo_mode/fov_prompt"));
    if (Lara_Pose_IsAvailable()) {
        UI_Label(GS("general/photo_mode/change_lara_pose"));
    }
    UI_Label(GS("general/photo_mode/advance_frame"));
    UI_Label(GS("general/photo_mode/toggle_help"));
    UI_Label(GS("general/photo_mode/snap_prompt"));
    UI_Label(GS("general/misc/exit"));
}

void UI_PhotoMode(const PHOTO_MODE current_mode)
{
    const int32_t frame_thickness =
        (int32_t)(UI_Scaler_Calc(4.0f, UI_SCALER_TARGET_TEXT) + 0.5f);
    Output_DrawPhotoModeFrame(frame_thickness);

    if (!g_Config.ui.enable_photo_mode_ui) {
        return;
    }

    UI_BeginModal(0.0f, 0.0f);
    UI_BeginPad(8.0f, 8.0f);
    UI_BeginFrame(UI_FRAME_DIALOG_BACKGROUND);
    UI_BeginPad(8.0, 6.0);

    UI_BeginStackEx((UI_STACK_SETTINGS) {
        .orientation = UI_STACK_VERTICAL,
        .align = { .h = UI_STACK_H_ALIGN_SPAN },
        .spacing = { .v = 8.0f },
    });

    M_Title(current_mode);

    UI_BeginStackEx((UI_STACK_SETTINGS) {
        .orientation = UI_STACK_HORIZONTAL,
        .spacing = { .h = 8.0f },
    });

    // Inputs column
    UI_BeginStack(UI_STACK_VERTICAL);
    M_Inputs(current_mode);
    UI_EndStack();
    UI_BeginStack(UI_STACK_VERTICAL);
    M_Actions(current_mode);
    UI_EndStack();

    UI_EndStack();

    UI_EndStack();
    UI_EndPad();
    UI_EndFrame();
    UI_EndPad();
    UI_EndModal();
}
