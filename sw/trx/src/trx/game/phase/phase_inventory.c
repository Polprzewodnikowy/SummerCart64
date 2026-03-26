#include <trx/game/phase/phase_inventory.h>

#include <trx/config.h>
#include <trx/core/memory.h>
#include <trx/debug.h>
#include <trx/game/fader.h>
#include <trx/game/game_flow.h>
#include <trx/game/inventory_ring.h>
#include <trx/game/music.h>
#include <trx/game/output.h>
#include <trx/game/overlay.h>

typedef struct {
    INVENTORY_MODE mode;
    INV_RING *ring;
    bool fade_to_black;
} M_PRIV;

static PHASE_CONTROL M_Start(PHASE *const phase)
{
    M_PRIV *const p = phase->priv;

    const GF_LEVEL *const level = GF_GetTitleLevel();
    if (p->mode == INV_TITLE_MODE && g_Config.audio.enable_music_in_menu
        && level->music_track >= 0) {
        Music_Stop();
        Music_Play_Direct(level->music_track, MPM_LOOP);
    }

    p->ring = InvRing_Open(p->mode);
    if (p->ring == nullptr) {
        return (PHASE_CONTROL) {
            .action = PHASE_ACTION_END,
            .gf_cmd = { .action = GF_NOOP },
        };
    }
    return (PHASE_CONTROL) { .action = PHASE_ACTION_CONTINUE };
}

static PHASE_CONTROL M_Control(PHASE *const phase)
{
    M_PRIV *const p = phase->priv;
    ASSERT(p->ring != nullptr);
    const GF_COMMAND gf_cmd = InvRing_Control(p->ring);
    if (p->mode == INV_TITLE_MODE && p->ring->status == RNG_DONE) {
        p->fade_to_black = true;
    }
    return (PHASE_CONTROL) {
        .action = (p->mode == INV_GLOBE_SELECT_MODE && gf_cmd.action != GF_NOOP)
                || p->ring->status == RNG_DONE
            ? PHASE_ACTION_END
            : PHASE_ACTION_CONTINUE,
        .gf_cmd = gf_cmd,
    };
}

static bool M_RequestFadeToBlack(PHASE *const phase, FADER_ARGS *const out_args)
{
    const M_PRIV *const p = phase->priv;
    if (p->mode != INV_TITLE_MODE || !p->fade_to_black) {
        return false;
    }

    if (out_args != nullptr) {
        *out_args = (FADER_ARGS) {
            .from_current = false,
            .initial = 0.0f,
            .target = 1.0f,
            .duration = 0.25f,
            .debuff = 0.1f,
        };
    }
    return true;
}

static void M_End(PHASE *const phase)
{
    M_PRIV *const p = phase->priv;
    if (p->mode == INV_TITLE_MODE) {
        Music_Stop();
    }
    if (p->ring != nullptr) {
        InvRing_Close(p->ring);
        p->ring = nullptr;
    }
}

static void M_Draw(PHASE *const phase)
{
    M_PRIV *const p = phase->priv;
    ASSERT(p->ring != nullptr);
    InvRing_Draw(p->ring);
}

PHASE *Phase_Inventory_Create(const INVENTORY_MODE mode)
{
    PHASE *const phase = Memory_Alloc(sizeof(PHASE));
    M_PRIV *const p = Memory_Alloc(sizeof(M_PRIV));
    p->mode = mode;
    p->fade_to_black = false;
    phase->priv = p;
    phase->start = M_Start;
    phase->end = M_End;
    phase->control = M_Control;
    phase->draw = M_Draw;
    phase->request_fade_to_black =
        mode == INV_TITLE_MODE ? M_RequestFadeToBlack : nullptr;
    return phase;
}

void Phase_Inventory_Destroy(PHASE *const phase)
{
    Memory_Free(phase->priv);
    Memory_Free(phase);
}
