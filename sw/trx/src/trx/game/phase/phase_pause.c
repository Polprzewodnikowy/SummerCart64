#include <trx/game/phase/phase_pause.h>

#include <trx/config.h>
#include <trx/core/memory.h>
#include <trx/game/const.h>
#include <trx/game/fader.h>
#include <trx/game/game_strings/entries.h>
#include <trx/game/input.h>
#include <trx/game/music.h>
#include <trx/game/output.h>
#include <trx/game/overlay.h>
#include <trx/game/shell.h>
#include <trx/game/sound.h>
#include <trx/game/ui.h>

#include <stdint.h>

#define M_FADE_TIME 0.4

typedef enum {
    STATE_FADE_IN,
    STATE_WAIT,
    STATE_ASK,
    STATE_FADE_OUT,
} STATE;

typedef struct {
    STATE state;
    struct {
        bool is_ready;
        UI_PAUSE_STATE state;
    } ui;
    GF_ACTION action;
    FADER fader;
} M_PRIV;

static void M_RemoveText(M_PRIV *const p)
{
    Overlay_SetBottomText((OVERLAY_TEXT) { 0 });
}

static void M_FadeIn(M_PRIV *const p)
{
    p->state = STATE_FADE_IN;
    Fader_InitTo(&p->fader, 0.0f, 1.0f, M_FADE_TIME);
}

static void M_FadeOut(M_PRIV *const p)
{
    M_RemoveText(p);
    p->ui.is_ready = false;
    if (p->action == GF_NOOP) {
        Fader_InitFromCurrent(&p->fader, 0.0f, M_FADE_TIME);
    } else {
        Fader_InitFromCurrentHold(
            &p->fader, 1.0f, M_FADE_TIME, 3.0 / (double)LOGIC_FPS);
    }
    p->state = STATE_FADE_OUT;
}

static void M_PauseGame(M_PRIV *const p)
{
    p->action = GF_NOOP;
    Music_Pause();
    Sound_PauseAll();
    M_FadeIn(p);
}

static void M_ReturnToGame(M_PRIV *const p)
{
    Music_Unpause();
    Sound_UnpauseAll();
    M_FadeOut(p);
}

static void M_ExitToTitle(M_PRIV *const p)
{
    p->action = GF_EXIT_TO_TITLE;
    M_FadeOut(p);
}

static void M_CreateText(M_PRIV *const p)
{
    Overlay_SetBottomText((OVERLAY_TEXT) {
        .kind = UI_OVERLAY_TEXT_GS_KEY,
        .gs_key = GS_ID("general/pause/paused"),
    });
}

static PHASE_CONTROL M_Start(PHASE *const phase)
{
    M_PRIV *const p = phase->priv;

    p->ui.is_ready = false;
    UI_Pause_Init(&p->ui.state);
    M_PauseGame(p);
    return (PHASE_CONTROL) { .action = PHASE_ACTION_CONTINUE };
}

static void M_End(PHASE *const phase)
{
    M_PRIV *const p = phase->priv;
    M_RemoveText(p);
    UI_Pause_Free(&p->ui.state);
}

static bool M_IsFadeActive(M_PRIV *const p)
{
    return Fader_IsActive(&p->fader) && g_Config.ui.pause_fade_effects
        && g_Config.ui.pause_background_style != BK_NONE;
}

static PHASE_CONTROL M_Control(PHASE *const phase)
{
    M_PRIV *const p = phase->priv;
    Input_Update();
    Shell_ProcessInput();

    if (p->ui.is_ready) {
        UI_Pause_Control(&p->ui.state);
    }

    switch (p->state) {
    case STATE_FADE_IN:
        if (g_InputDB.pause) {
            M_ReturnToGame(p);
            return (PHASE_CONTROL) { .action = PHASE_ACTION_NO_WAIT };
        } else if (!M_IsFadeActive(p)) {
            p->state = STATE_WAIT;
            M_CreateText(p);
            return (PHASE_CONTROL) { .action = PHASE_ACTION_NO_WAIT };
        }
        break;

    case STATE_WAIT:
        if (g_InputDB.pause) {
            M_ReturnToGame(p);
            return (PHASE_CONTROL) { .action = PHASE_ACTION_NO_WAIT };
        } else if (g_InputDB.option) {
            p->state = STATE_ASK;
        }
        break;

    case STATE_ASK: {
        const UI_PAUSE_EXIT_CHOICE choice = UI_Pause_Control(&p->ui.state);
        switch (choice) {
        case UI_PAUSE_RESUME_PAUSE:
            p->state = STATE_WAIT;
            return (PHASE_CONTROL) { .action = PHASE_ACTION_NO_WAIT };
        case UI_PAUSE_EXIT_TO_GAME:
            M_ReturnToGame(p);
            return (PHASE_CONTROL) { .action = PHASE_ACTION_NO_WAIT };
        case UI_PAUSE_EXIT_TO_TITLE:
            M_ExitToTitle(p);
            return (PHASE_CONTROL) { .action = PHASE_ACTION_NO_WAIT };
        default:
            break;
        }
        break;
    }

    case STATE_FADE_OUT:
        if (!M_IsFadeActive(p)) {
            return (PHASE_CONTROL) {
                .action = PHASE_ACTION_END,
                .gf_cmd = { .action = p->action },
            };
        }
        break;
    }

    return (PHASE_CONTROL) { .action = PHASE_ACTION_CONTINUE };
}

static void M_Draw(PHASE *const phase)
{
    M_PRIV *const p = phase->priv;

    const float progress = g_Config.ui.pause_fade_effects
        ? Fader_GetCurrentValue(&p->fader)
        : p->fader.args.target;
    switch (g_Config.ui.pause_background_style) {
    case BK_NONE:
        Output_Overlay_DrawGame();
        break;

    case BK_TRANSPARENT_MEDIUM:
        Output_Overlay_DrawGame();
        Output_Overlay_DrawBlackRectangle(progress * 0.5f, false);
        break;

    case BK_TRANSPARENT_DARK:
        Output_Overlay_DrawGame();
        Output_Overlay_DrawBlackRectangle(progress * 0.8f, false);
        break;

    case BK_BLACK:
        Output_Overlay_DrawGame();
        Output_Overlay_DrawBlackRectangle(progress, false);
        break;

    case BK_MONOCHROME:
        Output_Overlay_DrawGameMono(progress);
        break;

    case BK_MONOCHROME_COOL:
        Output_Overlay_DrawGameMonoCool(progress);
        break;

    case BK_MONOCHROME_WARM:
        Output_Overlay_DrawGameMonoWarm(progress);
        break;

    case BK_PATTERN_STATIC:
    case BK_PATTERN_WAVE:
        if (progress < 1.0f) {
            Output_Overlay_DrawGame();
        }
        Output_Overlay_DrawPatternOpacity(
            g_Config.ui.pause_background_style == BK_PATTERN_WAVE, progress);
        break;

    case BK_IMAGE:
    default:
        Output_Overlay_DrawGame();
        Output_Overlay_DrawBlackRectangle(progress * 0.8f, false);
        break;
    }

    if (p->state == STATE_ASK) {
        UI_Pause(&p->ui.state);
    }
}

PHASE *Phase_Pause_Create(void)
{
    PHASE *const phase = Memory_Alloc(sizeof(PHASE));
    phase->priv = Memory_Alloc(sizeof(M_PRIV));
    phase->start = M_Start;
    phase->end = M_End;
    phase->control = M_Control;
    phase->draw = M_Draw;
    return phase;
}

void Phase_Pause_Destroy(PHASE *phase)
{
    Memory_Free(phase->priv);
    Memory_Free(phase);
}
