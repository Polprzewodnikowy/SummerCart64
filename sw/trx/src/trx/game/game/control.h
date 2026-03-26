#pragma once

#include <trx/game/game_flow/types.h>

bool Game_Start(const GF_LEVEL *level, GF_SEQUENCE_CONTEXT seq_ctx);
void Game_End(void);

GF_COMMAND Game_Control(bool demo_mode);

void Game_ProcessInput(void);
