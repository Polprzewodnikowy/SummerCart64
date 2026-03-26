#include <trx/core/benchmark.h>

#include <trx/core/log.h>

#include <SDL2/SDL_timer.h>

static void M_Log(
    BENCHMARK *const b, const char *file, int32_t line, const char *func,
    Uint64 current, const char *message, const bool closing)
{
    const Uint64 freq = SDL_GetPerformanceFrequency();
    const double elapsed_start =
        (double)(current - b->start) * 1000.0 / (double)freq;
    const double elapsed_last =
        (double)(current - b->last) * 1000.0 / (double)freq;

    if (closing) {
        if (message == nullptr) {
            Log_Message(
                LOG_LEVEL_DEBUG, file, line, func, "took %.02f ms",
                elapsed_start);
        } else {
            Log_Message(
                LOG_LEVEL_DEBUG, file, line, func, "%s: took %.02f ms", message,
                elapsed_start);
        }
    } else {
        if (message == nullptr) {
            Log_Message(
                LOG_LEVEL_DEBUG, file, line, func, "took %.02f ms (%.02f ms)",
                elapsed_start, elapsed_last);
        } else {
            Log_Message(
                LOG_LEVEL_DEBUG, file, line, func,
                "%s: took %.02f ms (%.02f ms)", message, elapsed_start,
                elapsed_last);
        }
    }
}

BENCHMARK Benchmark_Start(void)
{
    const Uint64 perf = SDL_GetPerformanceCounter();
    return (BENCHMARK) {
        .start = perf,
        .last = perf,
    };
}

void Benchmark_Tick_Impl(
    BENCHMARK *const b, const char *const file, const int32_t line,
    const char *const func, const char *const message)
{
    const Uint64 current = SDL_GetPerformanceCounter();
    M_Log(b, file, line, func, current, message, false);
    b->last = current;
}

void Benchmark_End_Impl(
    BENCHMARK *b, const char *const file, const int32_t line,
    const char *const func, const char *const message)
{
    const Uint64 current = SDL_GetPerformanceCounter();
    M_Log(b, file, line, func, current, message, true);
}
