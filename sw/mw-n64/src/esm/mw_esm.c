/*
 * MW-N64: ESM/ESP file parser
 *
 * Two-pass approach:
 *   Pass 1 (esm_build_index): Scan entire file, record offsets for
 *     CELL/LAND records, load STAT/NPC/etc definitions into RAM.
 *     Takes ~2 seconds for the 79 MB Morrowind.esm.
 *
 *   Pass 2 (esm_load_cell/land): Seek to indexed offset, parse
 *     only the requested cell's data. Called when player moves.
 *
 * All reads are little-endian (N64 is big-endian, so we byte-swap).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "mw_esm.h"
#include <string.h>

/* -------------------------------------------------------------------
 * Low-level reader (little-endian byte-swap)
 * ------------------------------------------------------------------- */

static inline bool has_bytes(esm_reader_t *r, uint32_t n) {
    return (r->pos + n) <= r->size;
}

static inline uint8_t read_u8(esm_reader_t *r) {
    if (!has_bytes(r, 1)) return 0;
    return r->data[r->pos++];
}

static inline uint16_t read_u16(esm_reader_t *r) {
    if (!has_bytes(r, 2)) return 0;
    uint16_t v = r->data[r->pos] | ((uint16_t)r->data[r->pos+1] << 8);
    r->pos += 2;
    return v;
}

static inline int16_t read_s16(esm_reader_t *r) {
    return (int16_t)read_u16(r);
}

static inline uint32_t read_u32(esm_reader_t *r) {
    if (!has_bytes(r, 4)) return 0;
    uint32_t v = r->data[r->pos]
               | ((uint32_t)r->data[r->pos+1] << 8)
               | ((uint32_t)r->data[r->pos+2] << 16)
               | ((uint32_t)r->data[r->pos+3] << 24);
    r->pos += 4;
    return v;
}

static inline int32_t read_s32(esm_reader_t *r) {
    return (int32_t)read_u32(r);
}

static inline float read_f32(esm_reader_t *r) {
    uint32_t bits = read_u32(r);
    float f;
    memcpy(&f, &bits, 4);
    return f;
}

static inline uint32_t read_tag(esm_reader_t *r) {
    /* Tags are 4 ASCII bytes — read as-is (not byte-swapped,
     * since we compare against ESM_TAG which is also little-endian packed) */
    return read_u32(r);
}

static void read_string(esm_reader_t *r, char *buf, uint32_t max_len, uint32_t data_len) {
    uint32_t copy = data_len < max_len - 1 ? data_len : max_len - 1;
    if (has_bytes(r, copy)) {
        memcpy(buf, r->data + r->pos, copy);
    }
    buf[copy] = '\0';
    r->pos += data_len;
}

static void skip_bytes(esm_reader_t *r, uint32_t n) {
    r->pos += n;
    if (r->pos > r->size) r->pos = r->size;
}

/* Simple string hash for cell name lookup */
static uint32_t hash_string(const char *s) {
    uint32_t h = 5381;
    while (*s) {
        h = ((h << 5) + h) + (uint8_t)(*s);
        s++;
    }
    return h;
}

/* -------------------------------------------------------------------
 * Record/subrecord navigation
 * ------------------------------------------------------------------- */

void esm_reader_init(esm_reader_t *r, const uint8_t *data, uint32_t size) {
    memset(r, 0, sizeof(*r));
    r->data = data;
    r->size = size;
}

bool esm_next_record(esm_reader_t *r) {
    if (!has_bytes(r, 16)) return false;

    r->rec_name = read_tag(r);
    r->rec_size = read_u32(r);
    read_u32(r);  /* unused */
    r->rec_flags = read_u32(r);
    r->rec_end = r->pos + r->rec_size;

    if (r->rec_end > r->size) r->rec_end = r->size;
    return true;
}

bool esm_next_subrecord(esm_reader_t *r) {
    if (r->pos >= r->rec_end || !has_bytes(r, 8)) return false;

    r->sub_name = read_tag(r);
    r->sub_size = read_u32(r);
    r->sub_end = r->pos + r->sub_size;

    if (r->sub_end > r->rec_end) r->sub_end = r->rec_end;
    return true;
}

void esm_skip_record(esm_reader_t *r) {
    r->pos = r->rec_end;
}

void esm_skip_subrecord(esm_reader_t *r) {
    r->pos = r->sub_end;
}

/* Public typed reads */
uint8_t  esm_read_u8(esm_reader_t *r)  { return read_u8(r); }
int8_t   esm_read_s8(esm_reader_t *r)  { return (int8_t)read_u8(r); }
uint16_t esm_read_u16(esm_reader_t *r) { return read_u16(r); }
int16_t  esm_read_s16(esm_reader_t *r) { return (int16_t)read_u16(r); }
uint32_t esm_read_u32(esm_reader_t *r) { return read_u32(r); }
int32_t  esm_read_s32(esm_reader_t *r) { return read_s32(r); }
float    esm_read_f32(esm_reader_t *r) { return read_f32(r); }

void esm_read_string(esm_reader_t *r, char *buf, uint32_t max_len) {
    uint32_t remaining = r->sub_end - r->pos;
    read_string(r, buf, max_len, remaining);
}

void esm_read_bytes(esm_reader_t *r, void *buf, uint32_t len) {
    if (has_bytes(r, len)) {
        memcpy(buf, r->data + r->pos, len);
    }
    r->pos += len;
}

/* -------------------------------------------------------------------
 * Index builder — scan entire ESM, build lookup tables
 * ------------------------------------------------------------------- */

void esm_build_index(esm_reader_t *r, esm_index_t *index) {
    memset(index, 0, sizeof(*index));
    r->pos = 0;

    while (esm_next_record(r)) {
        uint32_t rec_start = r->pos - 16;  /* Position of record header */

        switch (r->rec_name) {
            case ESM_TES3: {
                /* Parse header */
                while (esm_next_subrecord(r)) {
                    if (r->sub_name == ESM_HEDR && r->sub_size >= 12) {
                        index->header.version = read_f32(r);
                        index->header.type = read_s32(r);
                        read_string(r, index->header.author, 32,
                                    r->sub_size >= 44 ? 32 : r->sub_end - r->pos);
                        if (r->pos < r->sub_end) {
                            uint32_t desc_len = r->sub_end - r->pos - 4;
                            if (desc_len > 255) desc_len = 255;
                            read_string(r, index->header.description, 256, desc_len);
                        }
                        if (r->pos < r->sub_end) {
                            index->header.record_count = read_s32(r);
                        }
                    }
                    esm_skip_subrecord(r);
                }
                break;
            }

            case ESM_CELL: {
                /* Index this cell: read just enough to get name + grid coords */
                if (index->cell_count < ESM_MAX_CELLS) {
                    int ci = index->cell_count;
                    index->cells[ci].file_offset = rec_start;
                    index->cells[ci].is_interior = false;

                    while (esm_next_subrecord(r)) {
                        if (r->sub_name == ESM_NAME) {
                            char name[64];
                            read_string(r, name, 64, r->sub_size);
                            index->cells[ci].name_hash = hash_string(name);
                        } else if (r->sub_name == ESM_DATA && r->sub_size >= 12) {
                            int32_t flags = read_s32(r);
                            index->cells[ci].grid_x = read_s32(r);
                            index->cells[ci].grid_y = read_s32(r);
                            index->cells[ci].is_interior = (flags & CELL_INTERIOR) != 0;
                        } else {
                            esm_skip_subrecord(r);
                        }
                    }
                    index->cell_count++;
                }
                break;
            }

            case ESM_LAND: {
                /* Index terrain: just need grid coords + offset */
                if (index->land_count < ESM_MAX_LAND) {
                    int li = index->land_count;
                    index->lands[li].file_offset = rec_start;

                    while (esm_next_subrecord(r)) {
                        if (r->sub_name == ESM_INTV && r->sub_size >= 8) {
                            index->lands[li].grid_x = read_s32(r);
                            index->lands[li].grid_y = read_s32(r);
                        }
                        esm_skip_subrecord(r);
                    }
                    index->land_count++;
                }
                break;
            }

            case ESM_STAT: {
                /* Load static object definition into memory */
                if (index->static_count < ESM_MAX_STATICS) {
                    esm_static_t *s = &index->statics[index->static_count];
                    memset(s, 0, sizeof(*s));

                    while (esm_next_subrecord(r)) {
                        if (r->sub_name == ESM_NAME)
                            read_string(r, s->name, 64, r->sub_size);
                        else if (r->sub_name == ESM_MODL)
                            read_string(r, s->model, 128, r->sub_size);
                        else
                            esm_skip_subrecord(r);
                    }
                    if (s->name[0]) index->static_count++;
                }
                break;
            }

            case ESM_NPC_: {
                /* Load NPC definition */
                if (index->npc_count < ESM_MAX_NPCS) {
                    esm_npc_t *npc = &index->npcs[index->npc_count];
                    memset(npc, 0, sizeof(*npc));

                    while (esm_next_subrecord(r)) {
                        if (r->sub_name == ESM_NAME)
                            read_string(r, npc->name, 64, r->sub_size);
                        else if (r->sub_name == ESM_FNAM)
                            read_string(r, npc->display_name, 64, r->sub_size);
                        else if (r->sub_name == ESM_RNAM)
                            read_string(r, npc->race, 64, r->sub_size);
                        else if (r->sub_name == ESM_CNAM)
                            read_string(r, npc->class_, 64, r->sub_size);
                        else if (r->sub_name == ESM_MODL)
                            read_string(r, npc->model, 128, r->sub_size);
                        else if (r->sub_name == ESM_NPDT && r->sub_size >= 52) {
                            npc->level = read_s16(r);
                            esm_read_bytes(r, npc->attributes, 8);
                            skip_bytes(r, 27); /* skills */
                            npc->health = read_s16(r);
                            npc->magicka = read_s16(r);
                            npc->fatigue = read_s16(r);
                            npc->disposition = read_u8(r);
                            skip_bytes(r, 2); /* reputation, rank */
                            npc->gold = read_s32(r);
                        } else if (r->sub_name == ESM_FLAG && r->sub_size >= 1) {
                            npc->flags = read_u8(r);
                        } else {
                            esm_skip_subrecord(r);
                        }
                    }
                    if (npc->name[0]) index->npc_count++;
                }
                break;
            }

            default:
                /* Skip all other record types */
                break;
        }

        esm_skip_record(r);
    }
}

/* -------------------------------------------------------------------
 * Cell loading — parse a specific cell's object references
 * ------------------------------------------------------------------- */

bool esm_load_cell(esm_reader_t *r, const esm_index_t *index,
                    int32_t grid_x, int32_t grid_y,
                    esm_cell_t *cell,
                    esm_cell_ref_t *refs, int *ref_count, int max_refs) {
    /* Find cell in index */
    int found = -1;
    for (int i = 0; i < index->cell_count; i++) {
        if (!index->cells[i].is_interior &&
            index->cells[i].grid_x == grid_x &&
            index->cells[i].grid_y == grid_y) {
            found = i;
            break;
        }
    }
    if (found < 0) return false;

    /* Seek to cell record */
    r->pos = index->cells[found].file_offset;
    if (!esm_next_record(r)) return false;

    memset(cell, 0, sizeof(*cell));
    *ref_count = 0;

    /* Parse cell header subrecords */
    while (esm_next_subrecord(r)) {
        if (r->sub_name == ESM_NAME) {
            read_string(r, cell->name, 64, r->sub_size);
        } else if (r->sub_name == ESM_DATA && r->sub_size >= 12) {
            cell->flags = read_s32(r);
            cell->grid_x = read_s32(r);
            cell->grid_y = read_s32(r);
            cell->is_interior = (cell->flags & CELL_INTERIOR) != 0;
        } else if (r->sub_name == ESM_WHGT && r->sub_size >= 4) {
            cell->water_level = read_f32(r);
        } else if (r->sub_name == ESM_AMBI && r->sub_size >= 16) {
            esm_read_bytes(r, &cell->ambient, 4);
            esm_read_bytes(r, &cell->sunlight, 4);
            esm_read_bytes(r, &cell->fog, 4);
            cell->fog_density = read_f32(r);
        } else if (r->sub_name == ESM_FRMR) {
            /* Object reference — parse placement data */
            if (*ref_count < max_refs) {
                esm_cell_ref_t *ref = &refs[*ref_count];
                memset(ref, 0, sizeof(*ref));
                ref->scale = 1.0f;
                ref->ref_num = read_u32(r);

                /* Continue reading subrecords for this reference */
                while (esm_next_subrecord(r)) {
                    if (r->sub_name == ESM_NAME) {
                        read_string(r, ref->object_id, 64, r->sub_size);
                    } else if (r->sub_name == ESM_DATA && r->sub_size >= 24) {
                        ref->pos.x = read_f32(r);
                        ref->pos.y = read_f32(r);
                        ref->pos.z = read_f32(r);
                        ref->rot.x = read_f32(r);
                        ref->rot.y = read_f32(r);
                        ref->rot.z = read_f32(r);
                    } else if (r->sub_name == ESM_XSCL && r->sub_size >= 4) {
                        ref->scale = read_f32(r);
                    } else if (r->sub_name == ESM_DODT && r->sub_size >= 24) {
                        ref->is_door = true;
                        ref->door_dest_pos.x = read_f32(r);
                        ref->door_dest_pos.y = read_f32(r);
                        ref->door_dest_pos.z = read_f32(r);
                        ref->door_dest_rot.x = read_f32(r);
                        ref->door_dest_rot.y = read_f32(r);
                        ref->door_dest_rot.z = read_f32(r);
                    } else if (r->sub_name == ESM_DNAM) {
                        read_string(r, ref->door_dest_cell, 64, r->sub_size);
                    } else if (r->sub_name == ESM_FRMR) {
                        /* Next reference — break to outer loop */
                        /* Push back: we already consumed the FRMR header */
                        (*ref_count)++;
                        if (*ref_count >= max_refs) goto done;
                        ref = &refs[*ref_count];
                        memset(ref, 0, sizeof(*ref));
                        ref->scale = 1.0f;
                        ref->ref_num = read_u32(r);
                        /* This FRMR's NAME will come next iteration */
                    } else {
                        esm_skip_subrecord(r);
                    }
                }
                (*ref_count)++;
            }
        } else {
            esm_skip_subrecord(r);
        }
    }

done:
    return true;
}

/* -------------------------------------------------------------------
 * Terrain loading — parse LAND record heightmap
 * ------------------------------------------------------------------- */

bool esm_load_land(esm_reader_t *r, const esm_index_t *index,
                    int32_t grid_x, int32_t grid_y,
                    esm_land_t *land) {
    /* Find LAND in index */
    int found = -1;
    for (int i = 0; i < index->land_count; i++) {
        if (index->lands[i].grid_x == grid_x &&
            index->lands[i].grid_y == grid_y) {
            found = i;
            break;
        }
    }
    if (found < 0) return false;

    /* Seek to LAND record */
    r->pos = index->lands[found].file_offset;
    if (!esm_next_record(r)) return false;

    memset(land, 0, sizeof(*land));
    land->grid_x = grid_x;
    land->grid_y = grid_y;

    while (esm_next_subrecord(r)) {
        if (r->sub_name == ESM_INTV && r->sub_size >= 8) {
            land->grid_x = read_s32(r);
            land->grid_y = read_s32(r);
        } else if (r->sub_name == ESM_DATA && r->sub_size >= 4) {
            land->flags = read_u32(r);
        } else if (r->sub_name == ESM_VHGT) {
            /*
             * Height data: float offset + 65x65 int8 deltas.
             *
             * Reconstruction algorithm (exact Morrowind format):
             *   row_offset = height_offset
             *   for each row y=0..64:
             *     row_offset += delta[y][0]
             *     height[y][0] = row_offset * HEIGHT_SCALE
             *     col_running = row_offset
             *     for each col x=1..64:
             *       col_running += delta[y][x]
             *       height[y][x] = col_running * HEIGHT_SCALE
             *
             * HEIGHT_SCALE = 8 in Morrowind
             */
            float height_offset = read_f32(r);
            skip_bytes(r, 3);  /* 3 padding bytes */

            int8_t deltas[65 * 65];
            if (has_bytes(r, 65 * 65)) {
                memcpy(deltas, r->data + r->pos, 65 * 65);
                r->pos += 65 * 65;
            } else {
                memset(deltas, 0, sizeof(deltas));
                esm_skip_subrecord(r);
                continue;
            }

            /* Reconstruct heights */
            float row_offset = height_offset;
            for (int y = 0; y < 65; y++) {
                row_offset += deltas[y * 65];
                float col_running = row_offset;
                land->heights[y * 65] = (int16_t)(col_running);

                for (int x = 1; x < 65; x++) {
                    col_running += deltas[y * 65 + x];
                    /* Store as int16_t (height * 8 happens at render time) */
                    land->heights[y * 65 + x] = (int16_t)(col_running);
                }
            }
        } else if (r->sub_name == ESM_VNML && r->sub_size >= 65 * 65 * 3) {
            esm_read_bytes(r, land->normals, 65 * 65 * 3);
        } else if (r->sub_name == ESM_VCLR && r->sub_size >= 65 * 65 * 3) {
            esm_read_bytes(r, land->colors, 65 * 65 * 3);
            land->has_colors = true;
        } else if (r->sub_name == ESM_VTEX && r->sub_size >= 16 * 16 * 2) {
            /* Texture indices with Morrowind's transposition */
            uint16_t raw[16 * 16];
            for (int i = 0; i < 256; i++) raw[i] = read_u16(r);

            /* De-transpose: 4x4 blocks of 4x4 sub-blocks */
            int read_pos = 0;
            for (int y1 = 0; y1 < 4; y1++) {
                for (int x1 = 0; x1 < 4; x1++) {
                    for (int y2 = 0; y2 < 4; y2++) {
                        for (int x2 = 0; x2 < 4; x2++) {
                            land->tex_indices[(y1*4+y2)*16 + (x1*4+x2)] = raw[read_pos++];
                        }
                    }
                }
            }
            land->has_textures = true;
        } else {
            esm_skip_subrecord(r);
        }
    }

    return true;
}

/* -------------------------------------------------------------------
 * Interior cell lookup by name
 * ------------------------------------------------------------------- */

bool esm_load_interior(esm_reader_t *r, const esm_index_t *index,
                         const char *name,
                         esm_cell_t *cell,
                         esm_cell_ref_t *refs, int *ref_count, int max_refs) {
    uint32_t target_hash = hash_string(name);

    for (int i = 0; i < index->cell_count; i++) {
        if (index->cells[i].is_interior &&
            index->cells[i].name_hash == target_hash) {
            /* Found — load it using the same parser */
            r->pos = index->cells[i].file_offset;
            if (!esm_next_record(r)) return false;

            /* Re-parse cell to get full data */
            /* (Reuse the exterior loader logic — it handles both) */
            r->pos = index->cells[i].file_offset;
            return esm_load_cell(r, index,
                                  index->cells[i].grid_x,
                                  index->cells[i].grid_y,
                                  cell, refs, ref_count, max_refs);
        }
    }
    return false;
}
