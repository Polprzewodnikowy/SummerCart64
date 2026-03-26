#include <trx/core/hash.h>

#include <string.h>

#define M_FNV_1A_PRIME 1099511628211ULL

uint64_t Hash_FNV1a64_Init(void)
{
    return HASH_FNV1A64_BASE;
}

uint64_t Hash_FNV1a64_Update(
    uint64_t hash, const void *const data, const size_t size)
{
    const uint8_t *cur = data;
    for (size_t i = 0; i < size; i++) {
        hash ^= cur[i];
        hash *= M_FNV_1A_PRIME;
    }
    return hash;
}

uint64_t Hash_FNV1a64_UpdateU32(const uint64_t hash, const uint32_t value)
{
    return Hash_FNV1a64_Update(hash, &value, sizeof(value));
}

uint64_t Hash_FNV1a64_UpdateU64(const uint64_t hash, const uint64_t value)
{
    return Hash_FNV1a64_Update(hash, &value, sizeof(value));
}

uint64_t Hash_FNV1a64_UpdateString(uint64_t hash, const char *const value)
{
    if (value == nullptr) {
        return Hash_FNV1a64_UpdateU32(hash, 0);
    }
    const uint32_t len = (uint32_t)strlen(value);
    hash = Hash_FNV1a64_UpdateU32(hash, len);
    return Hash_FNV1a64_Update(hash, value, len);
}
