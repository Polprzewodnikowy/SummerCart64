/*
 * overlay_sizes.h — Command size tables for known rspq overlays
 *
 * Enables correct mixed-stream walking by knowing how many bytes
 * to skip past each overlay's commands. Without this, scanning
 * a multiplexed command buffer word-by-word produces garbage because
 * interior words of large commands (e.g., rdpq triangles at 176 bytes)
 * accidentally match T3D command bit patterns.
 *
 * Sizes are in BYTES (not words). Divide by 4 for word count.
 * Source: rsp_rdpq.S RSPQ_DefineCommand table.
 */

#pragma once

#include <stdint.h>

/* rdpq overlay: 64 commands (cmd_id 0x00-0x3F)
 * Maps to RDP commands 0xC0-0xFF.
 * Sizes from rsp_rdpq.S RSPQ_DefineCommand directives. */
static const uint8_t rdpq_cmd_sizes[64] = {
    /* 0x00 */ 8,    /* NOOP (passthrough 8) */
    /* 0x01 */ 8,    /* SET_LOOKUP_ADDRESS */
    /* 0x02 */ 8,    /* FILL_RECTANGLE_EX */
    /* 0x03 */ 16,   /* CLEAR_ZBUFFER */
    /* 0x04 */ 16,   /* RESET_RENDER_MODE (set_mode_standard) */
    /* 0x05 */ 8,    /* SET_COMBINE_MODE_2PASS */
    /* 0x06 */ 4,    /* PUSH_RENDER_MODE */
    /* 0x07 */ 4,    /* POP_RENDER_MODE */
    /* 0x08 */ 32,   /* TRI (Filled) */
    /* 0x09 */ 48,   /* TRI_ZBUF (Filled ZBuffered) */
    /* 0x0A */ 96,   /* TRI_TEX (Textured) */
    /* 0x0B */ 112,  /* TRI_TEX_ZBUF (Textured ZBuffered) */
    /* 0x0C */ 96,   /* TRI_SHADE (Shaded) */
    /* 0x0D */ 112,  /* TRI_SHADE_ZBUF (Shaded ZBuffered) */
    /* 0x0E */ 160,  /* TRI_SHADE_TEX (Shaded Textured) */
    /* 0x0F */ 176,  /* TRI_SHADE_TEX_ZBUF (Shaded Textured ZBuffered) */
    /* 0x10 */ 16,   /* TEXTURE_RECTANGLE_EX */
    /* 0x11 */ 4,    /* SET_DEBUG_MODE */
    /* 0x12 */ 8,    /* SET_SCISSOR_EX */
    /* 0x13 */ 8,    /* SET_PRIM_COLOR_COMPONENT */
    /* 0x14 */ 12,   /* MODIFY_OTHER_MODES */
    /* 0x15 */ 8,    /* (noop) */
    /* 0x16 */ 8,    /* SET_FILL_COLOR_32 */
    /* 0x17 */ 8,    /* (noop) */
    /* 0x18 */ 8,    /* SET_BLENDING_MODE */
    /* 0x19 */ 8,    /* SET_FOG_MODE */
    /* 0x1A */ 4,    /* WAIT_IDLE */
    /* 0x1B */ 16,   /* SET_COMBINE_MODE_1PASS */
    /* 0x1C */ 4,    /* AUTOTMEM_SET_ADDR */
    /* 0x1D */ 8,    /* AUTOTMEM_SET_TILE */
    /* 0x1E */ 4,    /* TRIANGLE (RSP-assembled) */
    /* 0x1F */ 28,   /* TRIANGLE_DATA */
    /* 0x20 */ 8,    /* (noop) */
    /* 0x21 */ 8,    /* (noop) */
    /* 0x22 */ 8,    /* (noop) */
    /* 0x23 */ 8,    /* (noop) */
    /* 0x24 */ 16,   /* TEXTURE_RECTANGLE (passthrough 16) */
    /* 0x25 */ 16,   /* TEXTURE_RECTANGLE_FLIP (passthrough 16) */
    /* 0x26 */ 8,    /* SYNC_LOAD */
    /* 0x27 */ 8,    /* SYNC_PIPE */
    /* 0x28 */ 8,    /* SYNC_TILE */
    /* 0x29 */ 8,    /* SYNC_FULL */
    /* 0x2A */ 8,    /* SET_KEY_GB */
    /* 0x2B */ 8,    /* SET_KEY_R */
    /* 0x2C */ 8,    /* SET_CONVERT */
    /* 0x2D */ 8,    /* SET_SCISSOR */
    /* 0x2E */ 8,    /* SET_PRIM_DEPTH */
    /* 0x2F */ 8,    /* SET_OTHER_MODES */
    /* 0x30 */ 8,    /* LOAD_TLUT */
    /* 0x31 */ 8,    /* RDPQ_DEBUG */
    /* 0x32 */ 8,    /* SET_TILE_SIZE */
    /* 0x33 */ 8,    /* LOAD_BLOCK */
    /* 0x34 */ 8,    /* LOAD_TILE */
    /* 0x35 */ 8,    /* SET_TILE */
    /* 0x36 */ 8,    /* FILL_RECTANGLE */
    /* 0x37 */ 8,    /* SET_FILL_COLOR */
    /* 0x38 */ 8,    /* SET_FOG_COLOR */
    /* 0x39 */ 8,    /* SET_BLEND_COLOR */
    /* 0x3A */ 8,    /* SET_PRIM_COLOR */
    /* 0x3B */ 8,    /* SET_ENV_COLOR */
    /* 0x3C */ 8,    /* SET_COMBINE_MODE */
    /* 0x3D */ 8,    /* SET_TEXTURE_IMAGE */
    /* 0x3E */ 8,    /* SET_Z_IMAGE */
    /* 0x3F */ 8,    /* SET_COLOR_IMAGE */
};

/* T3D overlay: 13 commands (cmd_id 0x0-0xC)
 * Sizes in bytes from t3d.c rspq_write() calls. */
static const uint8_t t3d_cmd_sizes_bytes[13] = {
    /* 0x0 TRI_DRAW */      8,
    /* 0x1 SCREEN_SIZE */   20,
    /* 0x2 MATRIX_STACK */  8,
    /* 0x3 SET_WORD */      8,
    /* 0x4 VERT_LOAD */     12,
    /* 0x5 LIGHT_SET */     16,
    /* 0x6 DRAWFLAGS */     8,
    /* 0x7 PROJ_SET */      4,
    /* 0x8 FOG_RANGE */     8,
    /* 0x9 FOG_STATE */     4,
    /* 0xA TRI_SYNC */      4,
    /* 0xB TRI_STRIP */     8,
    /* 0xC TRI_SEQ */       8,
};

/* Get command size in bytes for a known overlay.
 * Returns 0 if overlay/cmd is unknown. */
static inline int overlay_cmd_size(int ovl_id, int cmd_id,
                                   int rdpq_ovl_id, int t3d_ovl_id) {
    if (ovl_id == rdpq_ovl_id && cmd_id < 64) {
        return rdpq_cmd_sizes[cmd_id];
    }
    if (ovl_id == t3d_ovl_id && cmd_id < 13) {
        return t3d_cmd_sizes_bytes[cmd_id];
    }
    return 0; /* unknown overlay */
}
