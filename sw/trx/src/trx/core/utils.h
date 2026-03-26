#pragma once

#define Q(x) #x
#define QUOTE(x) Q(x)
#define CONCAT_(a, b) a##b
#define CONCAT(a, b) CONCAT_(a, b)

#define ARRAY_SIZE(x) (sizeof((x)) / sizeof((x)[0]))
#define SQUARE(A) ((A) * (A))
#ifndef ABS
    #define ABS(x) (((x) < 0) ? (-(x)) : (x))
    #define MIN(x, y) ((x) <= (y) ? (x) : (y))
    #define MAX(x, y) ((x) >= (y) ? (x) : (y))
#endif

#define MIN3(x, y, z) MIN(MIN((x), (y)), (z))
#define MAX3(x, y, z) MAX(MAX((x), (y)), (z))

#define CLAMPL(a, b)                                                           \
    do {                                                                       \
        if ((a) < (b))                                                         \
            (a) = (b);                                                         \
    } while (0)
#define CLAMPG(a, b)                                                           \
    do {                                                                       \
        if ((a) > (b))                                                         \
            (a) = (b);                                                         \
    } while (0)
#define CLAMP(a, b, c)                                                         \
    do {                                                                       \
        if ((a) < (b))                                                         \
            (a) = (b);                                                         \
        else if ((a) > (c))                                                    \
            (a) = (c);                                                         \
    } while (0)

#define MINMAX(target, low, high) MAX(MIN((target), (low)), (target))

#define SWAP(a, b)                                                             \
    do {                                                                       \
        typeof(a) SWAP_tmp_ = (a);                                             \
        (a) = (b);                                                             \
        (b) = SWAP_tmp_;                                                       \
    } while (0)
#define SWAP2(a, b, tmp)                                                       \
    do {                                                                       \
        (tmp) = (a);                                                           \
        (a) = (b);                                                             \
        (b) = (tmp);                                                           \
    } while (0)
#define TOGGLE(target)                                                         \
    do {                                                                       \
        (target) = !(target);                                                  \
    } while (0);
#define CYCLE(target, rate, number_of)                                         \
    do {                                                                       \
        (target) += (rate);                                                    \
        (target) += (number_of);                                               \
        (target) %= (number_of);                                               \
    } while (0);

#define ALIGN(a, bytes) ((a + (bytes) - 1) & (~(bytes - 1)))
#define TOGGLE_BIT(target_var, target_bit, condition)                          \
    do {                                                                       \
        if (condition) {                                                       \
            (target_var) |= (target_bit);                                      \
        } else {                                                               \
            (target_var) &= ~(target_bit);                                     \
        }                                                                      \
    } while (0)

#define MKTAG(a, b, c, d)                                                      \
    ((a) | ((b) << 8) | ((c) << 16) | ((unsigned)(d) << 24))

#define LERP(a, b, ratio) ((a) + ((b) - (a)) * (ratio))
