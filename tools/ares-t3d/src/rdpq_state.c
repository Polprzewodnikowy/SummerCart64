/*
 * rdpq_state.c — RDP state tracker
 *
 * Parses standard 8-byte RDP commands and maintains render state.
 * This state is used by the GL renderer to configure depth test,
 * blending, scissor, and color registers.
 */

#include "rdpq_state.h"
#include <string.h>

void rdpq_state_init(RDPQState *s) {
    memset(s, 0, sizeof(*s));
    s->color_image_size = 2; /* 16-bit default */
    s->color_image_width = 320;
    s->scissor_x1 = 320;
    s->scissor_y1 = 240;
}

static void unpack_rgba32(uint8_t *out, uint32_t packed) {
    out[0] = (packed >> 24) & 0xFF;
    out[1] = (packed >> 16) & 0xFF;
    out[2] = (packed >>  8) & 0xFF;
    out[3] = (packed >>  0) & 0xFF;
}

bool rdpq_state_process(RDPQState *s, uint32_t cmd_hi, uint32_t cmd_lo) {
    int opcode = (cmd_hi >> 24) & 0x3F;

    switch (opcode) {
    case RDP_CMD_SET_FILL_COLOR:
        unpack_rgba32(s->fill_color, cmd_lo);
        s->state_changes++;
        return true;

    case RDP_CMD_SET_FOG_COLOR:
        unpack_rgba32(s->fog_color, cmd_lo);
        s->state_changes++;
        return true;

    case RDP_CMD_SET_BLEND_COLOR:
        unpack_rgba32(s->blend_color, cmd_lo);
        s->state_changes++;
        return true;

    case RDP_CMD_SET_PRIM_COLOR:
        /* cmd_hi bits 8-12: prim min level, bits 0-4: prim level frac */
        unpack_rgba32(s->prim_color, cmd_lo);
        s->state_changes++;
        return true;

    case RDP_CMD_SET_ENV_COLOR:
        unpack_rgba32(s->env_color, cmd_lo);
        s->state_changes++;
        return true;

    case RDP_CMD_SET_COMBINE_MODE:
        s->combine_mode = ((uint64_t)cmd_hi << 32) | cmd_lo;
        s->state_changes++;
        return true;

    case RDP_CMD_SET_OTHER_MODES: {
        s->other_modes = ((uint64_t)cmd_hi << 32) | cmd_lo;
        /* Extract key bits from other_modes */
        s->cycle_type    = (cmd_hi >> 20) & 3;
        s->depth_compare = !!(cmd_lo & (1 << 4));  /* Z_CMP */
        s->depth_update  = !!(cmd_lo & (1 << 5));  /* Z_UPD */
        s->alpha_compare = !!(cmd_lo & (1 << 0));  /* ALPHA_CMP_EN */
        s->state_changes++;
        return true;
    }

    case RDP_CMD_SET_SCISSOR:
        /* cmd_hi: XH(12 bits, 10.2 fixed), YH(12 bits, 10.2 fixed)
         * cmd_lo: XL(12 bits, 10.2 fixed), YL(12 bits, 10.2 fixed) */
        s->scissor_x0 = (cmd_hi >> 12) & 0xFFF;
        s->scissor_y0 = cmd_hi & 0xFFF;
        s->scissor_x1 = (cmd_lo >> 12) & 0xFFF;
        s->scissor_y1 = cmd_lo & 0xFFF;
        /* Convert from 10.2 to integer */
        s->scissor_x0 >>= 2;
        s->scissor_y0 >>= 2;
        s->scissor_x1 >>= 2;
        s->scissor_y1 >>= 2;
        s->state_changes++;
        return true;

    case RDP_CMD_SET_COLOR_IMAGE:
        s->color_image_format = (cmd_hi >> 21) & 7;
        s->color_image_size   = (cmd_hi >> 19) & 3;
        s->color_image_width  = (cmd_hi & 0x3FF) + 1;
        s->color_image_addr   = cmd_lo & 0x01FFFFFF;
        s->state_changes++;
        return true;

    case RDP_CMD_SET_Z_IMAGE:
        s->z_image_addr = cmd_lo & 0x01FFFFFF;
        s->state_changes++;
        return true;

    case RDP_CMD_SET_TEX_IMAGE:
        s->tex_image_addr = cmd_lo & 0x01FFFFFF;
        s->state_changes++;
        return true;

    case RDP_CMD_SYNC_FULL:
        s->sync_full_pending = true;
        s->sync_fulls++;
        return true;

    case RDP_CMD_SYNC_PIPE:
    case RDP_CMD_SYNC_TILE:
    case RDP_CMD_SYNC_LOAD:
        return true; /* consumed but no state change */

    default:
        return false; /* unknown/unhandled */
    }
}

const char *rdpq_cmd_name(int opcode) {
    switch (opcode) {
    case RDP_CMD_SET_FILL_COLOR:   return "SET_FILL_COLOR";
    case RDP_CMD_SET_FOG_COLOR:    return "SET_FOG_COLOR";
    case RDP_CMD_SET_BLEND_COLOR:  return "SET_BLEND_COLOR";
    case RDP_CMD_SET_PRIM_COLOR:   return "SET_PRIM_COLOR";
    case RDP_CMD_SET_ENV_COLOR:    return "SET_ENV_COLOR";
    case RDP_CMD_SET_COMBINE_MODE: return "SET_COMBINE_MODE";
    case RDP_CMD_SET_OTHER_MODES:  return "SET_OTHER_MODES";
    case RDP_CMD_SET_SCISSOR:      return "SET_SCISSOR";
    case RDP_CMD_SET_COLOR_IMAGE:  return "SET_COLOR_IMAGE";
    case RDP_CMD_SET_Z_IMAGE:      return "SET_Z_IMAGE";
    case RDP_CMD_SET_TEX_IMAGE:    return "SET_TEX_IMAGE";
    case RDP_CMD_SYNC_FULL:        return "SYNC_FULL";
    case RDP_CMD_SYNC_PIPE:        return "SYNC_PIPE";
    case RDP_CMD_SYNC_TILE:        return "SYNC_TILE";
    case RDP_CMD_SYNC_LOAD:        return "SYNC_LOAD";
    case RDP_CMD_SET_TILE:         return "SET_TILE";
    case RDP_CMD_LOAD_BLOCK:       return "LOAD_BLOCK";
    case RDP_CMD_LOAD_TILE:        return "LOAD_TILE";
    default: return "UNKNOWN";
    }
}
