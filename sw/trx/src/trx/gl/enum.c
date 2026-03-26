#include <trx/gl/enum.h>

#include <trx/core/enum_map.h>

static __attribute__((constructor)) void M_Init(void)
{
    ENUM_MAP(TEXTURE_FILTER, TEXTURE_FILTER_BILINEAR, "bilinear");
    ENUM_MAP(TEXTURE_FILTER, TEXTURE_FILTER_POINT, "point");
}
