#include <trx/game/items/actions.h>
#include <trx/game/rooms.h>

static void (*m_Routines[ITEM_ACTION_NUMBER_OF])(ITEM *item) = {};
static int16_t m_FXType = 0;

int16_t ItemAction_GetFXType(void)
{
    return m_FXType;
}

void ItemAction_Register(
    const ITEM_TRX_ACTION action, void (*const action_func)(ITEM *item))
{
    m_Routines[action] = action_func;
}

void ItemAction_Run(const ITEM_TRX_ACTION action_id, ITEM *const item)
{
    if (action_id >= 0 && action_id < ITEM_ACTION_NUMBER_OF
        && m_Routines[action_id] != nullptr) {
        m_Routines[action_id](item);
    }
}

static void M_RunWithFX(
    const ITEM_TRX_ACTION action_id, ITEM *const item, const int16_t fx_type)
{
    m_FXType = fx_type;
    ItemAction_Run(action_id, item);
    m_FXType = 0;
}

void ItemAction_RunDirect(const ITEM_ACTION action_id, ITEM *const item)
{
    const ITEM_TRX_ACTION trx_id = ItemAction_FromGameID(action_id);
    ItemAction_Run(trx_id, item);
}

void ItemAction_RunDirectWithFX(
    const ITEM_ACTION action_id, ITEM *const item, const int16_t fx_type)
{
    const ITEM_TRX_ACTION trx_id = ItemAction_FromGameID(action_id);
    M_RunWithFX(trx_id, item, fx_type);
}

void ItemAction_RunActive(void)
{
    const int32_t flip_effect = Room_GetFlipEffect();
    if (flip_effect != -1) {
        ItemAction_RunDirect(flip_effect, nullptr);
    }
}
