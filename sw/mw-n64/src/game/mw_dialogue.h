/*
 * MW-N64: Dialogue system
 *
 * Morrowind dialogue uses DIAL (topic) and INFO (response) records.
 * Each INFO has conditions (faction, skill, disposition checks) that
 * determine whether it's available. All conditions are evaluated
 * from exact ESM data — no approximations.
 *
 * Morrowind has NO voice acting — all dialogue is text. This is
 * perfect for N64's memory constraints.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MW_DIALOGUE_H
#define MW_DIALOGUE_H

#include <stdint.h>
#include <stdbool.h>

/* Dialogue types (from ESM DIAL.DATA) */
#define DIAL_TOPIC      0
#define DIAL_VOICE      1
#define DIAL_GREETING   2
#define DIAL_PERSUASION 3
#define DIAL_JOURNAL    4

/* Max dialogue topics available at once */
#define MAX_ACTIVE_TOPICS   32
#define MAX_RESPONSE_LEN    4096   /* Morrowind responses can be long */

/* A dialogue condition (evaluated against player/NPC state) */
typedef struct {
    uint8_t  func_index;    /* What to check (faction, skill, etc.) */
    uint8_t  compare_op;    /* ==, !=, <, >, <=, >= */
    char     variable[64];  /* Variable/faction/skill name */
    int32_t  value;         /* Comparison value */
} dial_condition_t;

/* A single dialogue response (INFO record) */
typedef struct {
    char     text[MAX_RESPONSE_LEN];  /* Response text */
    char     npc_id[64];      /* Specific NPC (empty = any) */
    char     race[64];        /* Required race (empty = any) */
    char     class_[64];      /* Required class */
    char     faction[64];     /* Required faction */
    char     cell[64];        /* Required cell */
    int8_t   faction_rank;    /* Min faction rank (-1 = any) */
    int8_t   gender;          /* 0=male, 1=female, -1=any */
    int16_t  disposition;     /* Min disposition (0-100) */
    char     result_script[256]; /* Script to run when selected */

    dial_condition_t conditions[6]; /* Up to 6 conditions per INFO */
    int condition_count;
} dial_response_t;

/* Active dialogue state */
typedef struct {
    /* Available topics for current NPC */
    char    topics[MAX_ACTIVE_TOPICS][64];
    int     topic_count;

    /* Current response being displayed */
    dial_response_t current_response;
    bool    has_response;

    /* The NPC we're talking to */
    int     npc_item_index;
    char    npc_id[64];
} dial_state_t;

/* -------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------- */

/* Start dialogue with an NPC */
void dial_begin(dial_state_t *ds, int npc_item_index, const char *npc_id);

/* Get available topics for current NPC (filtered by conditions) */
void dial_get_topics(dial_state_t *ds);

/* Select a topic — finds the best matching INFO response */
bool dial_select_topic(dial_state_t *ds, const char *topic);

/* End dialogue */
void dial_end(dial_state_t *ds);

#endif /* MW_DIALOGUE_H */
