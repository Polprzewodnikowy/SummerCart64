/*
 * MW-N64: ESM dialogue record parser (DIAL + INFO)
 *
 * INTEGRITY RULE: Record structure from OpenMW loadinfo.hpp/cpp
 * and loaddial.cpp. DATAstruct fields verified line-by-line.
 *
 * INFO.DATA (from loadinfo.hpp:38-48):
 *   int32 type (dialogue type)
 *   int32 disposition (or journal index)
 *   int8  rank (-1 = any)
 *   int8  gender (-1=NA, 0=male, 1=female)
 *   int8  PCrank (-1 = any)
 *   1 byte padding
 *
 * INFO subrecords (from loadinfo.cpp:28-95):
 *   DATA, ONAM(actor), RNAM(race), CNAM(class), FNAM(faction),
 *   ANAM(cell), DNAM(PCfaction), NAME(response text),
 *   SCVR(select conditions), BNAM(result script),
 *   QSTN/QSTF/QSTR(quest status)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "mw_esm.h"
#include "../game/mw_dialogue.h"
#include <string.h>

/* Max dialogues and infos to index */
#define MAX_DIAL_TOPICS     4096
#define MAX_DIAL_INFOS      32768

/* Dialogue topic entry */
typedef struct {
    char     name[64];
    uint8_t  type;          /* 0=topic,1=voice,2=greeting,3=persuasion,4=journal */
    uint32_t first_info;    /* Index into info array */
    uint32_t info_count;
} dial_topic_entry_t;

/* Dialogue info entry (minimal — full text loaded on demand) */
typedef struct {
    uint32_t file_offset;   /* ESM offset for lazy loading response text */
    int32_t  disposition;
    int8_t   rank;
    int8_t   gender;
    int8_t   pc_rank;
    char     actor[64];     /* NPC ID filter */
    char     race[64];
    char     class_[64];
    char     faction[64];
    char     cell[64];
} dial_info_entry_t;

/* Dialogue index (built during ESM scan) */
static dial_topic_entry_t s_topics[MAX_DIAL_TOPICS];
static int s_topic_count = 0;
static dial_info_entry_t s_infos[MAX_DIAL_INFOS];
static int s_info_count = 0;

/*
 * Parse DIAL and INFO records from ESM during index build.
 * Called from esm_build_index when a DIAL record is encountered.
 *
 * DIAL record: NAME(topic name), DATA(type byte)
 * Followed by zero or more INFO records with the dialogue responses.
 */
void esm_parse_dialogue(esm_reader_t *r) {
    if (s_topic_count >= MAX_DIAL_TOPICS) {
        esm_skip_record(r);
        return;
    }

    dial_topic_entry_t *topic = &s_topics[s_topic_count];
    memset(topic, 0, sizeof(*topic));
    topic->first_info = s_info_count;

    /* Parse DIAL subrecords */
    while (esm_next_subrecord(r)) {
        if (r->sub_name == ESM_NAME) {
            esm_read_string(r, topic->name, 64);
        } else if (r->sub_name == ESM_DATA && r->sub_size >= 1) {
            topic->type = esm_read_u8(r);
        } else {
            esm_skip_subrecord(r);
        }
    }

    if (topic->name[0]) {
        s_topic_count++;
    }

    /* INFO records follow the DIAL record.
     * They are separate top-level records, read by the main loop.
     * The caller (esm_build_index) should call esm_parse_info()
     * for each subsequent INFO record until the next non-INFO record. */
}

/*
 * Parse a single INFO record.
 * Called for each INFO record following a DIAL.
 */
void esm_parse_info(esm_reader_t *r) {
    if (s_info_count >= MAX_DIAL_INFOS || s_topic_count == 0) {
        esm_skip_record(r);
        return;
    }

    dial_info_entry_t *info = &s_infos[s_info_count];
    memset(info, 0, sizeof(*info));
    info->rank = -1;
    info->gender = -1;
    info->pc_rank = -1;
    info->file_offset = r->pos;  /* Remember offset for lazy text loading */

    while (esm_next_subrecord(r)) {
        if (r->sub_name == ESM_DATA && r->sub_size >= 12) {
            /* DATAstruct: type(4) + disposition(4) + rank(1) + gender(1) + PCrank(1) + pad(1) */
            esm_read_s32(r);  /* type (matches parent DIAL type) */
            info->disposition = esm_read_s32(r);
            info->rank = esm_read_s8(r);
            info->gender = esm_read_s8(r);
            info->pc_rank = esm_read_s8(r);
            esm_read_u8(r);  /* padding */
        } else if (r->sub_name == ESM_TAG('O','N','A','M')) {
            esm_read_string(r, info->actor, 64);
        } else if (r->sub_name == ESM_RNAM) {
            esm_read_string(r, info->race, 64);
        } else if (r->sub_name == ESM_CNAM) {
            esm_read_string(r, info->class_, 64);
        } else if (r->sub_name == ESM_FNAM) {
            esm_read_string(r, info->faction, 64);
        } else if (r->sub_name == ESM_ANAM) {
            esm_read_string(r, info->cell, 64);
        } else {
            /* NAME(response text), SCVR(conditions), BNAM(result script)
             * — skipped during indexing, loaded on demand */
            esm_skip_subrecord(r);
        }
    }

    s_info_count++;
    /* Update parent topic's count */
    s_topics[s_topic_count - 1].info_count++;
}

/*
 * Load the full response text for an INFO at a given file offset.
 */
bool esm_load_info_text(esm_reader_t *r, uint32_t file_offset,
                         char *text_out, int max_len,
                         char *script_out, int max_script_len) {
    r->pos = file_offset;
    text_out[0] = '\0';
    script_out[0] = '\0';

    /* Re-parse this INFO to find NAME and BNAM subrecords */
    while (r->pos < r->size && r->pos < file_offset + 8192) {
        uint32_t sub_tag = esm_read_u32(r);
        uint32_t sub_size = esm_read_u32(r);
        uint32_t sub_end = r->pos + sub_size;

        if (sub_tag == ESM_NAME) {
            uint32_t copy = sub_size < (uint32_t)(max_len - 1) ? sub_size : (uint32_t)(max_len - 1);
            esm_read_bytes(r, text_out, copy);
            text_out[copy] = '\0';
        } else if (sub_tag == ESM_BNAM) {
            uint32_t copy = sub_size < (uint32_t)(max_script_len - 1) ? sub_size : (uint32_t)(max_script_len - 1);
            esm_read_bytes(r, script_out, copy);
            script_out[copy] = '\0';
        }

        r->pos = sub_end;
    }

    return text_out[0] != '\0';
}

/* Getters for the dialogue system */
int esm_get_topic_count(void) { return s_topic_count; }

const char *esm_get_topic_name(int index) {
    if (index < 0 || index >= s_topic_count) return "";
    return s_topics[index].name;
}

int esm_get_topic_type(int index) {
    if (index < 0 || index >= s_topic_count) return -1;
    return s_topics[index].type;
}
