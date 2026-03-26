#include <trx/game/ui/dialogs/play_any_level.h>

#include <trx/core/memory.h>
#include <trx/core/vector.h>
#include <trx/game/game_flow.h>
#include <trx/game/game_strings/entries.h>
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
    int32_t level_num;
    const char *text;
} M_ROW;

typedef struct UI_PLAY_ANY_LEVEL_DIALOG_STATE {
    VECTOR *rows;
    UI_REQUESTER_STATE req;
} UI_PLAY_ANY_LEVEL_DIALOG_STATE;

UI_PLAY_ANY_LEVEL_DIALOG_STATE *UI_PlayAnyLevelDialog_Init(void)
{
    UI_PLAY_ANY_LEVEL_DIALOG_STATE *const s =
        Memory_Alloc(sizeof(UI_PLAY_ANY_LEVEL_DIALOG_STATE));
    s->rows = Vector_Create(sizeof(M_ROW));

    const GF_LEVEL_TABLE *const level_table = GF_GetLevelTable(GFLT_MAIN);
    for (int32_t i = 0; i < level_table->count; i++) {
        const GF_LEVEL *const level = &level_table->levels[i];
        if (level->type != GFL_GYM && level->type != GFL_DUMMY
            && level->type != GFL_CURRENT) {
            const M_ROW row = { .level_num = level->num, .text = level->title };
            Vector_Add(s->rows, &row);
        }
    }

    UI_BasePassportDialog_Init(&s->req, s->rows->count);
    return s;
}

void UI_PlayAnyLevelDialog_Free(UI_PLAY_ANY_LEVEL_DIALOG_STATE *const s)
{
    Vector_Free(s->rows);
    UI_Requester_Free(&s->req);
    Memory_Free(s);
}

int32_t UI_PlayAnyLevelDialog_Control(UI_PLAY_ANY_LEVEL_DIALOG_STATE *const s)
{
    UI_BasePassportDialog_Control(&s->req);
    const int32_t choice = UI_Requester_Control(&s->req);
    switch (choice) {
    case UI_REQUESTER_NO_CHOICE:
        return UI_PLAY_ANY_LEVEL_CHOICE_NO_CHOICE;
    case UI_REQUESTER_CANCEL:
        return UI_PLAY_ANY_LEVEL_CHOICE_CANCEL;
    default:
        return ((M_ROW *)Vector_Get(s->rows, choice))->level_num;
    }
}

void UI_PlayAnyLevelDialog(UI_PLAY_ANY_LEVEL_DIALOG_STATE *const s)
{
    UI_BeginBasePassportDialog();
    UI_BeginRequester(&s->req, GS("general/passport/select_level"));

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
