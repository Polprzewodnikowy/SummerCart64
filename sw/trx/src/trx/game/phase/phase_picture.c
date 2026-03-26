#include <trx/game/phase/phase_picture.h>

#include <trx/core/memory.h>
#include <trx/game/fader.h>
#include <trx/game/input.h>
#include <trx/game/output.h>
#include <trx/game/shell.h>

typedef enum {
    STATE_FADE_IN,
    STATE_DISPLAY,
    STATE_FADE_OUT,
} M_STATE;

typedef struct {
    M_STATE state;
    FADER fader;
    CLOCK_TIMER timer;
    PHASE_PICTURE_ARGS args;
    bool has_drawn;
} M_PRIV;

static bool M_UsesCrossFadeIn(PHASE *const phase)
{
    const M_PRIV *const p = phase->priv;
    return p->args.loading_pic && !p->args.block_cross_fade_in;
}

static void M_FadeOut(M_PRIV *const p)
{
    p->state = STATE_FADE_OUT;
    Fader_InitFromCurrentHold(&p->fader, 1.0f, p->args.fade_out_time, 0.1f);
}

static PHASE_CONTROL M_Start(PHASE *const phase)
{
    M_PRIV *const p = phase->priv;
    if (!Output_Overlay_LoadImage(p->args.file_name)) {
        return (PHASE_CONTROL) {
            .action = PHASE_ACTION_END,
            .gf_cmd = { .action = GF_NOOP },
        };
    }

    if (p->args.loading_pic && !p->args.block_cross_fade_in) {
        Output_Overlay_CaptureSnapshot();
    }
    Fader_InitTo(&p->fader, 1.0f, 0.0f, p->args.fade_in_time);
    ClockTimer_Sync(&p->timer);
    return (PHASE_CONTROL) {};
}

static void M_End(PHASE *const phase)
{
    M_PRIV *const p = phase->priv;
}

static PHASE_CONTROL M_Control(PHASE *const phase)
{
    M_PRIV *const p = phase->priv;
    Input_Update();
    Shell_ProcessInput();

    switch (p->state) {
    case STATE_FADE_IN:
        if (g_InputDB.menu_confirm || g_InputDB.menu_back
            || g_InputDB.menu_skip) {
            M_FadeOut(p);
        } else if (!Fader_IsActive(&p->fader)) {
            p->state = STATE_DISPLAY;
            ClockTimer_Sync(&p->timer);
        }
        break;

    case STATE_DISPLAY:
        if (g_InputDB.menu_confirm || g_InputDB.menu_back || g_InputDB.menu_skip
            || ClockTimer_CheckElapsed(
                &p->timer,
                p->args.display_time
                    - (p->args.display_time_includes_fades
                           ? p->args.fade_in_time + p->args.fade_out_time
                           : 0.0))) {
            M_FadeOut(p);
        }
        break;

    case STATE_FADE_OUT:
        if (p->args.loading_pic && p->has_drawn) {
            Output_Overlay_BeginTransitionFadeOut(
                p->args.fade_out_time, 1.0f - Fader_GetCurrentValue(&p->fader));
            return (PHASE_CONTROL) {
                .action = PHASE_ACTION_END,
                .gf_cmd = { .action = GF_NOOP },
            };
        }

        if (g_InputDB.menu_confirm || g_InputDB.menu_back || g_InputDB.menu_skip
            || !Fader_IsActive(&p->fader)) {
            return (PHASE_CONTROL) {
                .action = PHASE_ACTION_END,
                .gf_cmd = { .action = GF_NOOP },
            };
        }
        break;
    }

    return (PHASE_CONTROL) {};
}

static void M_Draw(PHASE *const phase)
{
    M_PRIV *const p = phase->priv;
    const float progress = Fader_GetCurrentValue(&p->fader);
    if (p->args.loading_pic
        && (p->state != STATE_FADE_IN || !p->args.block_cross_fade_in)) {
        Output_Overlay_DrawImage(p->args.file_name);
        if (p->state == STATE_FADE_IN) {
            Output_Overlay_DrawSnapshot(progress);
        }
    } else {
        Output_Overlay_DrawImage(p->args.file_name);
        Output_Overlay_DrawBlackRectangle(progress, false);
    }
    p->has_drawn = true;
}

PHASE *Phase_Picture_Create(const PHASE_PICTURE_ARGS args)
{
    PHASE *const phase = Memory_Alloc(sizeof(PHASE));
    M_PRIV *const p = Memory_Alloc(sizeof(M_PRIV));
    p->args = args;
    p->state = STATE_FADE_IN;
    p->has_drawn = false;
    phase->priv = p;
    phase->start = M_Start;
    phase->end = M_End;
    phase->control = M_Control;
    phase->draw = M_Draw;
    phase->request_fade_to_black = nullptr;
    phase->uses_cross_fade_in = M_UsesCrossFadeIn;
    return phase;
}

void Phase_Picture_Destroy(PHASE *const phase)
{
    M_PRIV *const p = phase->priv;
    Memory_Free(p);
    Memory_Free(phase);
}
