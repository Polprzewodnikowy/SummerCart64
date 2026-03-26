#pragma once

#include <stddef.h>

// Basic memory utilities that exit the game in case the system runs out of
// memory.

// Arena allocator - a buffer that only grows, until it's reset. Doesn't
// support freeing while in-use.
typedef struct MEMORY_ARENA_CHUNK {
    void *memory;
    size_t size;
    size_t offset;
    struct MEMORY_ARENA_CHUNK *next;
} MEMORY_ARENA_CHUNK;

typedef struct {
    MEMORY_ARENA_CHUNK *first_chunk;
    MEMORY_ARENA_CHUNK *current_chunk;
    size_t default_chunk_size;
} MEMORY_ARENA_ALLOCATOR;

// Align byte count to the platform-specific pointer size.
size_t Memory_Align(size_t size);

// Allocate n bytes. In case the memory allocation fails, shows an error to the
// user and exits the application. The allocated memory is filled with zeros.
void *Memory_Alloc(size_t size);

// Reallocate existing memory to n bytes, returning an address to the
// reallocated memory. In case the memory allocation fails, shows an error to
// the user and exits the application. All pointers to the old memory address
// become invalid. Preserves the previous memory contents. If the memory is
// nullptr, the function acts like Memory_Alloc.
void *Memory_Realloc(void *memory, size_t size);

// Frees the memory associated with a given address. If the memory is nullptr,
// the function is a no-op.
void Memory_Free(void *memory);

// Frees the memory associated with a given pointer and sets it to nullptr. The
// user is expected to pass a pointer of their variable like so:
//
// char *mem = Memory_Alloc(10);
// Memory_FreePointer(&mem);
// (mem is now nullptr)
//
// Giving a nullptr to this function is a fatal error. Passing mem directly is
// also an error.
void Memory_FreePointer(void *memory);

// Duplicates a buffer. In case the memory allocation fails, shows an error to
// the user and exits the application.
// Giving a nullptr to this function is a fatal error.
void *Memory_Dup(const void *buffer, size_t size);

// Duplicates a string. In case the memory allocation fails, shows an error to
// the user and exits the application. The string must be nullptr-terminated.
// Giving a nullptr to this function returns nullptr.
char *Memory_DupStr(const char *string);

// Allocate n bytes using the arena allocator. If there's insufficient memory,
// grow the buffer using internal growth function. The allocated memory is
// filled with zeros.
void *Memory_ArenaAlloc(MEMORY_ARENA_ALLOCATOR *allocator, size_t size);

// Resets the buffer used by the arena allocator, but does not free the memory.
// allocator must not be a nullptr. Used to reset the buffer, but not suffer
// from performance penalty associated with reallocating the actual memory.
void Memory_ArenaReset(MEMORY_ARENA_ALLOCATOR *allocator);

// Frees the entire buffer owned by the arena allocator. allocator must not be
// nullptr.
void Memory_ArenaFree(MEMORY_ARENA_ALLOCATOR *allocator);

#define AUTO_FREE __attribute__((cleanup(Memory_FreePointer)))
