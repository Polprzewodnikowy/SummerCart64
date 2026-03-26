#include <trx/game/phase/phase_demo.h>

#include <trx/core/memory.h>
#include <trx/game/demo.h>
#include <trx/game/fader.h>
#include <trx/game/game.h>
#include <trx/game/interpolation.h>
#include <trx/game/inventory_ring.h>
#include <trx/game/output/overlay.h>
#include <trx/game/shell.h>

typedef enum {
    STATE_RUN,
    STATE_FADE_OUT,
    STATE_FINISH,
} STATE;

typedef struct {
    STATE state;
    int32_t level_num;
    FADER fader;
    GF_COMMAND exit_gf_cmd;
} M_PRIV;

static PHASE_CONTROL M_Start(PHASE *const phase)
{
    M_PRIV *const p = phase->priv;
    if (p->level_num == -1) {
        return (PHASE_CONTROL) {
            .action = PHASE_ACTION_END,
            .gf_cmd = { .action = GF_EXIT_TO_TITLE },
        };
    }

    if (!Demo_Start(p->level_num)) {
        return (PHASE_CONTROL) {
            .action = PHASE_ACTION_END,
            .gf_cmd = { .action = GF_EXIT_TO_TITLE },
        };
    }

    p->state = STATE_RUN;
    Game_SetIsPlaying(true);

    return (PHASE_CONTROL) { .action = PHASE_ACTION_CONTINUE };
}

static void M_End(PHASE *const phase)
{
    Demo_End();
}

static void M_Suspend(PHASE *const phase)
{
    Game_SetIsPlaying(false);
    Demo_Pause();
}

static void M_Resume(PHASE *const phase)
{
    Game_SetIsPlaying(true);
    Demo_Unpause();
}

static PHASE_CONTROL M_Control(PHASE *const phase)
{
    M_PRIV *const p = phase->priv;

    switch (p->state) {
    case STATE_RUN:
        const GF_COMMAND gf_cmd = Demo_Control();
        if (gf_cmd.action != GF_NOOP) {
            p->state = STATE_FADE_OUT;
            p->exit_gf_cmd = gf_cmd;
            Fader_InitToHold(&p->fader, 0.0f, 1.0f, 0.5f, 0.1f);
            return (PHASE_CONTROL) { .action = PHASE_ACTION_NO_WAIT };
        }
        break;

    case STATE_FADE_OUT:
        Game_SetIsPlaying(false);
        Demo_StopFlashing();
        if (!Fader_IsActive(&p->fader)) {
            p->state = STATE_FINISH;
            return (PHASE_CONTROL) { .action = PHASE_ACTION_NO_WAIT };
        }
        break;

    case STATE_FINISH:
        return (PHASE_CONTROL) {
            .action = PHASE_ACTION_END,
            .gf_cmd = Shell_IsExiting()
                ? (GF_COMMAND) { .action = GF_EXIT_GAME }
                : p->exit_gf_cmd,
        };
    }

    return (PHASE_CONTROL) { .action = PHASE_ACTION_CONTINUE };
}

static void M_Draw(PHASE *const phase)
{
    M_PRIV *const p = phase->priv;
    if (p->state == STATE_FADE_OUT) {
        Interpolation_Disable();
    }
    Game_Draw(true);
    if (p->state == STATE_FADE_OUT) {
        Interpolation_Enable();
    }

    Output_Overlay_DrawBlackRectangle(Fader_GetCurrentValue(&p->fader), true);
}

PHASE *Phase_Demo_Create(const int32_t level_num)
{
    PHASE *const phase = Memory_Alloc(sizeof(PHASE));
    M_PRIV *const p = Memory_Alloc(sizeof(M_PRIV));
    p->level_num = level_num;
    phase->priv = p;
    phase->start = M_Start;
    phase->end = M_End;
    phase->suspend = M_Suspend;
    phase->resume = M_Resume;
    phase->control = M_Control;
    phase->draw = M_Draw;
    return phase;
}

void Phase_Demo_Destroy(PHASE *const phase)
{
    M_PRIV *const p = phase->priv;
    Memory_Free(p);
    Memory_Free(phase);
}
