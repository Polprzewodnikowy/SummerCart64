#pragma once

#include <stdint.h>

typedef struct THREAD_POOL THREAD_POOL;
typedef void (*THREAD_FUNC)(void *userdata);

// Create a thread pool with the given number of worker threads.
// Returns nullptr on failure.
THREAD_POOL *ThreadPool_Create(int32_t num_threads);

// Destroy a thread pool, freeing all resources.
// Blocks until all worker threads have exited.
void ThreadPool_Destroy(THREAD_POOL *pool);

// Submit a job to the thread pool. Returns true on success.
bool ThreadPool_AddJob(THREAD_POOL *pool, THREAD_FUNC func, void *user_data);

// Wait for all submitted jobs to complete.
void ThreadPool_Wait(THREAD_POOL *pool);
