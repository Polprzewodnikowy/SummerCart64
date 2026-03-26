/*
 * TRX-N64: TR1 sound effect bridge
 *
 * Maps TR1's sound effect system to our N64 audio mixer.
 * Handles 3D positioning (distance attenuation + stereo panning),
 * sample info lookup (volume, pitch, range, flags), and
 * playback mode management (normal, wait, restart, looped).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "n64_audio.h"
#include "../game/tr_game_shim.h"
#include "../level/tr_types.h"

extern fix16_t tr_sin(int32_t angle);
extern fix16_t tr_cos(int32_t angle);

/* External audio state */
extern audio_state_t g_audio;
extern game_state_t g_game;

/* -------------------------------------------------------------------
 * TR1 sample info structure (from level data)
 * ------------------------------------------------------------------- */

typedef struct {
    int16_t  number;        /* Sample track ID */
    uint16_t volume;        /* Raw volume (0-0x7FFF) */
    uint16_t range;         /* Playback distance range */
    uint16_t randomness;    /* Random play chance */
    uint16_t flags;         /* Mode bits, count, control flags */
} tr_sample_info_t;

/* Flag extraction */
#define SF_MODE(f)          ((f) & 0x03)
#define SF_NUM_SAMPLES(f)   (((f) >> 2) & 0x0F)
#define SF_NO_PAN(f)        (((f) >> 12) & 1)
#define SF_RAND_PITCH(f)    (((f) >> 13) & 1)
#define SF_RAND_VOLUME(f)   (((f) >> 14) & 1)

/* Sample modes (exact TR1 values) */
#define SM_NORMAL   0   /* Play once, allow overlapping */
#define SM_WAIT     1   /* Play once, don't restart if playing */
#define SM_RESTART  2   /* Restart from beginning if playing */
#define SM_LOOPED   3   /* Loop continuously */

/* -------------------------------------------------------------------
 * 3D audio: compute volume and pan from world position
 *
 * TR1's spatial audio:
 *   - Volume attenuates with distance² from camera
 *   - Pan based on angle between camera facing and sound direction
 *   - Range defines maximum audible distance
 * ------------------------------------------------------------------- */

static void compute_3d_audio(int32_t sound_x, int32_t sound_y, int32_t sound_z,
                              uint16_t base_volume, uint16_t range,
                              uint8_t *out_volume, int8_t *out_pan) {
    /* Distance from camera */
    int32_t dx = sound_x - g_game.camera.x;
    int32_t dy = sound_y - g_game.camera.y;
    int32_t dz = sound_z - g_game.camera.z;

    /* Distance squared (scale down to prevent overflow) */
    int32_t dist_sq = (dx >> 6) * (dx >> 6) + (dy >> 6) * (dy >> 6) + (dz >> 6) * (dz >> 6);
    int32_t range_sq = ((int32_t)range >> 6) * ((int32_t)range >> 6);

    /* Distance attenuation: linear falloff from 1.0 at origin to 0.0 at range */
    if (range_sq <= 0 || dist_sq >= range_sq) {
        *out_volume = 0;
        *out_pan = 0;
        return;
    }

    int32_t attenuation = 255 - (int32_t)((uint64_t)dist_sq * 255 / range_sq);
    if (attenuation < 0) attenuation = 0;

    /* Apply base volume (0-0x7FFF → 0-255) */
    int32_t vol = ((int32_t)base_volume * attenuation) >> 15;
    if (vol > 255) vol = 255;
    *out_volume = (uint8_t)vol;

    /* Stereo panning based on angle relative to camera */
    int32_t angle = 0;
    if (dx != 0 || dz != 0) {
        /* Compute angle of sound relative to camera facing */
        int32_t cam_sin = tr_sin(g_game.camera.rot_y) >> 2;
        int32_t cam_cos = tr_cos(g_game.camera.rot_y) >> 2;

        /* Project sound direction onto camera's left/right axis */
        int32_t lateral = (dx * cam_cos - dz * cam_sin) >> 14;

        /* Scale to pan range (-128..+127) */
        int32_t dist_xz = 1 + (((dx >> 6) * (dx >> 6) + (dz >> 6) * (dz >> 6)));
        if (dist_xz > 0) {
            int32_t pan = (lateral * 128) / (dist_xz >> 6);
            if (pan > 127) pan = 127;
            if (pan < -128) pan = -128;
            *out_pan = (int8_t)pan;
        } else {
            *out_pan = 0;
        }
    } else {
        *out_pan = 0;
    }
}

/* -------------------------------------------------------------------
 * Play a TR1 sound effect at a world position
 *
 * This is what the game logic calls when it needs a sound.
 * Looks up sample info, computes 3D audio, and submits to mixer.
 * ------------------------------------------------------------------- */

void game_sound_effect(int16_t sample_id,
                        int32_t x, int32_t y, int32_t z,
                        int16_t entity_id) {
    /* Bounds check */
    if (sample_id < 0) return;

    /* Compute 3D volume and pan */
    uint8_t volume;
    int8_t pan;
    compute_3d_audio(x, y, z, 0x4000, 8192, &volume, &pan);

    if (volume == 0) return;  /* Too far away to hear */

    /*
     * TODO: Look up sample info from level data to get:
     *   - actual volume, range, pitch modifiers
     *   - sample mode (normal, wait, restart, looped)
     *   - number of sample variations
     *
     * For now: play with default parameters.
     * The sample data pointer will come from the level's embedded
     * sample data, loaded via asset streaming.
     */

    /*
     * Sample data is stored in the .PHD file and loaded during
     * level parsing. For now this is a stub — the audio data
     * connection will be completed when sample extraction is
     * added to the asset packer.
     */
    (void)sample_id;
    (void)entity_id;
}

/* -------------------------------------------------------------------
 * Play music track (background music from SD card)
 * ------------------------------------------------------------------- */

void game_play_music(uint32_t track_num) {
    /* Music tracks are stored as separate assets */
    uint32_t asset_id = 300 + track_num;  /* ASSET_MUSIC_BASE */
    audio_play_music(&g_audio, asset_id, 180, true);
}

void game_stop_music(void) {
    audio_stop_music(&g_audio);
}
