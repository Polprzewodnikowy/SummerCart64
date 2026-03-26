#include <trx/game/catalog/manager.h>
#include <trx/game/items.h>

ITEM_ACTION ItemAction_ToGameID(const ITEM_TRX_ACTION action)
{
    ITEM_ACTION out;
    if (Catalog_EnumToGameID(CATALOG_ITEM_ACTIONS, action, &out)) {
        return out;
    }
    return ITEM_ACTION_INVALID;
}

ITEM_TRX_ACTION ItemAction_FromGameID(const ITEM_ACTION action)
{
    ITEM_TRX_ACTION out;
    if (Catalog_GameIDToEnum(CATALOG_ITEM_ACTIONS, action, &out)) {
        return out;
    }
    return ITEM_TRX_ACTION_INVALID;
}
