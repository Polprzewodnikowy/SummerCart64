#pragma once

#include <stdint.h>

typedef enum {
    MX_INACTIVE = -1,
} MUSIC_ID;

typedef enum {
    MX_TRX_INVALID = -1,
#define X_CATALOG_ID(enum_value) enum_value,
#include <trx/game/catalog/music.def>
#undef X_CATALOG_ID
    MX_NUMBER_OF,
} MUSIC_TRX_ID;

MUSIC_ID Music_ToGameID(MUSIC_TRX_ID music_track);
MUSIC_TRX_ID Music_FromGameID(MUSIC_ID track_id);
