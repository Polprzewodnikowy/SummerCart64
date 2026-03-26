#include <trx/game/ui/dialogs/pause.h>

#include <trx/game/game_strings/entries.h>
#include <trx/game/ui/elements/anchor.h>
#include <trx/game/ui/elements/frame.h>
#include <trx/game/ui/elements/label.h>
#include <trx/game/ui/elements/modal.h>
#include <trx/game/ui/elements/pad.h>
#include <trx/game/ui/elements/requester.h>

static const GAME_STRING_ID m_Options[2][2] = {
    { GS_ID("general/pause/continue"), GS_ID("general/pause/quit") },
    { GS_ID("general/pause/yes"), GS_ID("general/pause/no") },
};

void UI_Pause_Init(UI_PAUSE_STATE *const s)
{
    s->phase = 0;
    UI_Requester_Init(&s->req, 2, 2, true);
}

void UI_Pause_Free(UI_PAUSE_STATE *const s)
{
    UI_Requester_Free(&s->req);
}

UI_PAUSE_EXIT_CHOICE UI_Pause_Control(UI_PAUSE_STATE *const s)
{
    const int32_t choice = UI_Requester_Control(&s->req);
    if (s->phase == 0) {
        if (choice == UI_REQUESTER_CANCEL) {
            return UI_PAUSE_RESUME_PAUSE;
        } else if (choice == 0) {
            return UI_PAUSE_EXIT_TO_GAME;
        } else if (choice == 1) {
            s->phase = 1;
            UI_Requester_Free(&s->req);
            UI_Requester_Init(&s->req, 2, 2, true);
        }
    } else {
        if (choice == UI_REQUESTER_CANCEL) {
            s->phase = 0;
        } else if (choice == 0) {
            return UI_PAUSE_EXIT_TO_TITLE;
        } else if (choice == 1) {
            return UI_PAUSE_EXIT_TO_GAME;
        }
    }
    return UI_PAUSE_NOOP;
}

void UI_Pause(UI_PAUSE_STATE *const s)
{
    UI_BeginModal(0.5f, 1.0f);
    UI_BeginPad(50.0f, 50.0f);
    UI_BeginRequester(
        &s->req,
        s->phase == 0 ? GS("general/pause/exit_to_title")
                      : GS("general/pause/are_you_sure"));

    for (int32_t i = UI_Requester_GetFirstRow(&s->req);
         i < UI_Requester_GetLastRow(&s->req); i++) {
        UI_BeginRequesterRow(&s->req, i);
        UI_BeginAnchor(0.5f, 0.5f);
        UI_Label(GameString_Get(m_Options[s->phase][i]));
        UI_EndAnchor();
        UI_EndRequesterRow(&s->req, i);
    }

    UI_EndRequester(&s->req);
    UI_EndPad();
    UI_EndModal();
}
