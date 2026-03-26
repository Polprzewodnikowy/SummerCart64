#include <trx/game/game_buf.h>

#include <trx/core/enum_map.h>
#include <trx/core/memory.h>

static MEMORY_ARENA_ALLOCATOR m_Allocator[GBUF_NUM_MALLOC_TYPES] = {};

void GameBuf_Init(void)
{
    for (int32_t i = 0; i < GBUF_NUM_MALLOC_TYPES; i++) {
        m_Allocator[i].default_chunk_size = 2048;
    }
}

void GameBuf_Reset(void)
{
    for (int32_t i = 0; i < GBUF_NUM_MALLOC_TYPES; i++) {
        Memory_ArenaReset(&m_Allocator[i]);
    }
}

void GameBuf_ResetSingle(const GAME_BUFFER buffer)
{
    Memory_ArenaReset(&m_Allocator[buffer]);
}

void GameBuf_Shutdown(void)
{
    for (int32_t i = 0; i < GBUF_NUM_MALLOC_TYPES; i++) {
        Memory_ArenaFree(&m_Allocator[i]);
    }
}

void *GameBuf_Alloc(const size_t alloc_size, const GAME_BUFFER buffer)
{
    const size_t aligned_size = Memory_Align(alloc_size);
    return Memory_ArenaAlloc(&m_Allocator[buffer], aligned_size);
}
