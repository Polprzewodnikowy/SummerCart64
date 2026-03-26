#pragma once

#include <SDL2/SDL_stdinc.h>

typedef struct {
    Uint64 start;
    Uint64 last;
} BENCHMARK;

BENCHMARK Benchmark_Start(void);

#define Benchmark_End(b, ...)                                                  \
    Benchmark_End_Impl(b, __FILE__, __LINE__, __func__, __VA_ARGS__)

#define Benchmark_Tick(b, ...)                                                 \
    Benchmark_Tick_Impl(b, __FILE__, __LINE__, __func__, __VA_ARGS__)

void Benchmark_End_Impl(
    BENCHMARK *b, const char *file, int32_t line, const char *func,
    const char *message);

void Benchmark_Tick_Impl(
    BENCHMARK *b, const char *file, int32_t line, const char *func,
    const char *message);
