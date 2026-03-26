#pragma once

#include <trx/game/phase/types.h>

GF_COMMAND PhaseExecutor_Run(PHASE *phase);

PHASE *PhaseExecutor_GetOuterPhase(void);
