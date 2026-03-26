/*
 * TRX-N64: Audio system — software mixer + N64 AI DMA
 *
 * Mixes up to 16 concurrent mono samples into stereo 16-bit PCM,
 * then DMA's to the N64 Audio Interface for playback.
 *
 * Mixing is done in 32-bit to prevent clipping, then clamped to 16-bit.
 * The mixer handles sample rate conversion via fixed-point stepping.
 *
 * Performance target: <2ms per frame for mixing (leaves 31ms for game+render).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "n64_audio.h"

#include <string.h>

/* -------------------------------------------------------------------
 * N64 AI initialization
 * ------------------------------------------------------------------- */

void audio_init(audio_state_t *as) {
    memset(as, 0, sizeof(*as));
    as->master_volume = 200;  /* ~78% default volume */
    as->current_buf = 0;

    /* Configure AI for 22050 Hz 16-bit stereo (NTSC) */
    AI_DACRATE = (NTSC_CLOCK / AUDIO_SAMPLE_RATE) - 1;  /* 2206 for 22050 Hz */
    AI_BITRATE = 15;  /* 16-bit mode */

    /* Clear buffers */
    memset(as->buffers, 0, sizeof(as->buffers));

    /* Prime the AI with silence to start double-buffering */
    uint32_t phys = (uint32_t)(uintptr_t)as->buffers[0] & 0x1FFFFFFF;
    AI_DRAM_ADDR = phys;
    AI_LEN = AUDIO_BUF_BYTES;
    AI_CONTROL = 1;
}

void audio_shutdown(audio_state_t *as) {
    AI_CONTROL = 0;
    memset(as->channels, 0, sizeof(as->channels));
    as->music.playing = false;
}

/* -------------------------------------------------------------------
 * Software mixer
 *
 * For each active channel:
 *   1. Step through source samples at the channel's playback rate
 *   2. Apply per-channel volume (separate L/R for panning)
 *   3. Accumulate into 32-bit mix buffer
 *   4. Clamp to 16-bit and write to output buffer
 *
 * Rate conversion uses 16.16 fixed-point position stepping.
 * A source at 11025 Hz playing at 22050 Hz output has rate = 0x8000
 * (half speed through source = pitch-correct at 2× output rate).
 * ------------------------------------------------------------------- */

void audio_mix_frame(audio_state_t *as) {
    int16_t *out = as->buffers[as->current_buf];
    int samples = AUDIO_FRAME_SAMPLES;

    /* 32-bit accumulation buffer to prevent clipping */
    static int32_t mix_l[AUDIO_BUF_SAMPLES];
    static int32_t mix_r[AUDIO_BUF_SAMPLES];
    memset(mix_l, 0, samples * sizeof(int32_t));
    memset(mix_r, 0, samples * sizeof(int32_t));

    uint32_t active_count = 0;

    /* Mix sound effect channels */
    for (int ch = 0; ch < AUDIO_MAX_CHANNELS; ch++) {
        audio_channel_t *c = &as->channels[ch];
        if (!c->active || !c->data) continue;
        active_count++;

        for (int i = 0; i < samples; i++) {
            /* Get source sample via fixed-point interpolation */
            uint32_t pos_int = c->position >> 16;
            if (pos_int >= c->length) {
                if (c->looping) {
                    c->position = 0;
                    pos_int = 0;
                } else {
                    c->active = false;
                    break;
                }
            }

            int32_t sample = c->data[pos_int];

            /* Apply per-channel volume with panning */
            mix_l[i] += (sample * c->volume_l) >> 8;
            mix_r[i] += (sample * c->volume_r) >> 8;

            /* Advance position by playback rate */
            c->position += c->rate;
        }
    }

    /* Mix music stream */
    if (as->music.playing && as->music.data) {
        audio_stream_t *m = &as->music;
        for (int i = 0; i < samples; i++) {
            if (m->position >= m->length) {
                if (m->looping) {
                    m->position = 0;
                } else {
                    m->playing = false;
                    break;
                }
            }

            /* Music is assumed mono — center panned */
            int32_t sample = m->data[m->position];
            int32_t vol_sample = (sample * m->volume) >> 8;
            mix_l[i] += vol_sample;
            mix_r[i] += vol_sample;

            m->position++;
        }
    }

    /* Apply master volume and clamp to 16-bit output */
    for (int i = 0; i < samples; i++) {
        int32_t l = (mix_l[i] * as->master_volume) >> 8;
        int32_t r = (mix_r[i] * as->master_volume) >> 8;

        /* Clamp to int16 range */
        if (l > 32767) l = 32767;
        if (l < -32768) l = -32768;
        if (r > 32767) r = 32767;
        if (r < -32768) r = -32768;

        out[i * 2 + 0] = (int16_t)l;
        out[i * 2 + 1] = (int16_t)r;
    }

    as->active_channels = active_count;

    /* Wait for AI to be ready (not full) */
    while (AI_STATUS & AI_STATUS_FULL) {
        /* Busy wait — AI FIFO has room for 2 buffers */
    }

    /* DMA this buffer to the AI */
    /* Cache writeback to ensure RDRAM has our data */
    /* (On real N64, need: cache_data_hit_writeback(out, AUDIO_BUF_BYTES)) */

    uint32_t phys = (uint32_t)(uintptr_t)out & 0x1FFFFFFF;
    AI_DRAM_ADDR = phys;
    AI_LEN = samples * AUDIO_CHANNELS * (AUDIO_BIT_DEPTH / 8);
    AI_CONTROL = 1;

    /* Swap buffer for next frame */
    as->current_buf ^= 1;
}

/* -------------------------------------------------------------------
 * Sample playback
 * ------------------------------------------------------------------- */

int audio_play_sample(audio_state_t *as,
                       const int16_t *sample_data,
                       uint32_t sample_len,
                       uint32_t sample_rate,
                       uint8_t volume,
                       int8_t pan,
                       uint32_t pitch,
                       bool looping,
                       int16_t sample_id,
                       int16_t entity_id) {
    /* Find a free channel */
    int free_ch = -1;
    for (int ch = 0; ch < AUDIO_MAX_CHANNELS; ch++) {
        if (!as->channels[ch].active) {
            free_ch = ch;
            break;
        }
    }

    /* If no free channel, steal the quietest one */
    if (free_ch < 0) {
        uint16_t quietest = 65535;
        for (int ch = 0; ch < AUDIO_MAX_CHANNELS; ch++) {
            uint16_t vol = as->channels[ch].volume_l + as->channels[ch].volume_r;
            if (vol < quietest) {
                quietest = vol;
                free_ch = ch;
            }
        }
    }

    if (free_ch < 0) return -1;

    audio_channel_t *c = &as->channels[free_ch];
    c->data = sample_data;
    c->length = sample_len;
    c->position = 0;

    /*
     * Playback rate: convert source sample rate to output rate.
     * rate = (source_rate * pitch / 0x10000) / output_rate * 0x10000
     *
     * Example: 11025 Hz source at normal pitch to 22050 Hz output:
     *   rate = (11025 * 0x10000 / 0x10000) / 22050 * 0x10000
     *        = 11025 * 0x10000 / 22050 = 0x8000
     */
    uint64_t rate64 = ((uint64_t)sample_rate * pitch) / AUDIO_SAMPLE_RATE;
    c->rate = (uint32_t)rate64;
    if (c->rate == 0) c->rate = 0x10000;  /* Fallback to 1:1 */

    /* Volume with stereo panning.
     * pan: -128 (full left) to +127 (full right), 0 = center.
     * Linear crossfade between channels. */
    int32_t pan_l = 128 - pan;  /* 0 (right) to 256 (left) */
    int32_t pan_r = 128 + pan;  /* 0 (left) to 256 (right) */
    if (pan_l > 255) pan_l = 255;
    if (pan_r > 255) pan_r = 255;
    if (pan_l < 0) pan_l = 0;
    if (pan_r < 0) pan_r = 0;

    c->volume_l = (uint8_t)((volume * pan_l) >> 8);
    c->volume_r = (uint8_t)((volume * pan_r) >> 8);

    c->looping = looping;
    c->sample_id = sample_id;
    c->entity_id = entity_id;
    c->active = true;

    return free_ch;
}

void audio_stop_sample(audio_state_t *as, int16_t sample_id) {
    for (int ch = 0; ch < AUDIO_MAX_CHANNELS; ch++) {
        if (as->channels[ch].active && as->channels[ch].sample_id == sample_id) {
            as->channels[ch].active = false;
        }
    }
}

void audio_stop_entity(audio_state_t *as, int16_t entity_id) {
    for (int ch = 0; ch < AUDIO_MAX_CHANNELS; ch++) {
        if (as->channels[ch].active && as->channels[ch].entity_id == entity_id) {
            as->channels[ch].active = false;
        }
    }
}

/* -------------------------------------------------------------------
 * Music playback
 * ------------------------------------------------------------------- */

void audio_play_music(audio_state_t *as, uint32_t asset_id,
                       uint8_t volume, bool looping) {
    /* Load music data via asset streaming */
    extern void *trx_asset_load(uint32_t asset_id, uint32_t *out_size);
    uint32_t size = 0;
    const int16_t *data = (const int16_t *)trx_asset_load(asset_id, &size);
    if (!data || size == 0) return;

    as->music.asset_id = asset_id;
    as->music.data = data;
    as->music.length = size / 2;  /* int16_t samples */
    as->music.position = 0;
    as->music.volume = volume;
    as->music.looping = looping;
    as->music.playing = true;
}

void audio_stop_music(audio_state_t *as) {
    as->music.playing = false;
}
