#include <trx/game/phase/phase_game.h>

#include <trx/core/memory.h>
#include <trx/game/game.h>
#include <trx/game/lua/events.h>
#include <trx/game/output.h>
#include <trx/game/sound.h>

typedef struct {
    const GF_LEVEL *level;
    GF_SEQUENCE_CONTEXT seq_ctx;
    struct {
        uint8_t reverb_type;
    } stashed_state;
} M_PRIV;

static PHASE_CONTROL M_Start(PHASE *const phase)
{
    M_PRIV *const p = phase->priv;
    if (!Game_Start(p->level, p->seq_ctx)) {
        return (PHASE_CONTROL) {
            .action = PHASE_ACTION_END,
            .gf_cmd = { .action = GF_EXIT_TO_TITLE },
        };
    }
    Game_SetIsPlaying(true);
    return (PHASE_CONTROL) {
        .action = PHASE_ACTION_CONTINUE,
    };
}

static void M_End(PHASE *const phase)
{
    Game_End();
    Game_SetIsPlaying(false);
    Sound_SetReverbType(0);
}

static void M_Suspend(PHASE *const phase)
{
    Game_SetIsPlaying(false);
    M_PRIV *const p = phase->priv;
    p->stashed_state.reverb_type = Sound_GetReverbType();
    Sound_SetReverbType(0);
}

static void M_Resume(PHASE *const phase)
{
    Game_SetIsPlaying(true);
    M_PRIV *const p = phase->priv;
    Sound_SetReverbType(p->stashed_state.reverb_type);
}

static PHASE_CONTROL M_Control(PHASE *const phase)
{
    Lua_FireEventInt32(LUA_EVENT_BEFORE_CONTROL, 0);
    const GF_COMMAND gf_cmd = Game_Control(false);
    Lua_FireEventInt32(LUA_EVENT_AFTER_CONTROL, 0);
    if (gf_cmd.action != GF_NOOP) {
        return (PHASE_CONTROL) {
            .action = PHASE_ACTION_END,
            .gf_cmd = gf_cmd,
        };
    }
    return (PHASE_CONTROL) { .action = PHASE_ACTION_CONTINUE };
}

static void M_Draw(PHASE *const phase)
{
    Game_Draw(true);
}

PHASE *Phase_Game_Create(
    const GF_LEVEL *const level, const GF_SEQUENCE_CONTEXT seq_ctx)
{
    PHASE *const phase = Memory_Alloc(sizeof(PHASE));
    M_PRIV *const p = Memory_Alloc(sizeof(M_PRIV));
    p->level = level;
    p->seq_ctx = seq_ctx;
    phase->priv = p;
    phase->start = M_Start;
    phase->end = M_End;
    phase->suspend = M_Suspend;
    phase->resume = M_Resume;
    phase->control = M_Control;
    phase->draw = M_Draw;
    return phase;
}

void Phase_Game_Destroy(PHASE *const phase)
{
    M_PRIV *const p = phase->priv;
    Memory_Free(p);
    Memory_Free(phase);
}
