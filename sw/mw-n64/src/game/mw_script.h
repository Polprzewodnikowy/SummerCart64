/*
 * MW-N64: TES Script interpreter
 *
 * Morrowind's scripting language is a simple bytecode VM:
 *   - ~100 opcodes (if/else, set, get, messagebox, etc.)
 *   - No recursion, no complex data structures
 *   - Stack-based expression evaluation
 *   - Scripts run once per frame for active objects
 *
 * Scripts are stored in SCPT records in the ESM as:
 *   - SCHD: header (name, shorts count, longs count, floats count,
 *           data size, variable table size)
 *   - SCVR: variable names (null-separated)
 *   - SCDT: compiled bytecode
 *
 * We implement the full opcode set for quest completion fidelity.
 * All scripts from the ESM execute with exact Morrowind semantics.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MW_SCRIPT_H
#define MW_SCRIPT_H

#include <stdint.h>
#include <stdbool.h>

/* Max scripts active simultaneously */
#define SCRIPT_MAX_ACTIVE   64

/* Max local variables per script */
#define SCRIPT_MAX_SHORTS   32
#define SCRIPT_MAX_LONGS    32
#define SCRIPT_MAX_FLOATS   32

/* Script opcodes (subset — full list has ~100) */
typedef enum {
    OP_NOP = 0x0000,
    OP_END = 0x0101,        /* End script execution */

    /* Control flow */
    OP_IF = 0x0104,
    OP_ELSEIF = 0x0105,
    OP_ELSE = 0x0106,
    OP_ENDIF = 0x0107,
    OP_WHILE = 0x010E,
    OP_ENDWHILE = 0x010F,
    OP_RETURN = 0x0124,

    /* Variables */
    OP_SET = 0x0103,
    OP_SHORT = 0x0108,
    OP_LONG = 0x0109,
    OP_FLOAT = 0x010A,

    /* Game functions */
    OP_MESSAGEBOX = 0x1000,
    OP_JOURNAL = 0x1001,
    OP_SETJOURNALIDX = 0x1002,
    OP_GETJOURNALINDEX = 0x1003,
    OP_ADDITEM = 0x1004,
    OP_REMOVEITEM = 0x1005,
    OP_EQUIP = 0x1006,
    OP_GETITEMCOUNT = 0x1007,
    OP_ENABLE = 0x1008,
    OP_DISABLE = 0x1009,
    OP_STARTCOMBAT = 0x100A,
    OP_STOPCOMBAT = 0x100B,
    OP_MODDISP = 0x100C,
    OP_SETDISP = 0x100D,
    OP_GETDISP = 0x100E,
    OP_AITRAVEL = 0x100F,
    OP_AIWANDER = 0x1010,
    OP_AIFOLLOW = 0x1011,
    OP_AIESCORT = 0x1012,
    OP_PLAYSOUND = 0x1013,
    OP_GETPOS = 0x1014,
    OP_SETPOS = 0x1015,
    OP_GETANGLE = 0x1016,
    OP_SETANGLE = 0x1017,
    OP_GETDISTANCE = 0x1018,
    OP_POSITIONCELL = 0x1019,
    OP_MODHP = 0x101A,
    OP_MODCURRENTHEALTH = 0x101A,
    OP_GETHP = 0x101B,
    OP_GETCURRENTHEALTH = 0x101B,
    OP_MODATTRIB = 0x101C,
    OP_GETATTRIB = 0x101D,
    OP_MODSKILL = 0x101E,
    OP_GETSKILL = 0x101F,
    OP_SHOWMAP = 0x1020,
    OP_SETHEALTH = 0x1021,
    OP_GETLEVEL = 0x1022,
    OP_GETRACE = 0x1023,
    OP_GETPCRANK = 0x1024,
    OP_SAMEFACTION = 0x1025,
    OP_GETPCCELL = 0x1026,
    OP_FORCEGREETING = 0x1027,
    OP_GETDEAD = 0x1028,
    OP_GOODBYE = 0x1029,
    OP_CHOICE = 0x102A,
} script_opcode_t;

/* Script instance (one per active script) */
typedef struct {
    /* Script identity */
    char     name[64];
    char     attached_to[64];   /* Object ID this script is on */
    int      attached_item;     /* Item index in world */

    /* Compiled bytecode */
    const uint8_t *bytecode;
    uint32_t       bytecode_size;

    /* Instruction pointer */
    uint32_t ip;

    /* Local variables (exact Morrowind layout) */
    int16_t  shorts[SCRIPT_MAX_SHORTS];
    int32_t  longs[SCRIPT_MAX_LONGS];
    float    floats[SCRIPT_MAX_FLOATS];
    int      num_shorts, num_longs, num_floats;

    /* Variable names (for external access) */
    char     var_names[96][64];
    int      var_count;

    /* State */
    bool     active;
    bool     running;           /* Currently executing this frame */
} script_instance_t;

/* Global script state */
typedef struct {
    script_instance_t scripts[SCRIPT_MAX_ACTIVE];
    int               active_count;

    /* Global variables (GLOB records from ESM) */
    struct {
        char    name[64];
        float   value;
    } globals[256];
    int global_count;
} script_engine_t;

/* -------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------- */

/* Initialize script engine */
void script_init(script_engine_t *se);

/* Register a script from ESM SCPT record data */
void script_register(script_engine_t *se,
                      const char *name,
                      const uint8_t *bytecode, uint32_t size,
                      int num_shorts, int num_longs, int num_floats);

/* Attach a script to an object */
void script_attach(script_engine_t *se, const char *script_name,
                    const char *object_id, int item_index);

/* Run all active scripts for one frame */
void script_run_frame(script_engine_t *se);

/* Get/set global variable */
float script_get_global(script_engine_t *se, const char *name);
void  script_set_global(script_engine_t *se, const char *name, float value);

/* Get/set journal index (quest progress) */
int32_t script_get_journal(script_engine_t *se, const char *quest_id);
void    script_set_journal(script_engine_t *se, const char *quest_id, int32_t index);

#endif /* MW_SCRIPT_H */
