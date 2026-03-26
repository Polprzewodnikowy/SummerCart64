#include <trx/game/catalog/manager.h>
#include <trx/game/music.h>

MUSIC_ID Music_ToGameID(const MUSIC_TRX_ID music_track)
{
    int32_t out;
    if (Catalog_EnumToGameID(CATALOG_MUSIC, music_track, &out)) {
        return out;
    }
    return MX_INACTIVE;
}

MUSIC_TRX_ID Music_FromGameID(const MUSIC_ID track_id)
{
    CATALOG_ID out;
    if (Catalog_GameIDToEnum(CATALOG_MUSIC, track_id, &out)) {
        return out;
    }
    return MX_TRX_INVALID;
}
