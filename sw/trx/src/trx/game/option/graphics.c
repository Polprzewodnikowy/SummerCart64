#include <trx/game/option/graphics.h>

#include <trx/game/input.h>
#include <trx/game/ui/dialogs/graphic_settings.h>

typedef struct {
    UI_SETTINGS_DIALOG_STATE *ui_state;
} M_PRIV;

static M_PRIV m_Priv = {};

static void M_Init(M_PRIV *const p)
{
    if (p->ui_state == nullptr) {
        p->ui_state = UI_GraphicSettings_Init();
    }
}

static void M_Shutdown(M_PRIV *const p)
{
    if (p->ui_state != nullptr) {
        UI_GraphicSettings_Free(p->ui_state);
        p->ui_state = nullptr;
    }
}

void Option_Graphics_Control(INVENTORY_ITEM *const inv_item, const bool is_busy)
{
    M_PRIV *const p = &m_Priv;
    if (is_busy) {
        return;
    }
    if (p->ui_state == nullptr) {
        M_Init(p);
    }
    if (!UI_GraphicSettings_Control(p->ui_state)) {
        g_Input = (INPUT_STATE) {};
        g_InputDB = (INPUT_STATE) {};
    }
}

void Option_Graphics_Draw(INVENTORY_ITEM *const inv_item)
{
    M_PRIV *const p = &m_Priv;
    if (p->ui_state != nullptr) {
        UI_GraphicSettings(p->ui_state);
    }
}

void Option_Graphics_Close(void)
{
}

void Option_Graphics_Shutdown(void)
{
    M_PRIV *const p = &m_Priv;
    M_Shutdown(p);
}
