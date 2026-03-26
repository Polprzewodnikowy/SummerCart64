#include <trx/core/thread_pool.h>

#include <trx/core/memory.h>

#include <SDL2/SDL_cpuinfo.h>
#include <SDL2/SDL_thread.h>

typedef struct JOB {
    THREAD_FUNC func;
    void *user_data;
    struct JOB *next;
} JOB;

struct THREAD_POOL {
    SDL_Thread **threads;
    size_t num_threads;
    SDL_mutex *queue_mutex;
    SDL_cond *queue_cond;
    JOB *queue_head;
    JOB *queue_tail;
    bool stop;
    size_t working_count;
    SDL_cond *working_cond;
};

static int32_t M_WorkerThread(void *const arg)
{
    THREAD_POOL *const pool = arg;

    while (true) {
        SDL_LockMutex(pool->queue_mutex);
        while (!pool->stop && pool->queue_head == nullptr) {
            SDL_CondWait(pool->queue_cond, pool->queue_mutex);
        }
        if (pool->stop && pool->queue_head == nullptr) {
            SDL_UnlockMutex(pool->queue_mutex);
            break;
        }
        JOB *const job = pool->queue_head;
        pool->queue_head = job->next;
        if (pool->queue_head == nullptr) {
            pool->queue_tail = nullptr;
        }
        pool->working_count++;
        SDL_UnlockMutex(pool->queue_mutex);

        job->func(job->user_data);
        Memory_Free(job);

        SDL_LockMutex(pool->queue_mutex);
        pool->working_count--;
        if (pool->working_count == 0 && pool->queue_head == nullptr) {
            SDL_CondSignal(pool->working_cond);
        }
        SDL_UnlockMutex(pool->queue_mutex);
    }
    return 0;
}

THREAD_POOL *ThreadPool_Create(int32_t num_threads)
{
    if (num_threads <= 0) {
        num_threads = SDL_GetCPUCount();
        if (num_threads <= 0) {
            num_threads = 1;
        }
    }

    THREAD_POOL *const pool = Memory_Alloc(sizeof(*pool));
    pool->threads = Memory_Alloc(sizeof(SDL_Thread *) * num_threads);
    pool->num_threads = num_threads;
    pool->queue_mutex = SDL_CreateMutex();
    pool->queue_cond = SDL_CreateCond();
    pool->working_cond = SDL_CreateCond();
    pool->queue_head = pool->queue_tail = nullptr;
    pool->stop = false;
    pool->working_count = 0;

    for (int32_t i = 0; i < num_threads; i++) {
        pool->threads[i] = SDL_CreateThread(M_WorkerThread, "worker", pool);
    }
    return pool;
}

void ThreadPool_Destroy(THREAD_POOL *const pool)
{
    if (pool == nullptr) {
        return;
    }
    SDL_LockMutex(pool->queue_mutex);
    pool->stop = true;
    SDL_CondBroadcast(pool->queue_cond);
    SDL_UnlockMutex(pool->queue_mutex);

    for (size_t i = 0; i < pool->num_threads; i++) {
        SDL_WaitThread(pool->threads[i], nullptr);
    }
    SDL_DestroyCond(pool->working_cond);
    SDL_DestroyCond(pool->queue_cond);
    SDL_DestroyMutex(pool->queue_mutex);
    Memory_Free(pool->threads);
    Memory_Free(pool);
}

bool ThreadPool_AddJob(
    THREAD_POOL *const pool, THREAD_FUNC func, void *const user_data)
{
    JOB *const job = Memory_Alloc(sizeof(JOB));
    job->func = func;
    job->user_data = user_data;
    job->next = nullptr;

    SDL_LockMutex(pool->queue_mutex);
    if (pool->queue_tail) {
        pool->queue_tail->next = job;
        pool->queue_tail = job;
    } else {
        pool->queue_head = pool->queue_tail = job;
    }
    SDL_CondSignal(pool->queue_cond);
    SDL_UnlockMutex(pool->queue_mutex);
    return true;
}

void ThreadPool_Wait(THREAD_POOL *pool)
{
    SDL_LockMutex(pool->queue_mutex);
    while (pool->queue_head != nullptr || pool->working_count != 0) {
        SDL_CondWait(pool->working_cond, pool->queue_mutex);
    }
    SDL_UnlockMutex(pool->queue_mutex);
}
