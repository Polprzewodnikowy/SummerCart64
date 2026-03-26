#include <trx/game/option/gameplay.h>

#include <trx/game/input.h>
#include <trx/game/ui/dialogs/gameplay_settings.h>

typedef struct {
    bool is_ready;
    UI_SETTINGS_DIALOG_STATE *ui_state;
} M_PRIV;

static M_PRIV m_Priv = {};

static void M_Init(M_PRIV *const p)
{
    p->is_ready = true;
    if (p->ui_state == nullptr) {
        p->ui_state = UI_GameplaySettings_Init();
    }
}

static void M_Shutdown(M_PRIV *const p)
{
    if (p->ui_state != nullptr) {
        UI_GameplaySettings_Free(p->ui_state);
        p->ui_state = nullptr;
    }
    p->is_ready = false;
}

void Option_Gameplay_Control(INVENTORY_ITEM *const inv_item, const bool is_busy)
{
    M_PRIV *const p = &m_Priv;
    if (is_busy) {
        return;
    }
    if (!p->is_ready) {
        M_Init(p);
    }
    if (UI_GameplaySettings_Control(p->ui_state)) {
        if (g_InputDB.menu_confirm || g_InputDB.menu_back) {
            inv_item->anim_direction = 1;
            inv_item->goal_frame = inv_item->frames_total - 1;
        }
    } else {
        g_Input = (INPUT_STATE) {};
        g_InputDB = (INPUT_STATE) {};
    }
}

void Option_Gameplay_Draw(INVENTORY_ITEM *const inv_item)
{
    M_PRIV *const p = &m_Priv;
    if (p->is_ready && p->ui_state != nullptr) {
        UI_GameplaySettings(p->ui_state);
    }
}

void Option_Gameplay_Close(void)
{
    M_PRIV *const p = &m_Priv;
    p->is_ready = false;
}

void Option_Gameplay_Shutdown(void)
{
    M_PRIV *const p = &m_Priv;
    M_Shutdown(p);
}
