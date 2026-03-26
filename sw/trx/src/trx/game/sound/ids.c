#include <trx/game/catalog/manager.h>
#include <trx/game/sound.h>

SAMPLE_ID Sound_ToGameID(const SAMPLE_TRX_ID trx_id)
{
    int32_t out;
    if (Catalog_EnumToGameID(CATALOG_SAMPLES, trx_id, &out)) {
        return out;
    }
    return SFX_INVALID;
}

SAMPLE_TRX_ID Sound_FromGameID(const SAMPLE_ID sample_id)
{
    CATALOG_ID out;
    if (Catalog_GameIDToEnum(CATALOG_SAMPLES, sample_id, &out)) {
        return out;
    }
    return SFX_TRX_INVALID;
}
