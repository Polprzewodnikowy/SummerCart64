#pragma once

#include <trx/game/phase/executor.h>

typedef enum {
    PHOTO_MODE_CAMERA,
    PHOTO_MODE_LARA_POS,
    PHOTO_MODE_LAST = PHOTO_MODE_LARA_POS,
} PHOTO_MODE;

void PhotoMode_Start(void);
void PhotoMode_End(void);

PHOTO_MODE PhotoMode_GetCurrentMode(void);
PHASE_CONTROL PhotoMode_Control(void);
