#include <trx/game/phase/phase_cutscene.h>

#include <trx/core/memory.h>
#include <trx/game/cutscene.h>
#include <trx/game/game.h>
#include <trx/game/lara/common.h>
#include <trx/game/lua/events.h>
#include <trx/game/output.h>

typedef struct {
    int32_t level_num;
} M_PRIV;

static PHASE_CONTROL M_Start(PHASE *const phase)
{
    M_PRIV *const p = phase->priv;
    if (!Cutscene_Start(p->level_num)) {
        return (PHASE_CONTROL) {
            .action = PHASE_ACTION_END,
            .gf_cmd = { .action = GF_NOOP },
        };
    }
    Game_SetIsPlaying(true);
    Lara_SetControllable(false);
    return (PHASE_CONTROL) {};
}

static void M_End(PHASE *const phase)
{
    M_PRIV *const p = phase->priv;
    Game_SetIsPlaying(false);
    Cutscene_End();
}

static void M_Suspend(PHASE *const phase)
{
    Game_SetIsPlaying(false);
}

static void M_Resume(PHASE *const phase)
{
    Game_SetIsPlaying(true);
}

static PHASE_CONTROL M_Control(PHASE *const phase)
{
    Lua_FireEventInt32(LUA_EVENT_BEFORE_CONTROL, 0);
    M_PRIV *const p = phase->priv;
    const GF_COMMAND gf_cmd = Cutscene_Control();
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
    M_PRIV *const p = phase->priv;
    Cutscene_Draw();
}

PHASE *Phase_Cutscene_Create(const int32_t level_num)
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

void Phase_Cutscene_Destroy(PHASE *const phase)
{
    M_PRIV *const p = phase->priv;
    Memory_Free(p);
    Memory_Free(phase);
}
