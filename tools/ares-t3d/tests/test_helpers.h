/* Minimal test framework */
#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int _tests_run = 0;
static int _tests_failed = 0;

#define ASSERT(cond, msg) do { \
    _tests_run++; \
    if (!(cond)) { \
        fprintf(stderr, "  FAIL: %s:%d: %s\n", __FILE__, __LINE__, msg); \
        _tests_failed++; \
    } \
} while(0)

#define ASSERT_EQ(a, b, msg) ASSERT((a) == (b), msg)
#define ASSERT_NE(a, b, msg) ASSERT((a) != (b), msg)

#define TEST_SUMMARY() do { \
    if (_tests_failed) { \
        fprintf(stderr, "%d/%d tests FAILED\n", _tests_failed, _tests_run); \
        return 1; \
    } \
    fprintf(stderr, "%d/%d tests passed\n", _tests_run, _tests_run); \
    return 0; \
} while(0)

/* Build a big-endian 32-bit word in a byte buffer */
static inline void put_be32(uint8_t *buf, uint32_t offset, uint32_t val) {
    buf[offset+0] = (val >> 24) & 0xFF;
    buf[offset+1] = (val >> 16) & 0xFF;
    buf[offset+2] = (val >>  8) & 0xFF;
    buf[offset+3] = (val >>  0) & 0xFF;
}

static inline void put_be16(uint8_t *buf, uint32_t offset, uint16_t val) {
    buf[offset+0] = (val >> 8) & 0xFF;
    buf[offset+1] = (val >> 0) & 0xFF;
}
