#include "version.h"


#define VERSION_MAJOR   (2)
#define VERSION_MINOR   (12)


uint32_t version_firmware (void) {
    return ((VERSION_MAJOR << 16) | (VERSION_MINOR));
}
