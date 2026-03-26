#include <trx/game/ui/dialogs/save_slot.h>

#include <trx/core/memory.h>
#include <trx/core/utils.h>
#include <trx/debug.h>
#include <trx/game/game_strings/entries.h>
#include <trx/game/input.h>
#include <trx/game/savegame.h>
#include <trx/game/ui/common.h>
#include <trx/game/ui/dialogs/base_passport.h>
#include <trx/game/ui/elements/anchor.h>
#include <trx/game/ui/elements/hide.h>
#include <trx/game/ui/elements/label.h>
#include <trx/game/ui/elements/offset.h>
#include <trx/game/ui/elements/requester.h>
#include <trx/game/ui/elements/spacer.h>
#include <trx/game/ui/elements/stack.h>
#include <trx/game/ui/scaler.h>
#include <trx/game/viewport.h>
#include <trx/version.h>

#define M_IMMEDIATE (g_TRVersion >= 2)

typedef struct UI_SAVE_SLOT_DIALOG_STATE {
    UI_SAVE_SLOT_DIALOG_TYPE type;
    SAVEGAME_SLOT_REF *rows;
    int32_t row_count;
    UI_REQUESTER_STATE req;
} UI_SAVE_SLOT_DIALOG_STATE;

static void M_NonEmptySlot(
    const UI_SAVE_SLOT_DIALOG_STATE *const s, const SAVEGAME_SLOT_REF slot,
    const SAVEGAME_INFO *const info)
{
    if (g_TRVersion == 1) {
        UI_BeginAnchor(0.5f, 0.5f);
        UI_BeginStack(UI_STACK_HORIZONTAL);
    } else {
        UI_BeginStackEx((UI_STACK_SETTINGS) {
            .orientation = UI_STACK_HORIZONTAL,
            .align = { .h = UI_STACK_H_ALIGN_DISTRIBUTE },
        });
    }

    // Level title with the save counter
    UI_Label(info->level_title);
    if (info->counter > 0) {
        UI_Spacer(8.0f, 0.0f);
        if (slot.pool == SAVEGAME_SLOT_POOL_QUICK) {
            UI_BeginStackEx((UI_STACK_SETTINGS) {
                .orientation = UI_STACK_HORIZONTAL,
                .align = { .h = UI_STACK_H_ALIGN_RIGHT },
            });
            if (g_TRVersion != 1) {
                UI_Label("QS ");
            }
        }
        UI_LabelFmt("%d", info->counter);
        if (slot.pool == SAVEGAME_SLOT_POOL_QUICK) {
            if (g_TRVersion == 1) {
                UI_Label(" (QS)");
            }
            UI_EndStack();
        }
    }

    UI_EndStack();
    if (g_TRVersion == 1) {
        UI_EndAnchor();
    }
}

static void M_EmptySlot(
    const UI_SAVE_SLOT_DIALOG_STATE *const s, const SAVEGAME_SLOT_REF slot)
{
    UI_BeginAnchor(0.5f, 0.5f);
    if (slot.pool == SAVEGAME_SLOT_POOL_QUICK) {
        UI_LabelFmt(
            "[Q%d] %s", slot.index + 1, GS("general/misc/empty_slot_fmt"));
    } else {
        UI_LabelFmt(GS("general/misc/empty_slot_fmt"), slot.index + 1);
    }
    UI_EndAnchor();
}

static int32_t M_GetTotalSlots(void)
{
    return Savegame_GetSlotCount(SAVEGAME_SLOT_POOL_QUICK)
        + Savegame_GetSlotCount(SAVEGAME_SLOT_POOL_NORMAL);
}

static SAVEGAME_SLOT_REF M_MapRowToSlot(
    const UI_SAVE_SLOT_DIALOG_STATE *const s, const int32_t row)
{
    ASSERT(s != nullptr);
    if (row < 0 || row >= s->row_count || s->rows == nullptr) {
        return Savegame_InvalidSlot();
    }
    return s->rows[row];
}

static void M_BuildRows(UI_SAVE_SLOT_DIALOG_STATE *const s)
{
    const int32_t max_row_count = MAX(M_GetTotalSlots(), 1);
    s->rows = Memory_Alloc(sizeof(SAVEGAME_SLOT_REF) * max_row_count);
    s->row_count = 0;

    const int32_t quick_visual_count = Savegame_GetQuickVisualCount();
    for (int32_t i = 0; i < quick_visual_count; i++) {
        const SAVEGAME_SLOT_REF slot = Savegame_QuickFromVisualIndex(i);
        if (Savegame_IsValidSlotRef(slot)) {
            s->rows[s->row_count++] = slot;
        }
    }

    const int32_t normal_slot_count =
        Savegame_GetSlotCount(SAVEGAME_SLOT_POOL_NORMAL);
    for (int32_t i = 0; i < normal_slot_count; i++) {
        s->rows[s->row_count++] = Savegame_NormalSlot(i);
    }

    if (s->row_count == 0) {
        s->rows[s->row_count++] = Savegame_InvalidSlot();
    }
}

UI_SAVE_SLOT_DIALOG_STATE *UI_SaveSlotDialog_Init(
    const UI_SAVE_SLOT_DIALOG_TYPE type, const SAVEGAME_SLOT_REF initial_slot)
{
    UI_SAVE_SLOT_DIALOG_STATE *const s =
        Memory_Alloc(sizeof(UI_SAVE_SLOT_DIALOG_STATE));
    s->type = type;
    M_BuildRows(s);

    int32_t initial_row = 0;
    if (Savegame_IsValidSlotRef(initial_slot)) {
        for (int32_t i = 0; i < s->row_count; i++) {
            if (s->rows[i].pool == initial_slot.pool
                && s->rows[i].index == initial_slot.index) {
                initial_row = i;
                break;
            }
        }
    }
    UI_BasePassportDialog_Init(&s->req, s->row_count);
    UI_Requester_SelectRow(&s->req, initial_row);
    return s;
}

void UI_SaveSlotDialog_Free(UI_SAVE_SLOT_DIALOG_STATE *const s)
{
    UI_Requester_Free(&s->req);
    Memory_FreePointer(&s->rows);
    Memory_Free(s);
}

UI_SAVE_SLOT_DIALOG_CHOICE UI_SaveSlotDialog_Control(
    UI_SAVE_SLOT_DIALOG_STATE *const s)
{
    UI_BasePassportDialog_Control(&s->req);
    const int32_t sel_row = UI_Requester_GetCurrentRow(&s->req);
    const int32_t choice = UI_Requester_Control(&s->req);
    if (choice == UI_REQUESTER_CANCEL) {
        return (UI_SAVE_SLOT_DIALOG_CHOICE) {
            .action = UI_SAVE_SLOT_DIALOG_CANCEL,
        };
    }
    if (choice != UI_REQUESTER_NO_CHOICE) {
        const SAVEGAME_SLOT_REF slot = M_MapRowToSlot(s, sel_row);
        if (!Savegame_IsValidSlotRef(slot)) {
            return (UI_SAVE_SLOT_DIALOG_CHOICE) {
                .action = UI_SAVE_SLOT_DIALOG_NO_CHOICE,
            };
        }
        const bool is_valid_save_target =
            s->type == UI_SAVE_SLOT_DIALOG_SAVE_GAME
            && slot.pool == SAVEGAME_SLOT_POOL_NORMAL;
        const bool is_valid_load_target =
            s->type == UI_SAVE_SLOT_DIALOG_LOAD_GAME
            && !Savegame_IsSlotFree(slot);
        const bool is_valid_generic_target =
            s->type == UI_SAVE_SLOT_DIALOG_GENERIC
            && !Savegame_IsSlotFree(slot);

        if (is_valid_save_target || is_valid_load_target
            || is_valid_generic_target) {
            return (UI_SAVE_SLOT_DIALOG_CHOICE) {
                .action = UI_SAVE_SLOT_DIALOG_CONFIRM,
                .slot = slot,
            };
        }
    }
    return (UI_SAVE_SLOT_DIALOG_CHOICE) {
        .action = UI_SAVE_SLOT_DIALOG_NO_CHOICE,
    };
}

void UI_SaveSlotDialog(const UI_SAVE_SLOT_DIALOG_STATE *const s)
{
    UI_BeginBasePassportDialog();
    const char *title = nullptr;
    switch (s->type) {
    case UI_SAVE_SLOT_DIALOG_SAVE_GAME:
        title = GS("general/passport/save_game");
        break;
    case UI_SAVE_SLOT_DIALOG_LOAD_GAME:
        title = GS("general/passport/load_game");
        break;
    case UI_SAVE_SLOT_DIALOG_GENERIC:
        title = GS("general/passport/select_save");
        break;
    }
    UI_BeginRequester(&s->req, title);

    const int32_t first = UI_Requester_GetFirstRow(&s->req);
    const int32_t last = UI_Requester_GetLastRow(&s->req);
    for (int32_t i = first; i < last; ++i) {
        UI_BeginRequesterRow(&s->req, i);
        const SAVEGAME_SLOT_REF slot = M_MapRowToSlot(s, i);
        const SAVEGAME_INFO *const info = Savegame_GetSavegameInfo(slot);
        if (Savegame_IsValidSlotRef(slot) && info != nullptr
            && info->level_title != nullptr) {
            M_NonEmptySlot(s, slot, info);
        } else {
            M_EmptySlot(s, slot);
        }
        UI_EndRequesterRow(&s->req, i);
    }

    UI_EndRequester(&s->req);
    UI_EndBasePassportDialog();
}
