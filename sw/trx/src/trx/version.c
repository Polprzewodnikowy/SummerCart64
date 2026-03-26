#include <trx/version.h>

#ifndef MESON_BUILD
const char *g_TRXVersion = "TR1X (non-Docker build)";
#endif

int32_t g_TRVersion = 0; // overriden at runtime when loading a level
