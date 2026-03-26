#pragma once

#include <stdint.h>

void GF_DisableObjectsIfNeeded(void);

// NOTE: This API should be eliminated!
int32_t GF_BadGetLevelNum(void);
bool GF_BadIsMod(const char *mod);
