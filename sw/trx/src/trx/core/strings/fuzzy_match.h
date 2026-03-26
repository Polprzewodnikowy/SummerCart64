#pragma once

#include <trx/core/vector.h>

#include <stdint.h>

typedef struct {
    const char *key;
    void *value;
    int32_t weight;
} STRING_FUZZY_SOURCE;

typedef struct {
    bool is_full;
    bool is_word;
    int32_t score;
} STRING_FUZZY_SCORE;

typedef struct {
    const char *key;
    void *value;
    STRING_FUZZY_SCORE score;
} STRING_FUZZY_MATCH;

// Takes a vector of STRING_FUZZY_SOURCE.
// Returns a vector of STRING_FUZZY_MATCH.
VECTOR *String_FuzzyMatch(const char *user_input, const VECTOR *source);
