/*
 * TRX-N64: Level loader bridge implementation
 *
 * Loads TR1 .PHD level data via asset streaming and provides
 * a sequential reader interface compatible with TRX's section loaders.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "n64_level_loader.h"
#include "../render/n64_render.h"
#include "asset_manager.h"

#include <string.h>

/* External: the global asset manager instance */
extern asset_manager_t g_asset_mgr;

/* External: the global renderer instance */
extern n64_renderer_t g_renderer;

/* Store the handle for release later */
static asset_handle_t level_handle;

/* -------------------------------------------------------------------
 * Level loading
 * ------------------------------------------------------------------- */

bool n64_level_begin(n64_level_state_t *state, uint32_t level_num) {
    memset(state, 0, sizeof(*state));
    state->level_num = level_num;
    state->level_asset_id = LEVEL_ASSET_ID(level_num);

    /* Pin the level data in cache — it must stay resident during gameplay */
    level_handle = asset_acquire(&g_asset_mgr, state->level_asset_id);
    if (!asset_handle_valid(level_handle)) {
        return false;
    }

    uint32_t size = 0;
    state->level_data = (const uint8_t *)asset_resolve_with_size(
        &g_asset_mgr, level_handle, &size);
    if (!state->level_data) {
        asset_release(&g_asset_mgr, level_handle);
        return false;
    }

    state->level_size = size;
    state->read_pos = 0;
    state->level_handle = (void *)1;  /* Non-null marker */

    return true;
}

/* -------------------------------------------------------------------
 * Sequential reader (replaces fread for TRX section loaders)
 * ------------------------------------------------------------------- */

int32_t n64_level_read(n64_level_state_t *state, void *buf, uint32_t size) {
    if (!state->level_data) return -1;

    uint32_t remaining = state->level_size - state->read_pos;
    if (size > remaining) size = remaining;
    if (size == 0) return 0;

    memcpy(buf, state->level_data + state->read_pos, size);
    state->read_pos += size;
    return (int32_t)size;
}

void n64_level_skip(n64_level_state_t *state, uint32_t bytes) {
    state->read_pos += bytes;
    if (state->read_pos > state->level_size) {
        state->read_pos = state->level_size;
    }
}

/* Typed readers */
int8_t n64_level_read_s8(n64_level_state_t *s) {
    int8_t v; n64_level_read(s, &v, 1); return v;
}
uint8_t n64_level_read_u8(n64_level_state_t *s) {
    uint8_t v; n64_level_read(s, &v, 1); return v;
}
int16_t n64_level_read_s16(n64_level_state_t *s) {
    int16_t v; n64_level_read(s, &v, 2); return v;
}
uint16_t n64_level_read_u16(n64_level_state_t *s) {
    uint16_t v; n64_level_read(s, &v, 2); return v;
}
int32_t n64_level_read_s32(n64_level_state_t *s) {
    int32_t v; n64_level_read(s, &v, 4); return v;
}
uint32_t n64_level_read_u32(n64_level_state_t *s) {
    uint32_t v; n64_level_read(s, &v, 4); return v;
}

/* -------------------------------------------------------------------
 * Texture page loading
 * ------------------------------------------------------------------- */

bool n64_level_load_textures(n64_level_state_t *state) {
    /* Texture pages are parsed from the level data by tr_level_parse.
     * No separate loading needed. */
    return true;
}

/* -------------------------------------------------------------------
 * Cleanup
 * ------------------------------------------------------------------- */

void n64_level_end(n64_level_state_t *state) {
    if (state->level_handle) {
        asset_release(&g_asset_mgr, level_handle);
    }
    memset(state, 0, sizeof(*state));
}
