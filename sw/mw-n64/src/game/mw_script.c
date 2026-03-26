/*
 * MW-N64: TES Script interpreter — stub implementation
 *
 * Full bytecode interpreter to be implemented. For now, provides
 * the API surface so the game compiles and boots.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "mw_script.h"
#include <string.h>

void script_init(script_engine_t *se) {
    memset(se, 0, sizeof(*se));
}

void script_register(script_engine_t *se,
                      const char *name,
                      const uint8_t *bytecode, uint32_t size,
                      int num_shorts, int num_longs, int num_floats) {
    (void)se; (void)name; (void)bytecode; (void)size;
    (void)num_shorts; (void)num_longs; (void)num_floats;
}

void script_attach(script_engine_t *se, const char *script_name,
                    const char *object_id, int item_index) {
    (void)se; (void)script_name; (void)object_id; (void)item_index;
}

void script_run_frame(script_engine_t *se) {
    (void)se;
    /* TODO: iterate active scripts, execute bytecode */
}

float script_get_global(script_engine_t *se, const char *name) {
    for (int i = 0; i < se->global_count; i++) {
        if (strcmp(se->globals[i].name, name) == 0)
            return se->globals[i].value;
    }
    return 0.0f;
}

void script_set_global(script_engine_t *se, const char *name, float value) {
    for (int i = 0; i < se->global_count; i++) {
        if (strcmp(se->globals[i].name, name) == 0) {
            se->globals[i].value = value;
            return;
        }
    }
    if (se->global_count < 256) {
        strncpy(se->globals[se->global_count].name, name, 63);
        se->globals[se->global_count].value = value;
        se->global_count++;
    }
}

/* Journal tracking (quest progress) */
static struct { char id[64]; int32_t index; } s_journal[512];
static int s_journal_count = 0;

int32_t script_get_journal(script_engine_t *se, const char *quest_id) {
    (void)se;
    for (int i = 0; i < s_journal_count; i++) {
        if (strcmp(s_journal[i].id, quest_id) == 0)
            return s_journal[i].index;
    }
    return 0;
}

void script_set_journal(script_engine_t *se, const char *quest_id, int32_t index) {
    (void)se;
    for (int i = 0; i < s_journal_count; i++) {
        if (strcmp(s_journal[i].id, quest_id) == 0) {
            s_journal[i].index = index;
            return;
        }
    }
    if (s_journal_count < 512) {
        strncpy(s_journal[s_journal_count].id, quest_id, 63);
        s_journal[s_journal_count].index = index;
        s_journal_count++;
    }
}
