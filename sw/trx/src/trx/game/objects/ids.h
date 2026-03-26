#pragma once

#define O_FIRST 0

typedef enum {
    NO_OBJECT = -1,
#define X_CATALOG_ID(enum_value) enum_value,
#include <trx/game/catalog/objects.def>
#undef X_CATALOG_ID
    // sentinel
    O_NUMBER_OF,
} OBJECT_ID;
