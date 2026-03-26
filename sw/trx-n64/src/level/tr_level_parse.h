/*
 * TRX-N64: TR1 .PHD level file parser
 *
 * Reads the binary .PHD format and populates tr_level_t.
 * Uses the bump allocator for all allocations (reset per level).
 *
 * The .PHD data is already in the asset cache (loaded by n64_level_loader).
 * This parser reads sequentially from a memory buffer.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TR_LEVEL_PARSE_H
#define TR_LEVEL_PARSE_H

#include "tr_types.h"

/* Parse result codes */
typedef enum {
    TR_PARSE_OK = 0,
    TR_PARSE_BAD_VERSION,
    TR_PARSE_TRUNCATED,
    TR_PARSE_OUT_OF_MEMORY,
    TR_PARSE_TOO_MANY_ROOMS,
    TR_PARSE_TOO_MANY_ITEMS,
} tr_parse_result_t;

/* Sequential reader state */
typedef struct {
    const uint8_t *data;
    uint32_t       size;
    uint32_t       pos;
} tr_reader_t;

/*
 * Parse a complete TR1 .PHD level from memory.
 *
 * level_data: Pointer to the .PHD file contents in memory.
 * level_size: Size of the .PHD data in bytes.
 * level:      Output structure (caller provides, parser fills).
 *
 * All dynamic allocations go through n64_mem_alloc() (bump allocator).
 * Call n64_mem_reset() when done with the level to free everything.
 */
tr_parse_result_t tr_level_parse(const uint8_t *level_data,
                                  uint32_t level_size,
                                  tr_level_t *level);

/* Get human-readable error string */
const char *tr_parse_error_string(tr_parse_result_t result);

#endif /* TR_LEVEL_PARSE_H */
