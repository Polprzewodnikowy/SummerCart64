#pragma once

#include <trx/game/output/types.h>

CLIP Output_CheckBoundsClip(const BOUNDS_16 *bounds);

void Output_MakeScreenshot(const char *path);

void Output_ApplyFOV(void);
