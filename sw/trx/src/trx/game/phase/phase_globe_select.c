#include <trx/game/phase/phase_globe_select.h>

#include <trx/config.h>
#include <trx/core/memory.h>
#include <trx/debug.h>
#include <trx/game/fader.h>
#include <trx/game/game_flow.h>
#include <trx/game/input.h>
#include <trx/game/inventory_ring.h>
#include <trx/game/output.h>
#include <trx/game/overlay.h>
#include <trx/game/shell.h>

typedef enum {
    STATE_FADE_IN,
    STATE_DISPLAY,
    STATE_FADE_OUT,
    STATE_FINISH,
} STATE;

typedef struct {
    PHASE_GLOBE_SELECT_ARGS args;
    STATE state;
    FADER fader;
    INV_RING *ring;
    GF_COMMAND result;
} M_PRIV;

static bool M_IsFading(const M_PRIV *const p)
{
    return Fader_IsActive(&p->fader);
}

static void M_FadeIn(M_PRIV *const p)
{
    if (p->args.background_path != nullptr) {
        Fader_InitTo(&p->fader, 1.0f, 0.0f, 1.0);
    } else {
        Fader_InitTo(&p->fader, 0.0f, 1.0f, 0.5);
    }
    p->state = STATE_FADE_IN;
}

static void M_FadeOut(M_PRIV *const p)
{
    Output_Overlay_CaptureSnapshot();
    Fader_InitFromCurrentHold(&p->fader, 1.0f, 0.5f, 0.1f);
    p->state = STATE_FADE_OUT;
}

static PHASE_CONTROL M_Start(PHASE *const phase)
{
    M_PRIV *const p = phase->priv;
    p->ring = InvRing_Open(INV_GLOBE_SELECT_MODE);
    if (p->ring == nullptr) {
        return (PHASE_CONTROL) {
            .action = PHASE_ACTION_END,
            .gf_cmd = { .action = GF_EXIT_TO_TITLE },
        };
    }
    M_FadeIn(p);
    return (PHASE_CONTROL) { .action = PHASE_ACTION_CONTINUE };
}

static void M_End(PHASE *const phase)
{
    M_PRIV *const p = phase->priv;
    if (p->ring != nullptr) {
        InvRing_Close(p->ring);
        p->ring = nullptr;
    }

    Overlay_SetBottomText((OVERLAY_TEXT) { 0 });
    Overlay_ShowArrow(UI_OVERLAY_ARROW_BCL, false);
    Overlay_ShowArrow(UI_OVERLAY_ARROW_BCR, false);
}

static PHASE_CONTROL M_Control(PHASE *const phase)
{
    M_PRIV *const p = phase->priv;

    switch (p->state) {
    case STATE_FADE_IN:
        if (!M_IsFading(p)) {
            p->state = STATE_DISPLAY;
        }
        break;

    case STATE_DISPLAY:
        break;

    case STATE_FADE_OUT:
        if (g_InputDB.menu_confirm || g_InputDB.menu_back || !M_IsFading(p)) {
            p->state = STATE_FINISH;
            return (PHASE_CONTROL) { .action = PHASE_ACTION_NO_WAIT };
        }
        break;

    case STATE_FINISH:
        return (PHASE_CONTROL) {
            .action = PHASE_ACTION_END,
            .gf_cmd = p->result,
        };
    }

    ASSERT(p->ring != nullptr);
    const GF_COMMAND gf_cmd = InvRing_Control(p->ring);
    if (gf_cmd.action != GF_NOOP) {
        p->result = gf_cmd;
        M_FadeOut(p);
    }

    return (PHASE_CONTROL) { .action = PHASE_ACTION_CONTINUE };
}

static void M_Draw(PHASE *const phase)
{
    M_PRIV *const p = phase->priv;
    const float opacity = Fader_GetCurrentValue(&p->fader);

    if (p->args.background_path != nullptr) {
        Output_Overlay_DrawImageMono(p->args.background_path, 1.0f);
        Output_Overlay_DrawBlackRectangle(0.5f, false);
    } else {
        Output_Overlay_DrawBlackRectangle(1.0f, false);
    }
    Output_Flush();

    ASSERT(p->ring != nullptr);
    InvRing_Draw(p->ring);

    if (opacity > 0.0f) {
        Output_Overlay_DrawBlackRectangle(opacity, false);
    }
}

PHASE *Phase_GlobeSelect_Create(const PHASE_GLOBE_SELECT_ARGS args)
{
    PHASE *const phase = Memory_Alloc(sizeof(*phase));
    M_PRIV *const p = Memory_Alloc(sizeof(*p));
    p->result = (GF_COMMAND) { .action = GF_EXIT_TO_TITLE };
    p->args = args;
    phase->priv = p;
    phase->start = M_Start;
    phase->end = M_End;
    phase->control = M_Control;
    phase->draw = M_Draw;
    return phase;
}

void Phase_GlobeSelect_Destroy(PHASE *const phase)
{
    M_PRIV *const p = phase->priv;
    Memory_Free(p);
    Memory_Free(phase);
}
