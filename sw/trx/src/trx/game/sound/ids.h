#pragma once

typedef enum {
    SFX_INVALID = -1,
} SAMPLE_ID;

typedef enum {
    SFX_TRX_INVALID = -1,
#define X_CATALOG_ID(enum_value) enum_value,
#include <trx/game/catalog/samples.def>
#undef X_CATALOG_ID
} SAMPLE_TRX_ID;

SAMPLE_ID Sound_ToGameID(SAMPLE_TRX_ID sample_id);
SAMPLE_TRX_ID Sound_FromGameID(SAMPLE_ID sample_id);
