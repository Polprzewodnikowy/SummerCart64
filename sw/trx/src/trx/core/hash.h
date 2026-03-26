#pragma once

#include <stddef.h>
#include <stdint.h>

#define HASH_FNV1A64_BASE 14695981039346656037ULL

uint64_t Hash_FNV1a64_Init(void);
uint64_t Hash_FNV1a64_Update(uint64_t hash, const void *data, size_t size);
uint64_t Hash_FNV1a64_UpdateU32(uint64_t hash, uint32_t value);
uint64_t Hash_FNV1a64_UpdateU64(uint64_t hash, uint64_t value);
uint64_t Hash_FNV1a64_UpdateString(uint64_t hash, const char *value);
