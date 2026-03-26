#pragma once

#include <trx/game/phase/types.h>

typedef struct {
    const char *background_path;
} PHASE_GLOBE_SELECT_ARGS;

PHASE *Phase_GlobeSelect_Create(PHASE_GLOBE_SELECT_ARGS args);
void Phase_GlobeSelect_Destroy(PHASE *phase);
