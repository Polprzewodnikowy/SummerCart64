#include <trx/core/strings/fuzzy_match.h>

#include <trx/core/memory.h>
#include <trx/core/strings/common.h>

#include <stdio.h>
#include <string.h>

#define FULL_MATCH_SCORE_BONUS 100
#define WORD_MATCH_SCORE_BONUS 50
#define PERCENT_MATCH_SCORE 50
#define LETTER_MATCH_SCORE_BONUS 1

static STRING_FUZZY_SCORE M_GetScore(
    const char *const user_input, const char *const reference,
    const int32_t weight)
{
    const int32_t percent_score =
        PERCENT_MATCH_SCORE * strlen(user_input) / strlen(reference);
    const int32_t letter_score = LETTER_MATCH_SCORE_BONUS * strlen(user_input);

    char *word_regex = Memory_Alloc(strlen(user_input) + 20);
    char *full_regex = Memory_Alloc(strlen(user_input) + 20);
    sprintf(word_regex, "\\b%s\\b", user_input);
    sprintf(full_regex, "^\\s*%s\\s*$", user_input);

    // Assume a partial match
    bool is_full = false;
    bool is_word = false;
    int32_t score = letter_score + percent_score;
    if (String_Match(reference, full_regex)) {
        // Got a full match
        is_full = true;
        score += FULL_MATCH_SCORE_BONUS;
    } else if (String_Match(reference, word_regex)) {
        // Got a word match
        is_word = true;
        score += WORD_MATCH_SCORE_BONUS;
    } else if (String_CaseSubstring(reference, user_input) == nullptr) {
        // No match.
        score = 0;
    }

    Memory_FreePointer(&word_regex);
    Memory_FreePointer(&full_regex);

    return (STRING_FUZZY_SCORE) {
        .is_full = is_full,
        .is_word = is_word,
        .score = score * weight,
    };
}

static void M_DiscardNonFullMatches(VECTOR *const matches)
{
    bool has_full_match = false;
    for (int32_t i = 0; i < matches->count; i++) {
        const STRING_FUZZY_MATCH *const match = Vector_Get(matches, i);
        if (match->score.is_full) {
            has_full_match = true;
        }
    }
    if (has_full_match) {
        for (int32_t i = matches->count - 1; i >= 0; i--) {
            const STRING_FUZZY_MATCH *const match = Vector_Get(matches, i);
            if (!match->score.is_full) {
                Vector_RemoveAt(matches, i);
            }
        }
    }
}

static void M_DiscardNonWordMatches(VECTOR *const matches)
{
    bool has_word_match = false;
    for (int32_t i = 0; i < matches->count; i++) {
        const STRING_FUZZY_MATCH *const match = Vector_Get(matches, i);
        if (match->score.is_word) {
            has_word_match = true;
        }
    }
    if (has_word_match) {
        for (int32_t i = matches->count - 1; i >= 0; i--) {
            const STRING_FUZZY_MATCH *const match = Vector_Get(matches, i);
            if (!match->score.is_word) {
                Vector_RemoveAt(matches, i);
            }
        }
    }
}

static void M_SortMatches(VECTOR *const matches)
{
    // sort by match length so that best-matching results appear first
    for (int32_t i = 0; i < matches->count; i++) {
        const STRING_FUZZY_MATCH *const match_1 = Vector_Get(matches, i);
        for (int32_t j = i + 1; j < matches->count; j++) {
            const STRING_FUZZY_MATCH *const match_2 = Vector_Get(matches, j);
            if (match_1->score.score < match_2->score.score) {
                Vector_Swap(matches, i, j);
            }
        }
    }
}

static void M_DiscardDuplicateMatches(VECTOR *const matches)
{
    for (int32_t i = matches->count - 1; i >= 0; i--) {
        const STRING_FUZZY_MATCH *const match = Vector_Get(matches, i);
        bool is_unique = true;
        for (int32_t j = 0; j < matches->count; j++) {
            const STRING_FUZZY_MATCH *const other_match =
                Vector_Get(matches, j);
            if (j != i && match->value == other_match->value) {
                is_unique = false;
                break;
            }
        }
        if (!is_unique) {
            Vector_RemoveAt(matches, i);
        }
    }
}

VECTOR *String_FuzzyMatch(const char *user_input, const VECTOR *const source)
{
    VECTOR *matches = Vector_Create(sizeof(STRING_FUZZY_MATCH));

    for (int32_t i = 0; i < source->count; i++) {
        const STRING_FUZZY_SOURCE *const source_item =
            Vector_Get((VECTOR *)source, i);
        const STRING_FUZZY_SCORE score =
            M_GetScore(user_input, source_item->key, source_item->weight);

        if (score.score <= 0) {
            continue;
        }

        STRING_FUZZY_MATCH match = {
            .key = source_item->key,
            .value = source_item->value,
            .score = score,
        };
        Vector_Add(matches, &match);
    }

    M_DiscardNonFullMatches(matches);
    M_DiscardNonWordMatches(matches);
    M_DiscardDuplicateMatches(matches);
    M_SortMatches(matches);

    return matches;
}
