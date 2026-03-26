#pragma once

#include <stdint.h>

typedef enum {
    ITEM_ACTION_INVALID = -1,
} ITEM_ACTION;

typedef enum {
    ITEM_TRX_ACTION_INVALID = -1,
#define X_CATALOG_ID(enum_value) enum_value,
#include <trx/game/catalog/item_actions.def>
#undef X_CATALOG_ID
    ITEM_ACTION_NUMBER_OF,
} ITEM_TRX_ACTION;

ITEM_ACTION ItemAction_ToGameID(ITEM_TRX_ACTION action);
ITEM_TRX_ACTION ItemAction_FromGameID(ITEM_ACTION action);
