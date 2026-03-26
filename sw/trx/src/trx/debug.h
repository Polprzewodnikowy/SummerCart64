#pragma once

#include <trx/core/log.h>

#define ASSERT(x)                                                              \
    do {                                                                       \
        if (!(x)) {                                                            \
            LOG_ERROR("Assertion failed: %s", #x);                             \
            __builtin_trap();                                                  \
        }                                                                      \
    } while (0)

#define ASSERT_FAIL()                                                          \
    do {                                                                       \
        LOG_ERROR("Assertion failed");                                         \
        __builtin_trap();                                                      \
    } while (0)

#define ASSERT_FAIL_FMT(fmt, ...)                                              \
    do {                                                                       \
        LOG_ERROR("Assertion failed: " fmt __VA_OPT__(, ) __VA_ARGS__);        \
        __builtin_trap();                                                      \
    } while (0)

#define SOFT_ASSERT(x, msg)                                                    \
    do {                                                                       \
        if (!(x)) {                                                            \
            LOG_ERROR("Warning: %s (%s)", msg, #x);                            \
        }                                                                      \
    } while (0)
