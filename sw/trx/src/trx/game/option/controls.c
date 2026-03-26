#include <trx/game/option/controls.h>

#include <trx/config.h>
#include <trx/game/ui.h>

typedef struct {
    int32_t listeners[2];
    struct {
        bool is_ready;
        UI_CONTROLS_STATE state;
    } ui;
} M_PRIV;

static M_PRIV m_Priv = {};

static void M_HandleKeyChange(const EVENT *event, void *user_data)
{
    g_Config.dirty = true;
    Config_Update();
}

static void M_HandleLayoutChange(const EVENT *event, void *user_data)
{
    const M_PRIV *const p = user_data;
    g_Config.input.layout[p->ui.state.backend] =
        p->ui.state.editor_state[p->ui.state.backend].active_layout;
    Config_Update();
}

static void M_Init(M_PRIV *const p)
{
    UI_Controls_Init(&p->ui.state);
    p->ui.is_ready = true;
    p->listeners[0] = EventManager_Subscribe(
        p->ui.state.events, "layout_change", nullptr, M_HandleLayoutChange, p);
    p->listeners[1] = EventManager_Subscribe(
        p->ui.state.events, "key_change", nullptr, M_HandleKeyChange, p);
}

static void M_Shutdown(M_PRIV *const p)
{
    if (p->ui.is_ready) {
        EventManager_Unsubscribe(p->ui.state.events, p->listeners[0]);
        EventManager_Unsubscribe(p->ui.state.events, p->listeners[1]);
        UI_Controls_Free(&p->ui.state);
        p->ui.is_ready = false;
    }
}

void Option_Controls_Control(INVENTORY_ITEM *const inv_item, const bool is_busy)
{
    M_PRIV *const p = &m_Priv;
    if (is_busy) {
        return;
    }

    if (!p->ui.is_ready) {
        M_Init(p);
    }

    if (!UI_Controls_Control(&p->ui.state)) {
        g_Input = (INPUT_STATE) {};
        g_InputDB = (INPUT_STATE) {};
    }
}

void Option_Controls_Draw(INVENTORY_ITEM *const inv_item)
{
    M_PRIV *const p = &m_Priv;
    if (p->ui.is_ready) {
        UI_Controls(&p->ui.state);
    }
}

void Option_Controls_Close(void)
{
}

void Option_Controls_Shutdown(void)
{
    M_PRIV *const p = &m_Priv;
    M_Shutdown(p);
}
