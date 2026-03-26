#include <trx/game/option/stats.h>

#include <trx/config.h>
#include <trx/game/game.h>
#include <trx/game/game_flow.h>
#include <trx/game/gym.h>
#include <trx/game/input.h>
#include <trx/game/lara.h>
#include <trx/game/sound.h>
#include <trx/game/ui.h>

typedef struct {
    bool is_ready;
    UI_STATS_DIALOG_STATE *ui_state;
} M_PRIV;

static M_PRIV m_Priv = {};
static int16_t m_CompassNeedle = 0;
static int16_t m_CompassSpeed = 0;

static void M_Init(M_PRIV *const p, INVENTORY_ITEM *const inv_item)
{
    if (inv_item->object_id == O_COMPASS_OPTION
        && !g_Config.gameplay.enable_compass_stats) {
        return;
    }

    p->is_ready = true;
    p->ui_state = UI_StatsDialog_Init((UI_STATS_DIALOG_ARGS) {
        .mode = Game_IsInGym() && Gym_TrackManager_HasStats(GYM_TRACK_ASSAULT)
            ? UI_STATS_DIALOG_MODE_ASSAULT_COURSE
            : UI_STATS_DIALOG_MODE_LEVEL,
        .style = UI_STATS_DIALOG_STYLE_BORDERED,
        .level_num = Game_GetCurrentLevel()->num,
    });
}

static void M_Close(M_PRIV *const p)
{
    if (p->is_ready) {
        p->is_ready = false;
        UI_StatsDialog_Free(p->ui_state);
        p->ui_state = nullptr;
    }
}

void Option_Stats_Control(INVENTORY_ITEM *const inv_item, const bool is_busy)
{
    M_PRIV *const p = &m_Priv;
    if (is_busy) {
        return;
    }

    if (!p->is_ready) {
        M_Init(p, inv_item);
    }
    if (p->is_ready) {
        UI_StatsDialog_Control(p->ui_state);
    }

    if (g_InputDB.menu_confirm || g_InputDB.menu_back) {
        M_Close(p);
        inv_item->anim_direction = 1;
        inv_item->goal_frame = inv_item->frames_total - 1;
        if (inv_item->object_id == O_STOPWATCH_OPTION) {
            Sound_StopEffect(SFX_MENU_STOPWATCH);
        }
    } else if (inv_item->object_id == O_STOPWATCH_OPTION) {
        Sound_Effect(SFX_MENU_STOPWATCH, 0, SPM_ALWAYS);
    }
}

void Option_Stats_Draw(void)
{
    M_PRIV *const p = &m_Priv;
    if (p->is_ready) {
        UI_StatsDialog(p->ui_state);
    }
}

void Option_Stats_Close(void)
{
    M_PRIV *const p = &m_Priv;
    M_Close(p);
}

void Option_Stats_UpdateCompassNeedle(const INVENTORY_ITEM *const inv_item)
{
    const ITEM *const lara_item = Lara_GetItem();
    if (lara_item == nullptr) {
        return;
    }
    int16_t delta = -inv_item->y_rot - lara_item->rot.y - m_CompassNeedle;
    m_CompassSpeed = m_CompassSpeed * 19 / 20 + delta / 50;
    m_CompassNeedle += m_CompassSpeed;
}

int16_t Option_Stats_GetCompassNeedleAngle(void)
{
    return m_CompassNeedle;
}
