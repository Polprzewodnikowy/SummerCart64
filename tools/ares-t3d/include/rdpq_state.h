/*
 * rdpq_state.h — Track RDP render state from intercepted commands
 *
 * rdpq (libdragon's RDP overlay) generates standard RDP commands.
 * We intercept those at the RDP command submission point and
 * track the state needed to configure the GL renderer.
 *
 * This does NOT interpret rdpq overlay commands directly — it
 * reads the standard 8-byte RDP commands that rdpq generates.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

/* RDP command opcodes (6-bit, upper byte of 64-bit command word) */
#define RDP_CMD_SET_FILL_COLOR    0x37
#define RDP_CMD_SET_FOG_COLOR     0x38
#define RDP_CMD_SET_BLEND_COLOR   0x39
#define RDP_CMD_SET_PRIM_COLOR    0x3A
#define RDP_CMD_SET_ENV_COLOR     0x3B
#define RDP_CMD_SET_COMBINE_MODE  0x3C
#define RDP_CMD_SET_TEX_IMAGE     0x3D
#define RDP_CMD_SET_Z_IMAGE       0x3E
#define RDP_CMD_SET_COLOR_IMAGE   0x3F
#define RDP_CMD_SET_OTHER_MODES   0x2F
#define RDP_CMD_SET_SCISSOR       0x2D
#define RDP_CMD_SET_TILE          0x35
#define RDP_CMD_LOAD_BLOCK        0x33
#define RDP_CMD_LOAD_TILE         0x34
#define RDP_CMD_SYNC_FULL         0x29
#define RDP_CMD_SYNC_PIPE         0x27
#define RDP_CMD_SYNC_TILE         0x28
#define RDP_CMD_SYNC_LOAD         0x31

/* Tracked RDP state */
typedef struct {
    /* Color registers */
    uint8_t fill_color[4];   /* RGBA */
    uint8_t fog_color[4];
    uint8_t blend_color[4];
    uint8_t prim_color[4];
    uint8_t env_color[4];

    /* Combine mode (raw 64-bit word) */
    uint64_t combine_mode;

    /* Other modes (raw 64-bit word) */
    uint64_t other_modes;

    /* Derived from other_modes */
    bool depth_compare;
    bool depth_update;
    bool alpha_compare;
    int cycle_type;          /* 0=1cycle, 1=2cycle, 2=copy, 3=fill */

    /* Scissor */
    int scissor_x0, scissor_y0;
    int scissor_x1, scissor_y1;

    /* Framebuffer */
    uint32_t color_image_addr;
    int color_image_width;
    int color_image_format;  /* 0=RGBA, 1=YUV, 2=CI, 3=IA, 4=I */
    int color_image_size;    /* 0=4bit, 1=8bit, 2=16bit, 3=32bit */

    uint32_t z_image_addr;

    /* Texture */
    uint32_t tex_image_addr;

    /* Sync tracking */
    bool sync_full_pending;

    /* Stats */
    uint32_t state_changes;
    uint32_t sync_fulls;
} RDPQState;

/* Initialize RDP state tracker */
void rdpq_state_init(RDPQState *s);

/* Process a standard 8-byte RDP command.
 * cmd_hi = upper 32 bits, cmd_lo = lower 32 bits.
 * Returns true if state was updated. */
bool rdpq_state_process(RDPQState *s, uint32_t cmd_hi, uint32_t cmd_lo);

/* Get the RDP command name for a 6-bit opcode */
const char *rdpq_cmd_name(int opcode);
