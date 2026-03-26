#include <trx/game/ui/dialogs/select_level.h>

#include <trx/core/memory.h>
#include <trx/core/vector.h>
#include <trx/debug.h>
#include <trx/game/game_flow.h>
#include <trx/game/game_strings/entries.h>
#include <trx/game/inventory.h>
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

typedef struct {
    const char *const text;
    const GF_LEVEL *const level;
} M_ROW;

typedef struct UI_SELECT_LEVEL_DIALOG_STATE {
    SAVEGAME_SLOT_REF save_slot;
    VECTOR *rows;
    UI_REQUESTER_STATE req;
} UI_SELECT_LEVEL_DIALOG_STATE;

UI_SELECT_LEVEL_DIALOG_STATE *UI_SelectLevelDialog_Init(
    const SAVEGAME_SLOT_REF save_slot)
{
    UI_SELECT_LEVEL_DIALOG_STATE *const s =
        Memory_Alloc(sizeof(UI_SELECT_LEVEL_DIALOG_STATE));
    s->save_slot = save_slot;
    s->rows = Vector_Create(sizeof(M_ROW));

    const SAVEGAME_INFO *const info = Savegame_GetSavegameInfo(save_slot);
    ASSERT(info != nullptr);
    ASSERT(info->features.select_level);

    Savegame_LoadOnlyResumeInfo(save_slot);
    const GF_LEVEL_TABLE *const level_table = GF_GetLevelTable(GFLT_MAIN);
    for (int32_t i = 0; i <= info->level_num && i < level_table->count; i++) {
        const GF_LEVEL *const level = &level_table->levels[i];
        const RESUME_INFO *const resume = Savegame_GetCurrentInfo(level);
        if (resume != nullptr && resume->flags.available
            && level->type != GFL_GYM) {
            Vector_Add(
                s->rows,
                &(M_ROW) {
                    .text = level_table->levels[i].title,
                    .level = level,
                });
        }
    }

    UI_BasePassportDialog_Init(&s->req, s->rows->count);
    return s;
}

void UI_SelectLevelDialog_Free(UI_SELECT_LEVEL_DIALOG_STATE *const s)
{
    Vector_Free(s->rows);
    UI_Requester_Free(&s->req);
    Memory_Free(s);
}

int32_t UI_SelectLevelDialog_Control(UI_SELECT_LEVEL_DIALOG_STATE *const s)
{
    UI_BasePassportDialog_Control(&s->req);
    const int32_t choice = UI_Requester_Control(&s->req);
    if (choice == UI_REQUESTER_NO_CHOICE || choice == UI_REQUESTER_CANCEL) {
        return choice;
    }
    const M_ROW *const row = Vector_Get(s->rows, choice);
    return row->level->num;
}

void UI_SelectLevelDialog(UI_SELECT_LEVEL_DIALOG_STATE *const s)
{
    UI_BeginBasePassportDialog();
    UI_BeginRequester(&s->req, GS("general/passport/select_level"));

    const SAVEGAME_INFO *info = Savegame_GetSavegameInfo(s->save_slot);
    for (int32_t i = 0; i < s->rows->count; i++) {
        if (UI_Requester_IsRowVisible(&s->req, i)) {
            const M_ROW *const row = Vector_Get(s->rows, i);
            UI_BeginRequesterRow(&s->req, i);
            UI_BeginAnchor(0.5f, 0.5f);
            UI_Label(row->text);
            UI_EndAnchor();
            UI_EndRequesterRow(&s->req, i);
        }
    }

    UI_EndRequester(&s->req);
    UI_EndBasePassportDialog();
}
