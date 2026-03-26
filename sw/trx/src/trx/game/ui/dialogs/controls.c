#include <trx/game/ui/dialogs/controls.h>

#include <trx/config.h>
#include <trx/game/game_strings/entries.h>
#include <trx/game/input.h>
#include <trx/game/ui/dialogs/controls_backend.h>
#include <trx/game/ui/dialogs/controls_editor.h>
#include <trx/game/ui/elements/requester.h>

typedef enum {
    M_PHASE_BACKEND,
    M_PHASE_EDITOR,
} M_PHASE;

void UI_Controls_Init(UI_CONTROLS_STATE *const s)
{
    s->events = EventManager_Create();
    s->phase = M_PHASE_BACKEND;
    s->backend = INPUT_BACKEND_KEYBOARD;
    s->active_layout = g_Config.input.layout[s->backend];
    UI_ControlsBackend_Init(&s->backend_state);
    for (INPUT_BACKEND backend = 0; backend < INPUT_BACKEND_NUMBER_OF;
         backend++) {
        UI_ControlsEditor_Init(
            &s->editor_state[backend], backend, g_Config.input.layout[backend],
            s->events);
    }
}

void UI_Controls_Free(UI_CONTROLS_STATE *const s)
{
    for (INPUT_BACKEND backend = 0; backend < INPUT_BACKEND_NUMBER_OF;
         backend++) {
        UI_ControlsEditor_Free(&s->editor_state[backend]);
    }
    UI_ControlsBackend_Free(&s->backend_state);
    EventManager_Free(s->events);
    s->events = nullptr;
}

bool UI_Controls_Control(UI_CONTROLS_STATE *const s)
{
    switch (s->phase) {
    case M_PHASE_BACKEND: {
        const int32_t choice = UI_ControlsBackend_Control(&s->backend_state);
        switch (choice) {
        case UI_REQUESTER_NO_CHOICE:
            return false;
        case UI_REQUESTER_CANCEL:
            return true;
        case INPUT_BACKEND_KEYBOARD:
        case INPUT_BACKEND_CONTROLLER:
            s->backend = choice;
            s->phase = M_PHASE_EDITOR;
            g_Config.input.backend = s->backend;
            Config_Update();
            break;
        }
        break;
    }

    case M_PHASE_EDITOR: {
        const UI_CONTROLS_CHOICE choice =
            UI_ControlsEditor_Control(&s->editor_state[s->backend]);
        switch (choice) {
        case UI_CONTROLS_CHOICE_NOOP:
            break;
        case UI_CONTROLS_CHOICE_GO_BACK:
            s->phase = M_PHASE_BACKEND;
            break;
        case UI_CONTROLS_CHOICE_EXIT:
            return true;
        }
        break;
    }
    }

    return false;
}

void UI_Controls(UI_CONTROLS_STATE *const s)
{
    switch (s->phase) {
    case M_PHASE_BACKEND:
        UI_ControlsBackend(&s->backend_state);
        break;
    case M_PHASE_EDITOR:
        UI_ControlsEditor(&s->editor_state[s->backend]);
        break;
    }
}
