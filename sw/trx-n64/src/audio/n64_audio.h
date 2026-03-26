/*
 * TRX-N64: Audio system — N64 AI + software PCM mixer
 *
 * TR1 audio: mono PCM samples at varying rates (11025-22050 Hz),
 * mixed with 3D positioning (distance attenuation + stereo panning).
 *
 * N64 approach:
 *   - Software mixer running on CPU (not RSP, for simplicity)
 *   - Mix into 16-bit stereo buffer at 22050 Hz
 *   - Double-buffered DMA to Audio Interface (AI)
 *   - Up to 16 concurrent sound channels
 *   - Music streamed from SD card via asset streaming
 *
 * The mixer runs during VBlank, producing one frame's worth of
 * audio (~735 samples at 22050 Hz / 30 fps) per game tick.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef N64_AUDIO_H
#define N64_AUDIO_H

#include <stdint.h>
#include <stdbool.h>

/* -------------------------------------------------------------------
 * Audio configuration
 * ------------------------------------------------------------------- */

#define AUDIO_SAMPLE_RATE   22050   /* Hz — good balance of quality vs CPU */
#define AUDIO_CHANNELS      2       /* Stereo output */
#define AUDIO_BIT_DEPTH     16      /* Bits per sample */
#define AUDIO_FPS           30      /* Game frame rate */
#define AUDIO_FRAME_SAMPLES (AUDIO_SAMPLE_RATE / AUDIO_FPS) /* ~735 samples */
#define AUDIO_BUF_SAMPLES   (AUDIO_FRAME_SAMPLES + 16)      /* Padded */
#define AUDIO_BUF_BYTES     (AUDIO_BUF_SAMPLES * AUDIO_CHANNELS * 2)

/* Max concurrent sound channels */
#define AUDIO_MAX_CHANNELS  16

/* Max concurrent music streams */
#define AUDIO_MAX_STREAMS   1

/* -------------------------------------------------------------------
 * N64 Audio Interface registers
 * ------------------------------------------------------------------- */

#ifndef AI_BASE
#define AI_BASE             0xA4500000
#endif
#define AI_DRAM_ADDR        (*(volatile uint32_t *)(AI_BASE + 0x00))
#define AI_LEN              (*(volatile uint32_t *)(AI_BASE + 0x04))
#define AI_CONTROL          (*(volatile uint32_t *)(AI_BASE + 0x08))
#define AI_STATUS           (*(volatile uint32_t *)(AI_BASE + 0x0C))
#define AI_DACRATE          (*(volatile uint32_t *)(AI_BASE + 0x10))
#define AI_BITRATE          (*(volatile uint32_t *)(AI_BASE + 0x14))

/* NTSC video clock for DAC rate calculation */
#define NTSC_CLOCK          48681812
#define PAL_CLOCK           49656530

/* AI status bits */
#define AI_STATUS_BUSY      (1 << 30)
#define AI_STATUS_FULL      (1 << 31)

/* -------------------------------------------------------------------
 * Sound channel (one active sound effect)
 * ------------------------------------------------------------------- */

typedef struct {
    /* Source PCM data */
    const int16_t *data;        /* Sample data pointer (in RDRAM) */
    uint32_t       length;      /* Total samples */
    uint32_t       position;    /* Current playback position (16.16 fixed) */
    uint32_t       rate;        /* Playback rate (16.16 fixed, 1.0 = source rate) */

    /* Volume and panning (0-255) */
    uint8_t        volume_l;    /* Left channel volume */
    uint8_t        volume_r;    /* Right channel volume */

    /* State */
    bool           active;
    bool           looping;
    int16_t        sample_id;   /* TR1 sample info index */
    int16_t        entity_id;   /* Entity that owns this sound (-1 = none) */
} audio_channel_t;

/* -------------------------------------------------------------------
 * Music stream (background music from SD card)
 * ------------------------------------------------------------------- */

typedef struct {
    uint32_t       asset_id;    /* Asset streaming ID for this track */
    const int16_t *data;        /* Decoded PCM data in cache */
    uint32_t       length;      /* Total samples */
    uint32_t       position;    /* Current playback position */
    uint8_t        volume;      /* Master volume (0-255) */
    bool           playing;
    bool           looping;
} audio_stream_t;

/* -------------------------------------------------------------------
 * Audio system state
 * ------------------------------------------------------------------- */

typedef struct {
    /* Double-buffered output */
    int16_t    buffers[2][AUDIO_BUF_SAMPLES * AUDIO_CHANNELS]
                __attribute__((aligned(16)));
    int        current_buf;     /* Which buffer is being filled */

    /* Sound channels */
    audio_channel_t channels[AUDIO_MAX_CHANNELS];

    /* Music */
    audio_stream_t  music;

    /* Master volume */
    uint8_t    master_volume;   /* 0-255 */

    /* Stats */
    uint32_t   active_channels;
    uint32_t   mix_time_us;     /* Microseconds spent mixing */
} audio_state_t;

/* -------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------- */

/*
 * Initialize audio system: configure AI registers, clear buffers.
 */
void audio_init(audio_state_t *as);

/*
 * Shutdown audio: stop DMA, silence output.
 */
void audio_shutdown(audio_state_t *as);

/*
 * Mix one frame of audio and submit to AI.
 * Called once per game frame (30fps).
 *
 * This is the main audio processing function:
 *   1. Mix all active channels into the current buffer
 *   2. Apply master volume
 *   3. DMA the buffer to the AI
 *   4. Swap buffers
 */
void audio_mix_frame(audio_state_t *as);

/*
 * Play a sound effect.
 *
 * sample_data: PCM 16-bit mono sample data
 * sample_len:  Number of samples
 * sample_rate: Source sample rate (e.g., 11025, 22050)
 * volume:      0-255
 * pan:         -128 (left) to +127 (right), 0 = center
 * pitch:       Pitch multiplier (16.16 fixed, 0x10000 = normal)
 * looping:     Loop the sample
 * sample_id:   TR1 sample info index (for duplicate detection)
 * entity_id:   Entity that owns this sound (-1 = global)
 *
 * Returns channel index, or -1 if no channel available.
 */
int audio_play_sample(audio_state_t *as,
                       const int16_t *sample_data,
                       uint32_t sample_len,
                       uint32_t sample_rate,
                       uint8_t volume,
                       int8_t pan,
                       uint32_t pitch,
                       bool looping,
                       int16_t sample_id,
                       int16_t entity_id);

/*
 * Stop a sound by sample_id (used when an entity is destroyed).
 */
void audio_stop_sample(audio_state_t *as, int16_t sample_id);

/*
 * Stop all sounds from a specific entity.
 */
void audio_stop_entity(audio_state_t *as, int16_t entity_id);

/*
 * Start playing a music track from SD card.
 * Uses asset streaming to load the audio data.
 */
void audio_play_music(audio_state_t *as, uint32_t asset_id,
                       uint8_t volume, bool looping);

/*
 * Stop music playback.
 */
void audio_stop_music(audio_state_t *as);

#endif /* N64_AUDIO_H */
