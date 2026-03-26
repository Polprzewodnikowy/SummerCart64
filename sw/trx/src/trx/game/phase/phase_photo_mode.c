#include <trx/game/phase/phase_photo_mode.h>

#include <trx/config.h>
#include <trx/core/memory.h>
#include <trx/game/cutscene.h>
#include <trx/game/game.h>
#include <trx/game/phase/executor.h>
#include <trx/game/photo_mode.h>
#include <trx/game/screenshot.h>
#include <trx/game/shell.h>
#include <trx/game/sound.h>
#include <trx/game/ui.h>

typedef struct {
    bool in_cutscene;
    bool taking_screenshot;
} M_PRIV;

static PHASE_CONTROL M_Start(PHASE *phase)
{
    M_PRIV *const p = phase->priv;
    p->in_cutscene = GF_GetCurrentLevel()->type == GFL_CUTSCENE;
    PhotoMode_Start();
    return (PHASE_CONTROL) { .action = PHASE_ACTION_CONTINUE };
}

static void M_End(PHASE *const phase)
{
    PhotoMode_End();
}

static PHASE_CONTROL M_Control(PHASE *const phase)
{
    M_PRIV *const p = phase->priv;
    Input_Update();
    Shell_ProcessInput();

    // XXX: normally we'd be using menu_back alone to let the player go back
    // and exit the photo mode UI, BUT for controller players, the default
    // menu_back button conflicts with the roll input, as both are bound to the
    // B button. This causes neither to work as expected, when the player
    // presses B. This is a hacky solution since technically the player might
    // remap the roll input to some other button, making the roll check below
    // redundant, but this is the most straightforward approach.
    if (g_InputDB.toggle_photo_mode
        || (g_InputDB.menu_back && !g_InputDB.roll)) {
        return (PHASE_CONTROL) {
            .action = PHASE_ACTION_END,
            .gf_cmd = { .action = GF_NOOP },
        };
    } else if (g_InputDB.action) {
        p->taking_screenshot = true;
        Screenshot_Make(g_Config.rendering.screenshot_format);
        Sound_Effect(SFX_MENU_LARA_HOME, nullptr, SPM_ALWAYS);
    }
    return PhotoMode_Control();
}

static void M_Draw(PHASE *const phase)
{
    M_PRIV *const p = phase->priv;
    if (p->in_cutscene) {
        Cutscene_Draw();
    } else {
        Game_Draw(false);
    }

    if (p->taking_screenshot) {
        p->taking_screenshot = false;
    } else {
        UI_PhotoMode(PhotoMode_GetCurrentMode());
    }
}

PHASE *Phase_PhotoMode_Create(void)
{
    PHASE *const phase = Memory_Alloc(sizeof(PHASE));
    phase->priv = Memory_Alloc(sizeof(M_PRIV));
    phase->start = M_Start;
    phase->end = M_End;
    phase->control = M_Control;
    phase->draw = M_Draw;
    return phase;
}

void Phase_PhotoMode_Destroy(PHASE *phase)
{
    Memory_Free(phase->priv);
    Memory_Free(phase);
}
