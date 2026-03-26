#include <trx/game/ui/dialogs/switch_mod.h>

#include <trx/core/memory.h>
#include <trx/core/strings.h>
#include <trx/core/vector.h>
#include <trx/game/game_strings/entries.h>
#include <trx/game/shell/mod.h>
#include <trx/game/ui/common.h>
#include <trx/game/ui/dialogs/base_passport.h>
#include <trx/game/ui/elements/anchor.h>
#include <trx/game/ui/elements/label.h>
#include <trx/game/ui/elements/requester.h>

typedef struct {
    const char *name;
    const char *title;
} M_ROW;

struct UI_SWITCH_MOD_DIALOG_STATE {
    VECTOR *rows;
    UI_REQUESTER_STATE req;
};

UI_SWITCH_MOD_DIALOG_STATE *UI_SwitchModDialog_Init(void)
{
    UI_SWITCH_MOD_DIALOG_STATE *const s =
        Memory_Alloc(sizeof(UI_SWITCH_MOD_DIALOG_STATE));
    s->rows = Vector_Create(sizeof(M_ROW));

    int32_t current_row = 0;
    for (int32_t i = 0; i < Shell_GetModCount(); i++) {
        const SHELL_MOD *const mod = Shell_GetMod(i);
        if (!Shell_CanSwitchToMod(mod)) {
            continue;
        }
        if (Shell_IsCurrentMod(mod->name)) {
            current_row = s->rows->count;
        }
        Vector_Add(
            s->rows, &(M_ROW) { .name = mod->name, .title = mod->title });
    }

    UI_BasePassportDialog_Init(&s->req, s->rows->count);
    UI_Requester_SelectRow(&s->req, current_row);
    return s;
}

void UI_SwitchModDialog_Free(UI_SWITCH_MOD_DIALOG_STATE *const s)
{
    Vector_Free(s->rows);
    UI_Requester_Free(&s->req);
    Memory_Free(s);
}

int32_t UI_SwitchModDialog_Control(UI_SWITCH_MOD_DIALOG_STATE *const s)
{
    UI_BasePassportDialog_Control(&s->req);
    return UI_Requester_Control(&s->req);
}

const char *UI_SwitchModDialog_GetSelectedMod(
    const UI_SWITCH_MOD_DIALOG_STATE *const s, const int32_t choice)
{
    if (choice < 0 || choice >= s->rows->count) {
        return nullptr;
    }
    const M_ROW *const row = Vector_Get(s->rows, choice);
    return row->name;
}

void UI_SwitchModDialog(UI_SWITCH_MOD_DIALOG_STATE *const s)
{
    UI_BeginBasePassportDialog();
    UI_BeginRequester(&s->req, GS("general/passport/select_mod"));

    for (int32_t i = 0; i < s->rows->count; i++) {
        if (!UI_Requester_IsRowVisible(&s->req, i)) {
            continue;
        }

        const M_ROW *const row = Vector_Get(s->rows, i);
        UI_BeginRequesterRow(&s->req, i);
        UI_BeginAnchor(0.5f, 0.5f);
        const char *const gs_key =
            String_FormatStatic("dynamic/mods/%s/title", row->name);
        const char *const gs_title = GS(gs_key);
        const char *const display = gs_title != nullptr ? gs_title
            : row->title != nullptr                     ? row->title
                                                        : row->name;
        UI_Label(display);
        UI_EndAnchor();
        UI_EndRequesterRow(&s->req, i);
    }

    UI_EndRequester(&s->req);
    UI_EndBasePassportDialog();
}
