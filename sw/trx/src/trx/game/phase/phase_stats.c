#include <trx/game/phase/phase_stats.h>

#include <trx/config.h>
#include <trx/core/memory.h>
#include <trx/debug.h>
#include <trx/game/fader.h>
#include <trx/game/game.h>
#include <trx/game/game_flow.h>
#include <trx/game/input.h>
#include <trx/game/output.h>
#include <trx/game/shell.h>
#include <trx/game/ui.h>

typedef enum {
    STATE_FADE_IN,
    STATE_DISPLAY,
    STATE_FADE_OUT,
    STATE_FINISH,
} STATE;

typedef struct {
    PHASE_STATS_ARGS args;
    STATE state;
    FADER back_fader;
    FADER top_fader;
    bool ui_active;
    UI_STATS_DIALOG_STATE *ui_state;
} M_PRIV;

static bool M_EnableFade(const M_PRIV *const p)
{
    return g_Config.ui.stats_fade_effects || p->args.show_final_stats;
}

static bool M_IsFading(const M_PRIV *const p)
{
    return M_EnableFade(p)
        && (Fader_IsActive(&p->top_fader) || Fader_IsActive(&p->back_fader));
}

static void M_FadeIn(M_PRIV *const p)
{
    if (p->args.background_path != nullptr) {
        Fader_InitTo(&p->back_fader, 1.0f, 0.0f, 1.0);
    } else {
        Fader_InitTo(&p->back_fader, 0.0f, 1.0f, 0.5);
    }
}

static void M_FadeOut(M_PRIV *const p)
{
    p->state = STATE_FINISH;
}

static PHASE_CONTROL M_Start(PHASE *const phase)
{
    M_PRIV *const p = phase->priv;

    switch (p->args.background_type) {
    case BK_IMAGE:
        if (p->args.background_path == nullptr) {
            LOG_WARNING("Trying to load empty background image");
        } else if (!Output_Overlay_LoadImage(p->args.background_path)) {
            LOG_WARNING(
                "Failed to load background image: %s", p->args.background_path);
        }
        break;

    case BK_NONE:
    case BK_PATTERN_STATIC:
    case BK_PATTERN_WAVE:
    case BK_BLACK:
    case BK_MONOCHROME:
    case BK_MONOCHROME_COOL:
    case BK_MONOCHROME_WARM:
    case BK_TRANSPARENT_MEDIUM:
    case BK_TRANSPARENT_DARK:
        break;
    }

    if (Game_IsInGym()) {
        M_FadeOut(p);
    } else {
        if (p->args.background_type == BK_PATTERN_STATIC
            || p->args.background_type == BK_PATTERN_WAVE) {
            p->state = STATE_DISPLAY;
        } else {
            p->state = STATE_FADE_IN;
            M_FadeIn(p);
        }

        p->ui_active = true;
        p->ui_state = UI_StatsDialog_Init((UI_STATS_DIALOG_ARGS) {
            .mode = p->args.show_final_stats ? UI_STATS_DIALOG_MODE_FINAL
                                             : UI_STATS_DIALOG_MODE_LEVEL,
            .style = p->args.use_bare_style ? UI_STATS_DIALOG_STYLE_BARE
                                            : UI_STATS_DIALOG_STYLE_BORDERED,
            .level_num = p->args.level_num != -1 ? p->args.level_num
                                                 : Game_GetCurrentLevel()->num,
        });
    }

    return (PHASE_CONTROL) { .action = PHASE_ACTION_CONTINUE };
}

static void M_End(PHASE *const phase)
{
    M_PRIV *const p = phase->priv;
    if (p->ui_active) {
        p->ui_active = false;
        UI_StatsDialog_Free(p->ui_state);
        p->ui_state = nullptr;
    }
}

static PHASE_CONTROL M_Control(PHASE *const phase)
{
    M_PRIV *const p = phase->priv;
    Input_Update();
    Shell_ProcessInput();

    switch (p->state) {
    case STATE_FADE_IN:
        if (!M_IsFading(p)) {
            p->state = STATE_DISPLAY;
        } else if (g_InputDB.menu_confirm || g_InputDB.menu_back) {
            M_FadeOut(p);
        }
        break;

    case STATE_DISPLAY:
        if (g_InputDB.menu_confirm || g_InputDB.menu_back) {
            M_FadeOut(p);
        }
        break;

    case STATE_FADE_OUT:
        p->state = STATE_FINISH;
        return (PHASE_CONTROL) { .action = PHASE_ACTION_NO_WAIT };
        break;

    case STATE_FINISH:
        return (PHASE_CONTROL) {
            .action = PHASE_ACTION_END,
            .gf_cmd = { .action = GF_NOOP },
        };
    }

    return (PHASE_CONTROL) { .action = PHASE_ACTION_CONTINUE };
}

static bool M_RequestFadeToBlack(PHASE *const phase, FADER_ARGS *const out_args)
{
    M_PRIV *const p = phase->priv;
    if (!M_EnableFade(p)) {
        return false;
    }

    if (out_args != nullptr) {
        *out_args = (FADER_ARGS) {
            .from_current = false,
            .initial = 0.0f,
            .target = 1.0f,
            .duration = 0.5f,
            .debuff = 0.1f,
        };
    }
    return true;
}

static void M_Draw(PHASE *const phase)
{
    M_PRIV *const p = phase->priv;

    const float top_opacity = M_EnableFade(p)
        ? Fader_GetCurrentValue(&p->top_fader)
        : p->top_fader.args.target;
    if (top_opacity > 0.0f) {
        Output_Overlay_DrawSnapshot(1.0f);
        Output_Overlay_DrawBlackRectangle(top_opacity, false);
        return;
    }

    const float progress = M_EnableFade(p)
        ? Fader_GetCurrentValue(&p->back_fader)
        : p->back_fader.args.target;
    switch (p->args.background_type) {
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

    case BK_IMAGE:
        if (p->args.background_path != nullptr) {
            Output_Overlay_DrawImage(p->args.background_path);
        }
        Output_Overlay_DrawBlackRectangle(progress, false);
        break;

    case BK_PATTERN_STATIC:
    case BK_PATTERN_WAVE:
        Output_Overlay_DrawPattern(p->args.background_type == BK_PATTERN_WAVE);
        Output_Overlay_DrawBlackRectangle(progress, false);
        break;

    default:
        break;
    }

    if (p->ui_active) {
        UI_StatsDialog(p->ui_state);
    }
}

PHASE *Phase_Stats_Create(const PHASE_STATS_ARGS args)
{
    PHASE *const phase = Memory_Alloc(sizeof(PHASE));
    M_PRIV *const p = Memory_Alloc(sizeof(M_PRIV));
    p->args = args;
    p->state = STATE_FADE_IN;
    phase->priv = p;
    phase->start = M_Start;
    phase->end = M_End;
    phase->control = M_Control;
    phase->draw = M_Draw;
    phase->request_fade_to_black = M_RequestFadeToBlack;
    phase->uses_cross_fade_in = nullptr;
    return phase;
}

void Phase_Stats_Destroy(PHASE *const phase)
{
    M_PRIV *const p = phase->priv;
    Memory_Free(p);
    Memory_Free(phase);
}
