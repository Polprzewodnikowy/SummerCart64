#include <trx/core/memory.h>

#include <trx/core/utils.h>
#include <trx/debug.h>

#include <stdlib.h>
#include <string.h>

static MEMORY_ARENA_CHUNK *M_ArenaAllocChunk(
    MEMORY_ARENA_ALLOCATOR *const allocator, const size_t size)
{
    const size_t new_chunk_size = MAX(allocator->default_chunk_size, size);
    MEMORY_ARENA_CHUNK *const new_chunk =
        Memory_Alloc(sizeof(MEMORY_ARENA_CHUNK) + new_chunk_size);
    new_chunk->memory = (char *)new_chunk + sizeof(MEMORY_ARENA_CHUNK);
    new_chunk->size = new_chunk_size;
    new_chunk->offset = 0;
    new_chunk->next = nullptr;
    return new_chunk;
}

size_t Memory_Align(const size_t size)
{
    return (size + 7) & ~7;
}

void *Memory_Alloc(const size_t size)
{
    void *result = malloc(size);
    ASSERT(result != nullptr);
    memset(result, 0, size);
    return result;
}

void *Memory_Realloc(void *const memory, const size_t size)
{
    void *result = realloc(memory, size);
    ASSERT(result != nullptr);
    return result;
}

void Memory_Free(void *const memory)
{
    if (memory != nullptr) {
        free(memory);
    }
}

void Memory_FreePointer(void *arg)
{
    ASSERT(arg != nullptr);
    void *memory;
    memcpy(&memory, arg, sizeof(void *));
    memcpy(arg, &(void *) { nullptr }, sizeof(void *));
    Memory_Free(memory);
}

void *Memory_Dup(const void *const buffer, const size_t size)
{
    ASSERT(buffer != nullptr);
    char *memory = Memory_Alloc(size);
    memcpy(memory, buffer, size);
    return memory;
}

char *Memory_DupStr(const char *const string)
{
    if (string == nullptr) {
        return nullptr;
    }
    char *memory = Memory_Alloc(strlen(string) + 1);
    strcpy(memory, string);
    return memory;
}

void *Memory_ArenaAlloc(
    MEMORY_ARENA_ALLOCATOR *const allocator, const size_t size)
{
    // Ensure a default chunk size is set.
    if (allocator->default_chunk_size == 0) {
        allocator->default_chunk_size = 1024 * 4; // default to 4K
    }

    // Find first chunk that has enough space.
    MEMORY_ARENA_CHUNK *chunk = allocator->current_chunk;
    while (chunk != nullptr && chunk->offset + size > chunk->size) {
        chunk = chunk->next;
    }

    // If no chunk satisfies this criteria, append a new chunk.
    if (chunk == nullptr) {
        chunk = M_ArenaAllocChunk(allocator, size);
        if (allocator->current_chunk != nullptr) {
            chunk->next = allocator->current_chunk->next;
            allocator->current_chunk->next = chunk;
        }
        allocator->current_chunk = chunk;
        if (allocator->first_chunk == nullptr) {
            allocator->first_chunk = chunk;
        }
    }

    ASSERT(chunk != nullptr);

    // Allocate from the current chunk.
    void *const result = (char *)chunk->memory + chunk->offset;
    chunk->offset += size;
    return result;
}

void Memory_ArenaReset(MEMORY_ARENA_ALLOCATOR *const allocator)
{
    MEMORY_ARENA_CHUNK *chunk = allocator->first_chunk;
    while (chunk != nullptr) {
        memset(chunk->memory, 0, chunk->size);
        chunk->offset = 0;
        chunk = chunk->next;
    }
    allocator->current_chunk = allocator->first_chunk;
}

void Memory_ArenaFree(MEMORY_ARENA_ALLOCATOR *const allocator)
{
    MEMORY_ARENA_CHUNK *chunk = allocator->first_chunk;
    while (chunk != nullptr) {
        MEMORY_ARENA_CHUNK *const next = chunk->next;
        Memory_Free(chunk);
        chunk = next;
    }
    allocator->first_chunk = nullptr;
    allocator->current_chunk = nullptr;
}
