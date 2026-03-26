/*
 * TRX-N64: Platform abstraction layer
 *
 * Replaces TRX's SDL2/desktop platform layer (src/trx/game/shell/)
 * with N64 + SC64 hardware interfaces.
 *
 * Provides:
 *   - Video initialization (VI setup)
 *   - Controller input (joybus)
 *   - Timing (CP0 Count register)
 *   - Memory management (fixed pools, no dynamic allocation)
 *   - File I/O → asset streaming from SD card
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TRX_N64_PLATFORM_H
#define TRX_N64_PLATFORM_H

#include <stdint.h>
#include <stdbool.h>

/* -------------------------------------------------------------------
 * Video / VI initialization
 * ------------------------------------------------------------------- */

typedef enum {
    N64_VIDEO_NTSC,     /* 320×240 @ 60Hz */
    N64_VIDEO_PAL,      /* 320×240 @ 50Hz */
} n64_video_mode_t;

void n64_video_init(n64_video_mode_t mode);
void n64_video_set_framebuffer(uint16_t *fb);
void n64_video_wait_vsync(void);

/* -------------------------------------------------------------------
 * Controller input
 *
 * Maps N64 controller to TR1 inputs:
 *   A → Action (grab, pickup, shoot)
 *   B → Jump
 *   Z → Draw weapons
 *   L/R → Sidestep left/right
 *   C-Up → Look up
 *   C-Down → Look down (duck in TR2+)
 *   C-Left/Right → unused / inventory
 *   Start → Pause / inventory
 *   D-Pad → Movement (alternative to analog)
 *   Analog → Movement
 * ------------------------------------------------------------------- */

typedef enum {
    TR_INPUT_UP         = (1 << 0),
    TR_INPUT_DOWN       = (1 << 1),
    TR_INPUT_LEFT       = (1 << 2),
    TR_INPUT_RIGHT      = (1 << 3),
    TR_INPUT_ACTION     = (1 << 4),   /* N64: A */
    TR_INPUT_JUMP       = (1 << 5),   /* N64: B */
    TR_INPUT_DRAW       = (1 << 6),   /* N64: Z */
    TR_INPUT_LOOK       = (1 << 7),   /* N64: R */
    TR_INPUT_ROLL       = (1 << 8),   /* N64: C-Down + C-Up */
    TR_INPUT_WALK       = (1 << 9),   /* N64: C-Left (walk modifier) */
    TR_INPUT_STEP_LEFT  = (1 << 10),  /* N64: L + Left */
    TR_INPUT_STEP_RIGHT = (1 << 11),  /* N64: L + Right */
    TR_INPUT_OPTION     = (1 << 12),  /* N64: Start */
    TR_INPUT_LOOK_UP    = (1 << 13),  /* N64: C-Up */
    TR_INPUT_LOOK_DOWN  = (1 << 14),  /* N64: C-Down */
    TR_INPUT_LOOK_LEFT  = (1 << 15),  /* N64: C-Left (in look mode) */
    TR_INPUT_LOOK_RIGHT = (1 << 16),  /* N64: C-Right (in look mode) */
    TR_INPUT_FLARE      = (1 << 17),  /* N64: unused in TR1 */
    TR_INPUT_PAUSE      = (1 << 18),  /* N64: Start */
} tr_input_t;

typedef struct {
    uint32_t buttons;       /* Bitmask of tr_input_t */
    uint32_t buttons_prev;  /* Previous frame's buttons */
    int8_t   analog_x;     /* -128..127, raw stick */
    int8_t   analog_y;     /* -128..127, raw stick */
    float    move_x;       /* -1.0..1.0, normalized + deadzone */
    float    move_y;       /* -1.0..1.0, normalized + deadzone */
} n64_input_state_t;

void n64_input_init(void);
void n64_input_poll(n64_input_state_t *state);

/* Pressed this frame (rising edge) */
static inline bool n64_input_pressed(const n64_input_state_t *s, tr_input_t btn) {
    return (s->buttons & btn) && !(s->buttons_prev & btn);
}

/* Held this frame */
static inline bool n64_input_held(const n64_input_state_t *s, tr_input_t btn) {
    return (s->buttons & btn) != 0;
}

/* -------------------------------------------------------------------
 * Timing
 * ------------------------------------------------------------------- */

#define N64_CPU_FREQ        93750000    /* 93.75 MHz */
#define N64_COUNT_FREQ      (N64_CPU_FREQ / 2)  /* CP0 Count at half CPU */
#define N64_TICKS_PER_MS    (N64_COUNT_FREQ / 1000)

uint32_t n64_timer_ticks(void);
uint32_t n64_timer_ms(void);
void     n64_timer_wait_ms(uint32_t ms);

/* -------------------------------------------------------------------
 * Memory pools
 *
 * No malloc on N64 — we use fixed pools from the linker script.
 * The asset streaming cache handles all large allocations.
 * Game logic gets a fixed-size work buffer.
 * ------------------------------------------------------------------- */

#define N64_GAME_HEAP_SIZE  (512 * 1024)  /* 512 KB for game logic */

void  n64_mem_init(void);
void *n64_mem_alloc(uint32_t size);   /* Bump allocator from game heap */
void  n64_mem_reset(void);            /* Reset bump pointer (per-level) */
uint32_t n64_mem_available(void);     /* Bytes remaining */

/* -------------------------------------------------------------------
 * File I/O → Asset streaming bridge
 *
 * TRX uses fopen/fread for level files. We replace this with
 * our asset streaming system reading from SD card.
 * ------------------------------------------------------------------- */

typedef struct {
    uint32_t asset_id;      /* Asset manager ID */
    const uint8_t *data;    /* Pointer to cached data */
    uint32_t size;          /* Total size */
    uint32_t pos;           /* Current read position */
} n64_file_t;

n64_file_t *n64_file_open(uint32_t asset_id);
int32_t     n64_file_read(n64_file_t *f, void *buf, uint32_t size);
int32_t     n64_file_seek(n64_file_t *f, int32_t offset, int whence);
uint32_t    n64_file_tell(n64_file_t *f);
void        n64_file_close(n64_file_t *f);

/* -------------------------------------------------------------------
 * Audio stub
 *
 * Audio is a later milestone. For now, stub out all audio calls.
 * Future: RSP-based PCM mixer for sound effects,
 *         streaming audio for music tracks.
 * ------------------------------------------------------------------- */

void n64_audio_init(void);
void n64_audio_play_sample(uint32_t sample_id, int16_t volume, int16_t pan);
void n64_audio_play_music(uint32_t track_id);
void n64_audio_stop_music(void);
void n64_audio_shutdown(void);

#endif /* TRX_N64_PLATFORM_H */
