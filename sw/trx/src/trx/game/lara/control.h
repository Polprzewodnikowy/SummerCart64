#pragma once

#include <trx/game/game_flow.h>
#include <trx/game/lara/types.h>

void Lara_Control_Initialise(
    GF_LEVEL_TYPE level_type, LARA_EXTRA_STATE start_state);
void Lara_Control(void);
