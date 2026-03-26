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
#include "../../asset_streaming/src/asset_manager.h"

#include <string.h>

/* External: the global asset manager instance */
extern asset_manager_t g_asset_mgr;

/* External: the global renderer instance */
extern n64_renderer_t g_renderer;

/* -------------------------------------------------------------------
 * Level loading
 * ------------------------------------------------------------------- */

bool n64_level_begin(n64_level_state_t *state, uint32_t level_num) {
    memset(state, 0, sizeof(*state));
    state->level_num = level_num;
    state->level_asset_id = LEVEL_ASSET_ID(level_num);

    /* Pin the level data in cache — it must stay resident during gameplay */
    asset_handle_t handle = asset_acquire(&g_asset_mgr, state->level_asset_id);
    if (!asset_handle_valid(handle)) {
        return false;
    }

    state->level_handle = (void *)(uintptr_t)handle.slot_index;
    state->level_data = asset_resolve(&g_asset_mgr, handle);
    if (!state->level_data) {
        asset_release(&g_asset_mgr, handle);
        return false;
    }

    /* Get level size from cache slot */
    state->level_size = asset_load_with_size(&g_asset_mgr,
                                              state->level_asset_id,
                                              NULL);
    state->read_pos = 0;

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
 *
 * TR1 stores texture pages at a known offset in the .PHD file.
 * We pre-split them into separate assets during packaging so
 * they can be streamed independently (not all pages needed at once).
 *
 * If texture pages are embedded in the level asset (simple mode),
 * we extract them from the level data directly.
 * ------------------------------------------------------------------- */

bool n64_level_load_textures(n64_level_state_t *state) {
    /*
     * Two strategies:
     *
     * 1. Embedded: texture pages are at the end of the level asset.
     *    Parse them from level_data. Simpler but uses more cache RAM.
     *
     * 2. Split: texture pages are separate assets (TEXPAGE_ASSET_ID).
     *    Load each page on demand. Uses less RAM, better for streaming.
     *
     * Start with strategy 1 (embedded) for initial bring-up.
     * The TRX level format puts texture pages last, and the section
     * reader has already advanced past them during load.
     */

    /*
     * The TRX loader reads texture pages as:
     *   - tex_page_count pages of 256×256 8-bit indexed pixels
     *   - palette: 256 entries of RGB888
     *
     * These are stored in the LEVEL_CONTEXT_INFO by TRX.
     * For the N64, we intercept this and convert to N64 tile format.
     *
     * For the initial implementation, the texture page data and palette
     * are passed to us after TRX's section readers have extracted them.
     */

    /* Placeholder: actual wiring happens when we integrate TRX sections */
    return true;
}

/* -------------------------------------------------------------------
 * Cleanup
 * ------------------------------------------------------------------- */

void n64_level_end(n64_level_state_t *state) {
    if (state->level_handle) {
        asset_handle_t handle = {
            .asset_id = state->level_asset_id,
            .slot_index = (uint32_t)(uintptr_t)state->level_handle,
            .generation = 0,  /* TODO: store actual generation */
        };
        asset_release(&g_asset_mgr, handle);
    }
    memset(state, 0, sizeof(*state));
}
