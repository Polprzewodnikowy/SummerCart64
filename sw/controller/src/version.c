#include "version.h"


#define VERSION_MAJOR       (2)
#define VERSION_MINOR       (18)
#define VERSION_REVISION    (1)


void version_firmware (uint32_t *version, uint32_t *revision) {
    *version = ((VERSION_MAJOR << 16) | (VERSION_MINOR));
    *revision = VERSION_REVISION;
}
