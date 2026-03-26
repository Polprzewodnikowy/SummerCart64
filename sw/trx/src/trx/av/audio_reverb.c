// Based on Wine/FAudio reverb DSP (FAudioFX_reverb.c). Adapted for TRX.
// Original authors: Ethan Lee, Luigi Auriemma, and the MonoGame Team.
// License: zlib (see wine/libs/faudio/src/FAudioFX_reverb.c).

#include <trx/av/audio_internal.h>

#include <trx/core/memory.h>
#include <trx/core/utils.h>
#include <trx/debug.h>

#include <math.h>
#include <stdint.h>
#include <string.h>

#define M_PRESET_COUNT 3

#define M_REVERB_DEFAULT_REAR_DELAY 5
#define M_REVERB_DEFAULT_POSITION 6
#define M_REVERB_DEFAULT_POSITION_MATRIX 27
#define M_REVERB_DEFAULT_ROOM_SIZE 100.0f
#define M_REVERB_DEFAULT_WET_DRY_MIX 100.0f
#define M_REVERB_DEFAULT_ROOM_FILTER_FREQ 5000.0f
#define M_REVERB_DEFAULT_ROOM_FILTER_MAIN 0.0f
#define M_REVERB_DEFAULT_ROOM_FILTER_HF 0.0f
#define M_REVERB_DEFAULT_REFLECTIONS_GAIN 0.0f
#define M_REVERB_DEFAULT_REVERB_GAIN 0.0f
#define M_REVERB_DEFAULT_DECAY_TIME 1.0f
#define M_REVERB_DEFAULT_DENSITY 100.0f

#define M_REVERB_MAX_REFLECTIONS_DELAY 300
#define M_REVERB_MAX_REVERB_DELAY 85
#define M_REVERB_MIN_DECAY_TIME 0.1f
#define M_REVERB_DENORMAL_EPSILON 1e-15f

#define M_REVERB_WET_GAIN 1.20f // TRX addition
#define M_DELAY_MAX_MS 300

#define M_REVERB_COUNT_COMB 8
#define M_REVERB_COUNT_APF_IN 1
#define M_REVERB_COUNT_APF_OUT 4

typedef struct {
    float wet_dry_mix;
    int32_t room;
    int32_t room_hf;
    float room_rolloff_factor;
    float decay_time;
    float decay_hf_ratio;
    int32_t reflections;
    float reflections_delay;
    int32_t reverb;
    float reverb_delay;
    float diffusion;
    float density;
    float hf_reference;
} M_I3DL2_PARAMETERS;

typedef struct {
    float wet_dry_mix;
    uint32_t reflections_delay;
    uint8_t reverb_delay;
    uint8_t rear_delay;
    uint8_t position_left;
    uint8_t position_right;
    uint8_t position_matrix_left;
    uint8_t position_matrix_right;
    uint8_t early_diffusion;
    uint8_t late_diffusion;
    uint8_t low_eq_gain;
    uint8_t low_eq_cutoff;
    uint8_t high_eq_gain;
    uint8_t high_eq_cutoff;
    float room_filter_freq;
    float room_filter_main;
    float room_filter_hf;
    float reflections_gain;
    float reverb_gain;
    float decay_time;
    float density;
    float room_size;
} M_PARAMETERS;

typedef struct {
    int32_t sample_rate;
    uint32_t capacity;
    uint32_t delay;
    uint32_t read_idx;
    uint32_t write_idx;
    float *buffer;
} M_DSP_DELAY;

typedef enum {
    M_DSP_BI_QUAD_LOW_SHELVING,
    M_DSP_BI_QUAD_HIGH_SHELVING,
} M_DSP_BI_QUAD_TYPE;

typedef struct {
    int32_t sample_rate;
    float a0;
    float a1;
    float a2;
    float b1;
    float b2;
    float c0;
    float d0;
    float delay0;
    float delay1;
} M_DSP_BI_QUAD;

typedef struct {
    M_DSP_DELAY comb_delay;
    M_DSP_BI_QUAD low_shelving;
    M_DSP_BI_QUAD high_shelving;
    float comb_feedback_gain;
} M_DSP_COMB_SHELVING;

typedef struct {
    M_DSP_DELAY apf_delay;
    float apf_feedback_gain;
} M_DSP_ALL_PASS;

typedef enum {
    M_POSITION_LEFT = 1,
    M_POSITION_RIGHT = 2,
    M_POSITION_CENTER = 4,
    M_POSITION_REAR = 8
} M_CHANNEL_POSITION_FLAGS;

typedef struct {
    M_DSP_DELAY reverb_delay;
    M_DSP_COMB_SHELVING lpf_comb[M_REVERB_COUNT_COMB];
    M_DSP_ALL_PASS apf_out[M_REVERB_COUNT_APF_OUT];
    M_DSP_BI_QUAD room_high_shelf;
    float early_gain;
    float gain;
} M_DSP_REVERB_CHANNEL;

typedef struct {
    M_DSP_DELAY early_delay;
    M_DSP_ALL_PASS apf_in[M_REVERB_COUNT_APF_IN];

    int32_t in_channels;
    int32_t out_channels;
    int32_t reverb_channels;
    M_DSP_REVERB_CHANNEL channel[2];

    float early_gain;
    float reverb_gain;
    float room_gain;
    float wet_ratio;
    float dry_ratio;
} M_DSP_REVERB;

static const float m_CombDelays[M_REVERB_COUNT_COMB] = {
    25.31f, 26.94f, 28.96f, 30.75f, 32.24f, 33.80f, 35.31f, 36.67f,
};

static const float m_ApfInDelays[M_REVERB_COUNT_APF_IN] = { 13.28f };
static const float m_ApfOutDelays[M_REVERB_COUNT_APF_OUT] = {
    5.10f,
    12.61f,
    10.0f,
    7.73f,
};

static const M_I3DL2_PARAMETERS m_ReverbPresets[M_PRESET_COUNT] = {
    // clang-format off
    { 50.0f, -1000, -500, 0.0f, 2.31f, 0.64f, -711, 0.012f, -800, 0.017f, 100.0f, 100.0f, 5000.0f, }, // Small Room
    { 50.0f, -1000, -500, 0.0f, 2.31f, 0.64f, -711, 0.012f, -300, 0.017f, 100.0f, 100.0f, 5000.0f, }, // Medium Room
    { 50.0f, -1000, -500, 0.0f, 2.31f, 0.64f, -711, 0.012f, 200, 0.017f, 100.0f, 100.0f, 5000.0f, } // Large Room
    // clang-format on
};

static bool m_IsInitialised = false;
static uint8_t m_ReverbType = 0;
static M_PARAMETERS m_ReverbTypes[M_PRESET_COUNT];
static M_DSP_REVERB m_Reverb;

static inline float M_DbGainToFactor(const float gain)
{
    return powf(10.0f, gain / 20.0f);
}

static inline uint32_t M_MsToSamples(
    const float msec, const int32_t sample_rate)
{
    return (uint32_t)((sample_rate * msec) / 1000.0f);
}

static inline float M_Undenormalize(const float sample_in)
{
    return fabsf(sample_in) < M_REVERB_DENORMAL_EPSILON ? 0.0f : sample_in;
}

static inline void M_DspDelay_Initialize(
    M_DSP_DELAY *const filter, const int32_t sample_rate, const float delay_ms)
{
    ASSERT(delay_ms >= 0.0f && delay_ms <= M_DELAY_MAX_MS);

    filter->sample_rate = sample_rate;
    filter->capacity = M_MsToSamples(M_DELAY_MAX_MS, sample_rate);
    filter->delay = M_MsToSamples(delay_ms, sample_rate);
    filter->read_idx = 0;
    filter->write_idx = filter->delay;
    filter->buffer = Memory_Alloc(filter->capacity * sizeof(float));
}

static inline void M_DspDelay_Change(
    M_DSP_DELAY *const filter, const float delay_ms)
{
    ASSERT(delay_ms >= 0.0f && delay_ms <= M_DELAY_MAX_MS);

    filter->delay = M_MsToSamples(delay_ms, filter->sample_rate);
    filter->read_idx = (filter->write_idx - filter->delay + filter->capacity)
        % filter->capacity;
}

static inline float M_DspDelay_Read(M_DSP_DELAY *const filter)
{
    ASSERT(filter->read_idx < filter->capacity);
    const float delay_out = filter->buffer[filter->read_idx];
    filter->read_idx = (filter->read_idx + 1) % filter->capacity;
    return delay_out;
}

static inline void M_DspDelay_Write(
    M_DSP_DELAY *const filter, const float sample)
{
    ASSERT(filter->write_idx < filter->capacity);
    filter->buffer[filter->write_idx] = M_Undenormalize(sample);
    filter->write_idx = (filter->write_idx + 1) % filter->capacity;
}

static inline float M_DspDelay_Process(
    M_DSP_DELAY *const filter, const float sample)
{
    const float delay_out = M_DspDelay_Read(filter);
    M_DspDelay_Write(filter, sample);
    return delay_out;
}

static inline void M_DspDelay_Reset(M_DSP_DELAY *const filter)
{
    filter->read_idx = 0;
    filter->write_idx = filter->delay;
    memset(filter->buffer, 0, filter->capacity * sizeof(float));
}

static inline void M_DspDelay_Destroy(M_DSP_DELAY *const filter)
{
    Memory_Free(filter->buffer);
    filter->buffer = nullptr;
}

static inline float M_DspComb_FeedbackFromRT60(
    M_DSP_DELAY *const delay, const float rt60_ms)
{
    const float exponent = (-3.0f * (float)delay->delay * 1000.0f)
        / ((float)delay->sample_rate * rt60_ms);
    return powf(10.0f, exponent);
}

static inline void M_DspBiQuad_Change(
    M_DSP_BI_QUAD *const filter, const M_DSP_BI_QUAD_TYPE type,
    const float frequency, const float q, const float gain)
{
    const float theta_c = (2 * M_PI * frequency) / (float)filter->sample_rate;
    const float mu = M_DbGainToFactor(gain);
    const float beta = type == M_DSP_BI_QUAD_LOW_SHELVING ? 4.0f / (1.0f + mu)
                                                          : (1.0f + mu) / 4.0f;
    const float delta = beta * tanf(theta_c * 0.5f);
    const float gamma = (1.0f - delta) / (1.0f + delta);

    if (type == M_DSP_BI_QUAD_LOW_SHELVING) {
        filter->a0 = (1.0f - gamma) * 0.5f;
        filter->a1 = filter->a0;
    } else {
        filter->a0 = (1.0f + gamma) * 0.5f;
        filter->a1 = -filter->a0;
    }

    filter->a2 = 0.0f;
    filter->b1 = -gamma;
    filter->b2 = 0.0f;
    filter->c0 = mu - 1.0f;
    filter->d0 = 1.0f;
}

static inline void M_DspBiQuad_Initialize(
    M_DSP_BI_QUAD *const filter, const int32_t sample_rate,
    const M_DSP_BI_QUAD_TYPE type, const float frequency, const float q,
    const float gain)
{
    filter->sample_rate = sample_rate;
    filter->delay0 = 0.0f;
    filter->delay1 = 0.0f;
    M_DspBiQuad_Change(filter, type, frequency, q, gain);
}

static inline float M_DspBiQuad_Process(
    M_DSP_BI_QUAD *const filter, const float sample_in)
{
    const float result = (filter->a0 * sample_in) + filter->delay0;
    const float delay0 =
        (filter->a1 * sample_in) - (filter->b1 * result) + filter->delay1;
    const float delay1 = (filter->a2 * sample_in) - (filter->b2 * result);
    filter->delay0 = M_Undenormalize(delay0);
    filter->delay1 = M_Undenormalize(delay1);

    return M_Undenormalize((result * filter->c0) + (sample_in * filter->d0));
}

static inline void M_DspBiQuad_Reset(M_DSP_BI_QUAD *const filter)
{
    filter->delay0 = 0.0f;
    filter->delay1 = 0.0f;
}

static inline void M_DspBiQuad_Destroy(M_DSP_BI_QUAD *const filter)
{
    (void)filter;
}

static inline void M_DspCombShelving_Initialize(
    M_DSP_COMB_SHELVING *const filter, const int32_t sample_rate,
    const float delay_ms, const float rt60_ms, const float low_frequency,
    const float low_q, const float low_gain, const float high_frequency,
    const float high_q, const float high_gain)
{
    M_DspDelay_Initialize(&filter->comb_delay, sample_rate, delay_ms);
    filter->comb_feedback_gain =
        M_DspComb_FeedbackFromRT60(&filter->comb_delay, rt60_ms);
    M_DspBiQuad_Initialize(
        &filter->low_shelving, sample_rate, M_DSP_BI_QUAD_LOW_SHELVING,
        low_frequency, low_q, low_gain);
    M_DspBiQuad_Initialize(
        &filter->high_shelving, sample_rate, M_DSP_BI_QUAD_HIGH_SHELVING,
        high_frequency, high_q, high_gain);
}

static inline float M_DspCombShelving_Process(
    M_DSP_COMB_SHELVING *const filter, const float sample_in)
{
    const float delay_out = M_DspDelay_Read(&filter->comb_delay);

    float feedback = M_DspBiQuad_Process(&filter->high_shelving, delay_out);
    feedback = M_DspBiQuad_Process(&filter->low_shelving, feedback);

    const float to_buf =
        M_Undenormalize(sample_in + (filter->comb_feedback_gain * feedback));
    M_DspDelay_Write(&filter->comb_delay, to_buf);

    return delay_out;
}

static inline void M_DspCombShelving_Reset(M_DSP_COMB_SHELVING *const filter)
{
    M_DspDelay_Reset(&filter->comb_delay);
    M_DspBiQuad_Reset(&filter->low_shelving);
    M_DspBiQuad_Reset(&filter->high_shelving);
}

static inline void M_DspCombShelving_Destroy(M_DSP_COMB_SHELVING *const filter)
{
    M_DspDelay_Destroy(&filter->comb_delay);
    M_DspBiQuad_Destroy(&filter->low_shelving);
    M_DspBiQuad_Destroy(&filter->high_shelving);
}

static inline void M_DspAllPass_Initialize(
    M_DSP_ALL_PASS *const filter, const int32_t sample_rate,
    const float delay_ms, const float gain)
{
    M_DspDelay_Initialize(&filter->apf_delay, sample_rate, delay_ms);
    filter->apf_feedback_gain = gain;
}

static inline void M_DspAllPass_Change(
    M_DSP_ALL_PASS *const filter, const float delay_ms, const float gain)
{
    M_DspDelay_Change(&filter->apf_delay, delay_ms);
    filter->apf_feedback_gain = gain;
}

static inline float M_DspAllPass_Process(
    M_DSP_ALL_PASS *const filter, const float sample_in)
{
    const float delay_out = M_DspDelay_Read(&filter->apf_delay);
    const float to_buf =
        M_Undenormalize(sample_in + (filter->apf_feedback_gain * delay_out));
    M_DspDelay_Write(&filter->apf_delay, to_buf);
    return M_Undenormalize(delay_out - (filter->apf_feedback_gain * to_buf));
}

static inline void M_DspAllPass_Reset(M_DSP_ALL_PASS *const filter)
{
    M_DspDelay_Reset(&filter->apf_delay);
}

static inline void M_DspAllPass_Destroy(M_DSP_ALL_PASS *const filter)
{
    M_DspDelay_Destroy(&filter->apf_delay);
}

static inline M_CHANNEL_POSITION_FLAGS M_GetChannelPositionFlags(
    const int32_t total_channels, const int32_t channel)
{
    switch (total_channels) {
    case 1:
        return M_POSITION_CENTER;
    case 2:
        return channel == 0 ? M_POSITION_LEFT : M_POSITION_RIGHT;
    default:
        break;
    }

    ASSERT(0 && "Unsupported channel count");
    return M_POSITION_LEFT;
}

static inline float M_GetStereoSpreadDelayMS(
    const int32_t total_channels, const int32_t channel)
{
    const M_CHANNEL_POSITION_FLAGS flags =
        M_GetChannelPositionFlags(total_channels, channel);
    return (flags & M_POSITION_RIGHT) != 0 ? 0.5216f : 0.0f;
}

static inline void M_DspReverb_Create(
    M_DSP_REVERB *const reverb, const int32_t sample_rate,
    const int32_t in_channels, const int32_t out_channels)
{
    ASSERT(in_channels == 1 || in_channels == 2);
    ASSERT(out_channels == 1 || out_channels == 2);

    memset(reverb, 0, sizeof(*reverb));
    M_DspDelay_Initialize(&reverb->early_delay, sample_rate, 10.0f);

    for (int32_t i = 0; i < M_REVERB_COUNT_APF_IN; i++) {
        M_DspAllPass_Initialize(
            &reverb->apf_in[i], sample_rate, m_ApfInDelays[i], 0.5f);
    }

    reverb->reverb_channels = out_channels;
    for (int32_t c = 0; c < reverb->reverb_channels; c++) {
        M_DspDelay_Initialize(
            &reverb->channel[c].reverb_delay, sample_rate, 10.0f);

        for (int32_t i = 0; i < M_REVERB_COUNT_COMB; i++) {
            M_DspCombShelving_Initialize(
                &reverb->channel[c].lpf_comb[i], sample_rate,
                m_CombDelays[i]
                    + M_GetStereoSpreadDelayMS(reverb->reverb_channels, c),
                500.0f, 500.0f, 0.0f, -6.0f, 5000.0f, 0.0f, -6.0f);
        }

        for (int32_t i = 0; i < M_REVERB_COUNT_APF_OUT; i++) {
            M_DspAllPass_Initialize(
                &reverb->channel[c].apf_out[i], sample_rate,
                m_ApfOutDelays[i]
                    + M_GetStereoSpreadDelayMS(reverb->reverb_channels, c),
                0.5f);
        }

        M_DspBiQuad_Initialize(
            &reverb->channel[c].room_high_shelf, sample_rate,
            M_DSP_BI_QUAD_HIGH_SHELVING, 5000.0f, 0.0f, -10.0f);
        reverb->channel[c].gain = 1.0f;
    }

    reverb->early_gain = 1.0f;
    reverb->reverb_gain = 1.0f;
    reverb->dry_ratio = 0.0f;
    reverb->wet_ratio = 1.0f;
    reverb->in_channels = in_channels;
    reverb->out_channels = out_channels;
}

static inline void M_DspReverb_Destroy(M_DSP_REVERB *const reverb)
{
    M_DspDelay_Destroy(&reverb->early_delay);

    for (int32_t i = 0; i < M_REVERB_COUNT_APF_IN; i++) {
        M_DspAllPass_Destroy(&reverb->apf_in[i]);
    }

    for (int32_t c = 0; c < reverb->reverb_channels; c++) {
        M_DspDelay_Destroy(&reverb->channel[c].reverb_delay);
        for (int32_t i = 0; i < M_REVERB_COUNT_COMB; i++) {
            M_DspCombShelving_Destroy(&reverb->channel[c].lpf_comb[i]);
        }
        M_DspBiQuad_Destroy(&reverb->channel[c].room_high_shelf);
        for (int32_t i = 0; i < M_REVERB_COUNT_APF_OUT; i++) {
            M_DspAllPass_Destroy(&reverb->channel[c].apf_out[i]);
        }
    }
}

static inline void M_DspReverb_SetParameters(
    M_DSP_REVERB *const reverb, const M_PARAMETERS *const params)
{
    const float early_diffusion =
        0.6f - ((params->early_diffusion / 15.0f) * 0.2f);

    M_DspDelay_Change(&reverb->early_delay, (float)params->reflections_delay);

    for (int32_t i = 0; i < M_REVERB_COUNT_APF_IN; i++) {
        M_DspAllPass_Change(
            &reverb->apf_in[i], m_ApfInDelays[i], early_diffusion);
    }

    for (int32_t c = 0; c < reverb->reverb_channels; c++) {
        const M_CHANNEL_POSITION_FLAGS position =
            M_GetChannelPositionFlags(reverb->reverb_channels, c);
        const float channel_delay =
            (position & M_POSITION_REAR) != 0 ? params->rear_delay : 0.0f;

        M_DspDelay_Change(
            &reverb->channel[c].reverb_delay,
            (float)params->reverb_delay + channel_delay);

        for (int32_t i = 0; i < M_REVERB_COUNT_COMB; i++) {
            M_DSP_COMB_SHELVING *const comb = &reverb->channel[c].lpf_comb[i];

            M_DspDelay_Change(
                &comb->comb_delay,
                m_CombDelays[i]
                    + M_GetStereoSpreadDelayMS(reverb->reverb_channels, c));

            comb->comb_feedback_gain = M_DspComb_FeedbackFromRT60(
                &comb->comb_delay,
                MAX(params->decay_time, M_REVERB_MIN_DECAY_TIME) * 1000.0f);

            M_DspBiQuad_Change(
                &comb->low_shelving, M_DSP_BI_QUAD_LOW_SHELVING,
                50.0f + params->low_eq_cutoff * 50.0f, 0.0f,
                params->low_eq_gain - 8.0f);
            M_DspBiQuad_Change(
                &comb->high_shelving, M_DSP_BI_QUAD_HIGH_SHELVING,
                1000.0f + params->high_eq_cutoff * 500.0f, 0.0f,
                params->high_eq_gain - 8.0f);
        }
    }

    reverb->early_gain = M_DbGainToFactor(params->reflections_gain);
    reverb->reverb_gain = M_DbGainToFactor(params->reverb_gain);
    reverb->room_gain = M_DbGainToFactor(params->room_filter_main);

    const float late_diffusion =
        0.6f - ((params->late_diffusion / 15.0f) * 0.2f);
    for (int32_t c = 0; c < reverb->reverb_channels; c++) {
        const M_CHANNEL_POSITION_FLAGS position =
            M_GetChannelPositionFlags(reverb->reverb_channels, c);
        for (int32_t i = 0; i < M_REVERB_COUNT_APF_OUT; i++) {
            M_DspAllPass_Change(
                &reverb->channel[c].apf_out[i],
                m_ApfOutDelays[i]
                    + M_GetStereoSpreadDelayMS(reverb->reverb_channels, c),
                late_diffusion);
        }

        M_DspBiQuad_Change(
            &reverb->channel[c].room_high_shelf, M_DSP_BI_QUAD_HIGH_SHELVING,
            params->room_filter_freq, 0.0f,
            params->room_filter_main + params->room_filter_hf);

        float gain = 0.0f;
        if ((position & M_POSITION_LEFT) != 0) {
            gain = params->position_matrix_left;
        } else if ((position & M_POSITION_RIGHT) != 0) {
            gain = params->position_matrix_right;
        } else {
            gain =
                (params->position_matrix_left + params->position_matrix_right)
                / 2.0f;
        }
        reverb->channel[c].gain = 1.5f - (gain / 27.0f) * 0.5f;

        if ((position & M_POSITION_REAR) != 0) {
            reverb->channel[c].gain *= 0.75f;
        }

        if ((position & M_POSITION_LEFT) != 0) {
            gain = params->position_left;
        } else if ((position & M_POSITION_RIGHT) != 0) {
            gain = params->position_right;
        } else {
            gain = (params->position_left + params->position_right) / 2.0f;
        }
        reverb->channel[c].early_gain =
            (1.2f - (gain / 6.0f) * 0.2f) * reverb->early_gain;
    }

    reverb->wet_ratio = params->wet_dry_mix / 100.0f;
    reverb->dry_ratio = 1.0f - reverb->wet_ratio;
}

static inline void M_DspReverb_Reset(M_DSP_REVERB *const reverb)
{
    M_DspDelay_Reset(&reverb->early_delay);
    for (int32_t i = 0; i < M_REVERB_COUNT_APF_IN; i++) {
        M_DspAllPass_Reset(&reverb->apf_in[i]);
    }
    for (int32_t c = 0; c < reverb->reverb_channels; c++) {
        M_DspDelay_Reset(&reverb->channel[c].reverb_delay);
        for (int32_t i = 0; i < M_REVERB_COUNT_COMB; i++) {
            M_DspCombShelving_Reset(&reverb->channel[c].lpf_comb[i]);
        }
        for (int32_t i = 0; i < M_REVERB_COUNT_APF_OUT; i++) {
            M_DspAllPass_Reset(&reverb->channel[c].apf_out[i]);
        }
        M_DspBiQuad_Reset(&reverb->channel[c].room_high_shelf);
    }
}

static inline float M_DspReverb_ProcessEarly(
    M_DSP_REVERB *const reverb, const float sample_in)
{
    float delay_out = M_DspDelay_Process(&reverb->early_delay, sample_in);
    for (int32_t i = 0; i < M_REVERB_COUNT_APF_IN; i++) {
        delay_out = M_DspAllPass_Process(&reverb->apf_in[i], delay_out);
    }
    return delay_out;
}

static inline float M_DspReverb_ProcessChannel(
    M_DSP_REVERB *const reverb, M_DSP_REVERB_CHANNEL *const channel,
    const float sample_in)
{
    float sample_out = 0.0f;
    float early_late = 0.0f;

    const float revdelay =
        M_DspDelay_Process(&channel->reverb_delay, sample_in);

    for (int32_t i = 0; i < M_REVERB_COUNT_COMB; i++) {
        sample_out +=
            M_DspCombShelving_Process(&channel->lpf_comb[i], revdelay);
    }
    sample_out /= (float)M_REVERB_COUNT_COMB;

    for (int32_t i = 0; i < M_REVERB_COUNT_APF_OUT; i++) {
        sample_out = M_DspAllPass_Process(&channel->apf_out[i], sample_out);
    }

    early_late =
        (sample_in * channel->early_gain) + (sample_out * reverb->reverb_gain);

    sample_out = M_DspBiQuad_Process(
        &channel->room_high_shelf, early_late * reverb->room_gain);

    return sample_out * channel->gain;
}

static inline void M_DspReverb_Process_2_to_2(
    M_DSP_REVERB *const reverb, float *samples, const size_t sample_count)
{
    float *const samples_end = samples + (sample_count * 2);
    while (samples < samples_end) {
        const float in = (samples[0] + samples[1]) * 0.5f;
        const float early = M_DspReverb_ProcessEarly(reverb, in);

        const float left =
            (M_DspReverb_ProcessChannel(reverb, &reverb->channel[0], early)
             * reverb->wet_ratio * M_REVERB_WET_GAIN)
            + samples[0] * reverb->dry_ratio;
        const float right =
            (M_DspReverb_ProcessChannel(reverb, &reverb->channel[1], early)
             * reverb->wet_ratio * M_REVERB_WET_GAIN)
            + samples[1] * reverb->dry_ratio;

        samples[0] = M_Undenormalize(left);
        samples[1] = M_Undenormalize(right);
        samples += 2;
    }
}

static inline void M_DspReverb_Process_1_to_1(
    M_DSP_REVERB *const reverb, float *samples, const size_t sample_count)
{
    float *const samples_end = samples + sample_count;
    while (samples < samples_end) {
        const float in = *samples;
        const float early = M_DspReverb_ProcessEarly(reverb, in);
        const float late =
            M_DspReverb_ProcessChannel(reverb, &reverb->channel[0], early);
        *samples = M_Undenormalize(
            (late * reverb->wet_ratio) + (in * reverb->dry_ratio));
        samples++;
    }
}

static inline void M_ConvertI3DL2ToNative(
    const M_I3DL2_PARAMETERS *const i3dl2, M_PARAMETERS *const native)
{
    native->rear_delay = M_REVERB_DEFAULT_REAR_DELAY;
    native->position_left = M_REVERB_DEFAULT_POSITION;
    native->position_right = M_REVERB_DEFAULT_POSITION;
    native->position_matrix_left = M_REVERB_DEFAULT_POSITION_MATRIX;
    native->position_matrix_right = M_REVERB_DEFAULT_POSITION_MATRIX;
    native->room_size = M_REVERB_DEFAULT_ROOM_SIZE;
    native->low_eq_cutoff = 4;
    native->high_eq_cutoff = 6;

    native->room_filter_main = (float)i3dl2->room / 100.0f;
    native->room_filter_hf = (float)i3dl2->room_hf / 100.0f;

    if (i3dl2->decay_hf_ratio >= 1.0f) {
        int32_t index = (int32_t)(-4.0f * log10f(i3dl2->decay_hf_ratio));
        if (index < -8) {
            index = -8;
        }
        native->low_eq_gain = (uint8_t)(index < 0 ? index + 8 : 8);
        native->high_eq_gain = 8;
        native->decay_time = i3dl2->decay_time * i3dl2->decay_hf_ratio;
    } else {
        int32_t index = (int32_t)(4.0f * log10f(i3dl2->decay_hf_ratio));
        if (index < -8) {
            index = -8;
        }
        native->low_eq_gain = 8;
        native->high_eq_gain = (uint8_t)(index < 0 ? index + 8 : 8);
        native->decay_time = i3dl2->decay_time;
    }

    float reflections_delay = i3dl2->reflections_delay * 1000.0f;
    if (reflections_delay >= M_REVERB_MAX_REFLECTIONS_DELAY) {
        reflections_delay = (float)(M_REVERB_MAX_REFLECTIONS_DELAY - 1);
    } else if (reflections_delay <= 1.0f) {
        reflections_delay = 1.0f;
    }
    native->reflections_delay = (uint32_t)reflections_delay;

    float reverb_delay = i3dl2->reverb_delay * 1000.0f;
    if (reverb_delay >= M_REVERB_MAX_REVERB_DELAY) {
        reverb_delay = (float)(M_REVERB_MAX_REVERB_DELAY - 1);
    }
    native->reverb_delay = (uint8_t)reverb_delay;

    native->reflections_gain = i3dl2->reflections / 100.0f;
    native->reverb_gain = i3dl2->reverb / 100.0f;
    native->early_diffusion = (uint8_t)(15.0f * i3dl2->diffusion / 100.0f);
    native->late_diffusion = native->early_diffusion;
    native->density = i3dl2->density;
    native->room_filter_freq = i3dl2->hf_reference;
    native->wet_dry_mix = i3dl2->wet_dry_mix;
}

void Audio_Reverb_Init(const int32_t sample_rate, const int32_t channels)
{
    if (m_IsInitialised) {
        return;
    }

    M_DspReverb_Create(&m_Reverb, sample_rate, channels, channels);
    for (int32_t i = 0; i < M_PRESET_COUNT; i++) {
        M_ConvertI3DL2ToNative(&m_ReverbPresets[i], &m_ReverbTypes[i]);
    }
    m_IsInitialised = true;
}

void Audio_Reverb_Shutdown(void)
{
    if (!m_IsInitialised) {
        return;
    }

    M_DspReverb_Destroy(&m_Reverb);
    m_IsInitialised = false;
}

void Audio_Reverb_SetType(uint8_t reverb_type)
{
    CLAMPG(reverb_type, M_PRESET_COUNT);

    const bool type_changed = m_ReverbType != reverb_type;
    m_ReverbType = reverb_type;
    if (!m_IsInitialised) {
        return;
    }

    if (type_changed) {
        M_DspReverb_Reset(&m_Reverb);
    }

    if (m_ReverbType != 0) {
        M_DspReverb_SetParameters(&m_Reverb, &m_ReverbTypes[m_ReverbType - 1]);
    }
}

uint8_t Audio_Reverb_GetType(void)
{
    return m_ReverbType;
}

void Audio_Reverb_Process(float *const dst_buffer, const size_t len)
{
    if (!m_IsInitialised) {
        return;
    }

    if (m_ReverbType == 0) {
        return;
    }

    const size_t samples = len / sizeof(float) / (size_t)AUDIO_WORKING_CHANNELS;
    if (samples == 0) {
        return;
    }

    if (AUDIO_WORKING_CHANNELS == 1) {
        M_DspReverb_Process_1_to_1(&m_Reverb, dst_buffer, samples);
    } else if (AUDIO_WORKING_CHANNELS == 2) {
        M_DspReverb_Process_2_to_2(&m_Reverb, dst_buffer, samples);
    }
}
