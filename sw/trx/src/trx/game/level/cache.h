#pragma once

#include <trx/core/filesystem.h>
#include <trx/core/json.h>
#include <trx/game/game_flow/types.h>

#include <stddef.h>
#include <stdint.h>

uint64_t LevelCache_InitChecksum(const char *scope, uint32_t version);
uint64_t LevelCache_UpdateLevelChecksum(
    uint64_t checksum, const GF_LEVEL *level);

const char *LevelCache_GetLevelKey(const GF_LEVEL *level);

MYFILE *LevelCache_OpenBinaryRead(const char *filename, uint64_t checksum);
MYFILE *LevelCache_OpenBinaryWrite(const char *filename, uint64_t checksum);

JSON_VALUE *LevelCache_ReadJSON(const char *filename, uint64_t checksum);
bool LevelCache_WriteJSON(
    const char *filename, uint64_t checksum, JSON_VALUE *root);
