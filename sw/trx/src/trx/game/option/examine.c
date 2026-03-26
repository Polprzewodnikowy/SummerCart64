#include <trx/game/option/examine.h>

#include <trx/config.h>
#include <trx/core/strings.h>
#include <trx/game/const.h>
#include <trx/game/game_strings/entries.h>
#include <trx/game/input.h>
#include <trx/game/inventory_ring/control.h>
#include <trx/game/matrix.h>
#include <trx/game/objects/names.h>
#include <trx/game/ui.h>
#include <trx/game/ui/scaler.h>
#include <trx/game/viewport.h>

#define M_EXAMINE_ROTATION_SPEED (DEG_1 * 3)

typedef struct {
    OBJECT_ID object_id;
    bool is_dialog_hidden;
    struct {
        bool is_ready;
        UI_TEXT_DIALOG_STATE *state;
    } ui;
} M_PRIV;

static M_PRIV m_Priv = {};

static void M_DrawHideDialogFooter(void *const user_data)
{
    UI_BeginAnchor(0.5f, 0.5f);
    UI_LabelFmt(
        "\\{input look} %s",
        user_data != nullptr ? (const char *)user_data : "");
    UI_EndAnchor();
}

static void M_DrawRotateHint(void *const user_data)
{
    UI_ButtonLabelEx(
        g_Config.input.backend == INPUT_BACKEND_KEYBOARD
            ? GS("general/misc/direction_keys_keyboard")
            : GS("general/misc/direction_keys_controller"),
        GS("general/actions/rotate"));
}

static bool M_ShouldShowDialog(const OBJECT_ID obj_id)
{
    const char *const description = Object_GetDescription(obj_id);
    return !String_IsEmpty(description);
}

static int32_t M_GetMaxRows(void)
{
    const int32_t res_h = UI_Scaler_CalcInverse(
        Viewport_GetHeight(VIEWPORT_UI), UI_SCALER_TARGET_TEXT);
    if (res_h <= 240) {
        return 5;
    } else if (res_h <= 384) {
        return 7;
    } else {
        return 12;
    }
}

static void M_Init(M_PRIV *const p, const OBJECT_ID obj_id)
{
    p->object_id = obj_id;
    p->is_dialog_hidden = false;
    p->ui.is_ready = true;
    p->ui.state = UI_TextDialog_Init(
        UI_GetCanvasWidth() * 2.0 / 3.0f, M_GetMaxRows(), false);
}

static void M_Close(M_PRIV *const p)
{
    InvRing_ClearButtonHint();
    if (p->ui.is_ready) {
        UI_TextDialog_Free(p->ui.state);
        p->ui.state = nullptr;
        p->ui.is_ready = false;
    }
}

static void M_ApplyExamineRotation(INVENTORY_ITEM *const inv_item)
{
    const int32_t yaw_input =
        (g_Input.menu_left ? 1 : 0) - (g_Input.menu_right ? 1 : 0);
    const int32_t pitch_input =
        (g_Input.menu_down ? 1 : 0) - (g_Input.menu_up ? 1 : 0);
    if (yaw_input == 0 && pitch_input == 0) {
        return;
    }
    inv_item->has_manual_rot = true;
    MATRIX delta = g_IDMatrix;
    if (yaw_input != 0) {
        Matrix_RotY_M(&delta, yaw_input * M_EXAMINE_ROTATION_SPEED);
    }
    if (pitch_input != 0) {
        Matrix_RotX_M(&delta, pitch_input * M_EXAMINE_ROTATION_SPEED);
    }
    Matrix_Mul3x3_M(&inv_item->manual_rot, &delta, &inv_item->manual_rot);
}

bool Option_Examine_CanExamine(const OBJECT_ID obj_id)
{
    return Object_GetDescription(obj_id) != nullptr;
}

void Option_Examine_Control(INVENTORY_ITEM *const inv_item, const bool is_busy)
{
    M_PRIV *const p = &m_Priv;
    if (is_busy) {
        return;
    }

    const OBJECT_ID obj_id = inv_item->object_id;
    if (!p->ui.is_ready) {
        M_Init(p, obj_id);
    }

    const bool has_dialog = M_ShouldShowDialog(obj_id);
    const bool show_dialog = has_dialog && !p->is_dialog_hidden;
    if (show_dialog) {
        InvRing_ClearButtonHint();
    } else {
        InvRing_SetButtonHintDrawer(M_DrawRotateHint, nullptr);
    }
    if (show_dialog) {
        UI_TextDialog_Control(p->ui.state);
    }

    if (g_InputDB.look) {
        if (show_dialog) {
            p->is_dialog_hidden = true;
            return;
        } else {
            g_InputDB.menu_back = true;
            g_InputDB.menu_confirm = false;
            inv_item->has_manual_rot = false;
            p->is_dialog_hidden = false;
            M_Close(p);
            return;
        }
    }

    if (g_InputDB.menu_back || g_InputDB.menu_confirm) {
        g_InputDB.menu_back = true;
        g_InputDB.menu_confirm = false;
        inv_item->has_manual_rot = false;
        p->is_dialog_hidden = false;
        M_Close(p);
        return;
    }

    if (!show_dialog) {
        M_ApplyExamineRotation(inv_item);
    }
}

void Option_Examine_Draw(void)
{
    M_PRIV *const p = &m_Priv;
    if (!p->ui.is_ready) {
        return;
    }

    if (M_ShouldShowDialog(p->object_id) && !p->is_dialog_hidden) {
        const char *const footer_label = GS("general/actions/hide_dialog");
        UI_TextDialogEx(
            p->ui.state,
            (UI_TEXT_DIALOG_SETTINGS) {
                .title_raw = Object_GetName(p->object_id),
                .text_raw = Object_GetDescription(p->object_id),
                .footer_func = M_DrawHideDialogFooter,
                .footer_user_data = (void *)footer_label,
            });
    }
}

void Option_Examine_Close(void)
{
    M_PRIV *const p = &m_Priv;
    M_Close(p);
}
