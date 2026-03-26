#include <trx/av/audio_internal.h>

#include <trx/core/benchmark.h>
#include <trx/core/log.h>
#include <trx/core/memory.h>
#include <trx/debug.h>
#include <trx/version.h>

#include <SDL2/SDL_audio.h>
#include <errno.h>
#include <libavcodec/avcodec.h>
#include <libavcodec/codec.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
#include <libavutil/mem.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    struct {
        int32_t format;
        AVChannelLayout ch_layout;
        int32_t sample_rate;
    } src, dst;
    SwrContext *ctx;
    size_t working_buffer_size;
    uint8_t *working_buffer;
} M_SWR_CONTEXT;

typedef struct {
    char *original_data;
    size_t original_size;
    float *sample_data;
    int32_t channels;
    int32_t num_samples;
} AUDIO_SAMPLE;

typedef struct {
    bool is_used;
    bool is_looped;
    bool is_playing;
    float volume_l; // sample gain multiplier
    float volume_r; // sample gain multiplier

    float pitch;
    // `volume`/`pan` come from the game layer (src/trx/game/sound/common.c).
    // Despite the historic "decibel" naming, these values are not base-10
    // centi-dB. They are a log2-based gain domain that the OG engine used to
    // feed directly to DirectSound (-10000..0 style range).
    //
    // In TRX we keep the game-side math pristine and interpret these as:
    //   TR1/2: log2(gain) * 1000
    //   TR3:   DirectSound-style centi-dB in [-10000..0]
    //
    // `M_DecibelToMultiplier()` is the corresponding inverse transform:
    //   TR1/2: gain = 2^(value/1000)
    //   TR3:   gain = 10^(value/2000)
    //
    // This makes combining contributions (volume + pan) an additive operation
    // in the game's log domain, while still producing a linear multiplier for
    // the mixer.
    int32_t volume;
    int32_t pan;

    // pitch shift means the same samples can be reused twice, hence float
    float current_sample;

    AUDIO_SAMPLE *sample;
} AUDIO_SAMPLE_SOUND;

typedef struct {
    const uint8_t *data;
    const uint8_t *ptr;
    int32_t size;
    int32_t remaining;
} AUDIO_AV_BUFFER;

static int32_t m_LoadedSamplesCount = 0;
static AUDIO_SAMPLE m_LoadedSamples[AUDIO_MAX_SAMPLES] = {};
static AUDIO_SAMPLE_SOUND m_Samples[AUDIO_MAX_ACTIVE_SAMPLES] = {};

static double M_DecibelToMultiplier(double db_gain)
{
    if (g_TRVersion < 3) {
        // Legacy scale
        return pow(2.0, db_gain / 600.0);
    } else {
        // DirectSound-style centi-dB domain: gain = 10^(centi_dB/2000).
        return pow(10.0, db_gain / 2000.0);
    }
}

static bool M_RecalculateChannelVolumes(int32_t sound_id)
{
    if (!g_AudioDeviceID || sound_id < 0
        || sound_id >= AUDIO_MAX_ACTIVE_SAMPLES) {
        return false;
    }

    AUDIO_SAMPLE_SOUND *sound = &m_Samples[sound_id];
    sound->volume_l = M_DecibelToMultiplier(
        sound->volume - (sound->pan > 0 ? sound->pan : 0));
    sound->volume_r = M_DecibelToMultiplier(
        sound->volume + (sound->pan < 0 ? sound->pan : 0));

    return true;
}

static int32_t M_ReadAVBuffer(void *opaque, uint8_t *dst, int32_t dst_size)
{
    ASSERT(opaque != nullptr);
    ASSERT(dst != nullptr);
    AUDIO_AV_BUFFER *src = opaque;
    int32_t read = dst_size >= src->remaining ? src->remaining : dst_size;
    if (!read) {
        return AVERROR_EOF;
    }
    memcpy(dst, src->ptr, read);
    src->ptr += read;
    src->remaining -= read;
    return read;
}

static int64_t M_SeekAVBuffer(void *opaque, int64_t offset, int32_t whence)
{
    ASSERT(opaque != nullptr);
    AUDIO_AV_BUFFER *src = opaque;
    if (whence & AVSEEK_SIZE) {
        return src->size;
    }
    switch (whence) {
    case SEEK_SET:
        if (src->size - offset < 0) {
            return AVERROR_EOF;
        }
        src->ptr = src->data + offset;
        src->remaining = src->size - offset;
        break;
    case SEEK_CUR:
        if (src->remaining - offset < 0) {
            return AVERROR_EOF;
        }
        src->ptr += offset;
        src->remaining -= offset;
        break;
    case SEEK_END:
        if (src->size + offset < 0) {
            return AVERROR_EOF;
        }
        src->ptr = src->data - offset;
        src->remaining = src->size + offset;
        break;
    }
    return src->ptr - src->data;
}

static int32_t M_OutputAudioFrame(
    M_SWR_CONTEXT *const swr, AVFrame *const frame)
{
    // Determine the maximum number of output samples this call can produce,
    // based on the current delay already inside the resampler plus the new
    // input. Using av_rescale_rnd() keeps everything in integer domain and
    // avoids cumulative rounding errors.
    const int64_t delay = swr_get_delay(swr->ctx, swr->src.sample_rate);
    const int32_t out_samples = (int32_t)av_rescale_rnd(
        delay + frame->nb_samples, swr->dst.sample_rate, swr->src.sample_rate,
        AV_ROUND_UP);
    if (out_samples <= 0) {
        return 0; // nothing to do
    }

    uint8_t *out_buffer = nullptr;
    if (av_samples_alloc(
            &out_buffer, nullptr, swr->dst.ch_layout.nb_channels, out_samples,
            swr->dst.format, 1)
        < 0) {
        return AVERROR(ENOMEM);
    }

    // Convert – we do *not* drain the resampler here.
    const int32_t converted = swr_convert(
        swr->ctx, &out_buffer, out_samples, (const uint8_t **)frame->data,
        frame->nb_samples);

    if (converted < 0) {
        av_freep(&out_buffer);
        return converted; // propagate error
    }

    if (converted > 0) {
        const int32_t out_buffer_size = av_samples_get_buffer_size(
            nullptr, swr->dst.ch_layout.nb_channels, converted, swr->dst.format,
            1);
        if (out_buffer_size > 0) {
            swr->working_buffer = Memory_Realloc(
                swr->working_buffer,
                swr->working_buffer_size + out_buffer_size);
            memcpy(
                swr->working_buffer + swr->working_buffer_size, out_buffer,
                out_buffer_size);
            swr->working_buffer_size += out_buffer_size;
        }
    }

    av_freep(&out_buffer);
    return 0;
}

static int32_t M_DecodePacket(
    AVCodecContext *const dec, const AVPacket *const pkt, AVFrame *frame,
    M_SWR_CONTEXT *const swr)
{
    // Submit the packet to the decoder
    int32_t ret = avcodec_send_packet(dec, pkt);
    if (ret < 0) {
        LOG_ERROR(
            "Error submitting a packet for decoding (%s)\n", av_err2str(ret));
        return ret;
    }

    // Get all the available frames from the decoder
    while (ret >= 0) {
        ret = avcodec_receive_frame(dec, frame);
        if (ret < 0) {
            // those two return values are special and mean there is no output
            // frame available, but there were no errors during decoding
            if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
                return 0;
            }
            LOG_ERROR(
                "Error receiving a frame for decoding (%s)\n", av_err2str(ret));
            return ret;
        }

        ret = M_OutputAudioFrame(swr, frame);
        av_frame_unref(frame);
    }

    return ret;
}

static bool M_ConvertRawData(
    const uint8_t *const original_data, const int32_t original_size,
    const int32_t dst_sample_rate, const int32_t dst_format,
    const int32_t dst_channel_count, uint8_t **const out_sample_data,
    size_t *const out_size, size_t *const out_sample_count)
{
    bool result = false;

    struct {
        size_t read_buffer_size;
        AVIOContext *avio_context;
        AVStream *stream;
        AVFormatContext *format_ctx;
        const AVCodec *codec;
        AVCodecContext *codec_ctx;
        AVPacket *packet;
        AVFrame *frame;
    } av = {
        .read_buffer_size = 8192,
        .avio_context = nullptr,
        .stream = nullptr,
        .format_ctx = nullptr,
        .codec = nullptr,
        .codec_ctx = nullptr,
        .packet = nullptr,
        .frame = nullptr,
    };

    M_SWR_CONTEXT swr = {};
    int32_t error_code;

    uint8_t *const read_buffer = av_malloc(av.read_buffer_size);
    if (read_buffer == nullptr) {
        error_code = AVERROR(ENOMEM);
        goto cleanup;
    }

    AUDIO_AV_BUFFER av_buf = {
        .data = original_data,
        .ptr = original_data,
        .size = original_size,
        .remaining = original_size,
    };

    av.avio_context = avio_alloc_context(
        read_buffer, av.read_buffer_size, 0, &av_buf, M_ReadAVBuffer, nullptr,
        M_SeekAVBuffer);

    av.format_ctx = avformat_alloc_context();
    av.format_ctx->pb = av.avio_context;
    error_code = avformat_open_input(&av.format_ctx, "mem:", nullptr, nullptr);
    if (error_code != 0) {
        goto cleanup;
    }

    error_code = avformat_find_stream_info(av.format_ctx, nullptr);
    if (error_code < 0) {
        goto cleanup;
    }

    av.stream = nullptr;
    for (uint32_t i = 0; i < av.format_ctx->nb_streams; i++) {
        AVStream *current_stream = av.format_ctx->streams[i];
        if (current_stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            av.stream = current_stream;
            break;
        }
    }
    if (av.stream == nullptr) {
        error_code = AVERROR_STREAM_NOT_FOUND;
        goto cleanup;
    }

    av.codec = avcodec_find_decoder(av.stream->codecpar->codec_id);
    if (av.codec == nullptr) {
        error_code = AVERROR_DEMUXER_NOT_FOUND;
        goto cleanup;
    }

    av.codec_ctx = avcodec_alloc_context3(av.codec);
    if (av.codec_ctx == nullptr) {
        error_code = AVERROR(ENOMEM);
        goto cleanup;
    }

    error_code =
        avcodec_parameters_to_context(av.codec_ctx, av.stream->codecpar);
    if (error_code) {
        goto cleanup;
    }

    error_code = avcodec_open2(av.codec_ctx, av.codec, nullptr);
    if (error_code < 0) {
        goto cleanup;
    }

    av.packet = av_packet_alloc();
    if (av.packet == nullptr) {
        error_code = AVERROR(ENOMEM);
        goto cleanup;
    }

    av.frame = av_frame_alloc();
    if (av.frame == nullptr) {
        error_code = AVERROR(ENOMEM);
        goto cleanup;
    }

    swr.src.sample_rate = av.codec_ctx->sample_rate;
    swr.src.ch_layout = av.codec_ctx->ch_layout;
    swr.src.format = av.codec_ctx->sample_fmt;
    swr.dst.sample_rate = AUDIO_WORKING_RATE;
    av_channel_layout_default(&swr.dst.ch_layout, dst_channel_count);
    swr.dst.format = Audio_GetAVAudioFormat(AUDIO_WORKING_FORMAT);
    swr_alloc_set_opts2(
        &swr.ctx, &swr.dst.ch_layout, swr.dst.format, swr.dst.sample_rate,
        &swr.src.ch_layout, swr.src.format, swr.src.sample_rate, 0, 0);
    if (swr.ctx == nullptr) {
        av_packet_unref(av.packet);
        error_code = AVERROR(ENOMEM);
        goto cleanup;
    }

    error_code = swr_init(swr.ctx);
    if (error_code != 0) {
        av_packet_unref(av.packet);
        goto cleanup;
    }

    while ((error_code = av_read_frame(av.format_ctx, av.packet)) >= 0) {
        M_DecodePacket(av.codec_ctx, av.packet, av.frame, &swr);
        av_packet_unref(av.packet);
        if (error_code < 0) {
            break;
        }
    }

    if (av.codec_ctx != nullptr) {
        M_DecodePacket(av.codec_ctx, nullptr, av.frame, &swr);
    }

    if (error_code == AVERROR_EOF) {
        error_code = 0;
    } else if (error_code < 0) {
        goto cleanup;
    }

    if (out_size != nullptr) {
        *out_size = swr.working_buffer_size;
    }
    if (out_sample_count != nullptr) {
        *out_sample_count = (int32_t)swr.working_buffer_size
            / av_get_bytes_per_sample(swr.dst.format)
            / swr.dst.ch_layout.nb_channels;
    }
    if (out_sample_data != nullptr) {
        *out_sample_data = swr.working_buffer;
    } else {
        Memory_FreePointer(&swr.working_buffer);
    }
    result = true;

cleanup:
    if (error_code != 0) {
        LOG_ERROR("Error while decoding sample: %s", av_err2str(error_code));
    }

    if (!result) {
        if (out_size != nullptr) {
            *out_size = 0;
        }
        if (out_sample_count != nullptr) {
            *out_sample_count = 0;
        }
        if (out_sample_data != nullptr) {
            *out_sample_data = nullptr;
        }
        Memory_FreePointer(&swr.working_buffer);
    }

    if (swr.ctx) {
        swr_free(&swr.ctx);
    }
    if (av.frame) {
        av_frame_free(&av.frame);
    }
    if (av.packet) {
        av_packet_free(&av.packet);
    }
    av.codec = nullptr;
    if (av.codec_ctx) {
        avcodec_free_context(&av.codec_ctx);
    }
    if (av.format_ctx) {
        avformat_close_input(&av.format_ctx);
    }
    if (av.avio_context) {
        av_freep(&av.avio_context->buffer);
        avio_context_free(&av.avio_context);
    }
    return result;
}

static bool M_ConvertSample(const int32_t sample_id)
{
    ASSERT(sample_id >= 0 && sample_id < m_LoadedSamplesCount);
    AUDIO_SAMPLE *const sample = &m_LoadedSamples[sample_id];
    if (sample->sample_data != nullptr) {
        return true;
    }

    size_t num_samples;
    BENCHMARK benchmark = Benchmark_Start();

    const bool result = M_ConvertRawData(
        (uint8_t *)sample->original_data, sample->original_size,
        AUDIO_WORKING_RATE, Audio_GetAVAudioFormat(AUDIO_WORKING_FORMAT), 1,
        (uint8_t **)&sample->sample_data, nullptr, &num_samples);

    char buffer[80];
    sprintf(buffer, "sample %d decoded", sample_id);
    Benchmark_End(&benchmark, buffer);

    sample->channels = 1;
    sample->num_samples = num_samples;
    return result;
}

static bool M_IsOriginalDataDefined(const int32_t sample_id)
{
    ASSERT(sample_id >= 0 && sample_id < m_LoadedSamplesCount);
    const AUDIO_SAMPLE *const sample = &m_LoadedSamples[sample_id];
    return sample->original_data != nullptr;
}

void Audio_Sample_Init(void)
{
    for (int32_t sound_id = 0; sound_id < AUDIO_MAX_ACTIVE_SAMPLES;
         sound_id++) {
        AUDIO_SAMPLE_SOUND *sound = &m_Samples[sound_id];
        sound->is_used = false;
        sound->is_playing = false;
        sound->volume = 0.0f;
        sound->pitch = 1.0f;
        sound->pan = 0.0f;
        sound->current_sample = 0.0f;
        sound->sample = nullptr;
    }
}

void Audio_Sample_Shutdown(void)
{
    Audio_Sample_CloseAll();
    Audio_Sample_UnloadAll();
}

bool Audio_Sample_Unload(const int32_t sample_id)
{
    if (sample_id < 0 || sample_id >= AUDIO_MAX_SAMPLES) {
        LOG_ERROR("Maximum allowed samples: %d", AUDIO_MAX_SAMPLES);
        return false;
    }

    bool result = false;
    AUDIO_SAMPLE *const sample = &m_LoadedSamples[sample_id];
    if (sample->sample_data == nullptr) {
        LOG_ERROR("Sample %d is already unloaded", sample_id);
        return false;
    }
    Memory_FreePointer(&sample->sample_data);
    Memory_FreePointer(&sample->original_data);
    m_LoadedSamplesCount--;
    return true;
}

bool Audio_Sample_UnloadAll(void)
{
    m_LoadedSamplesCount = 0;
    for (int32_t i = 0; i < AUDIO_MAX_SAMPLES; i++) {
        AUDIO_SAMPLE *const sample = &m_LoadedSamples[i];
        Memory_FreePointer(&sample->sample_data);
        Memory_FreePointer(&sample->original_data);
    }
    return true;
}

bool Audio_Sample_Load(
    const int32_t sample_id, const char *const data, const size_t size)
{
    if (data == nullptr || size == 0) {
        LOG_ERROR("Missing sample data %d", sample_id);
        return false;
    }

    if (!g_AudioDeviceID) {
        LOG_ERROR("Unitialized audio device");
        return false;
    }

    if (sample_id < 0 || sample_id >= AUDIO_MAX_SAMPLES) {
        LOG_ERROR("Maximum allowed samples: %d", AUDIO_MAX_SAMPLES);
        return false;
    }

    AUDIO_SAMPLE *const sample = &m_LoadedSamples[sample_id];
    if (sample->original_data != nullptr) {
        LOG_ERROR(
            "Sample %d is already loaded (trying to overwrite with %d bytes)",
            sample_id, size);
        return false;
    }

    sample->original_data = Memory_Alloc(size);
    sample->original_size = size;
    memcpy(sample->original_data, data, size);
    m_LoadedSamplesCount++;
    return true;
}

int32_t Audio_Sample_Play(
    int32_t sample_id, int32_t volume, float pitch, int32_t pan, bool is_looped)
{
    if (!g_AudioDeviceID) {
        LOG_ERROR("audio device is unavailable");
        return false;
    }

    if (sample_id < 0 || sample_id >= m_LoadedSamplesCount) {
        LOG_DEBUG("Invalid sample id: %d", sample_id);
        return AUDIO_NO_SOUND;
    }

    if (!M_IsOriginalDataDefined(sample_id)) {
        return AUDIO_NO_SOUND;
    }

    int32_t result = AUDIO_NO_SOUND;

    Audio_LockDevice();
    for (int32_t sound_id = 0; sound_id < AUDIO_MAX_ACTIVE_SAMPLES;
         sound_id++) {
        AUDIO_SAMPLE_SOUND *sound = &m_Samples[sound_id];
        if (sound->is_used) {
            continue;
        }

        M_ConvertSample(sample_id);

        sound->is_used = true;
        sound->is_playing = true;
        sound->volume = volume;
        sound->pitch = pitch;
        sound->pan = pan;
        sound->is_looped = is_looped;
        sound->current_sample = 0.0f;
        sound->sample = &m_LoadedSamples[sample_id];

        M_RecalculateChannelVolumes(sound_id);

        result = sound_id;
        break;
    }
    Audio_UnlockDevice();

    if (result == AUDIO_NO_SOUND) {
        LOG_ERROR("All sample buffers are used!");
    }

    return result;
}

bool Audio_Sample_IsPlaying(int32_t sound_id)
{
    if (!g_AudioDeviceID || sound_id < 0
        || sound_id >= AUDIO_MAX_ACTIVE_SAMPLES) {
        return false;
    }

    return m_Samples[sound_id].is_playing;
}

bool Audio_Sample_Pause(int32_t sound_id)
{
    if (!g_AudioDeviceID) {
        return false;
    }

    if (m_Samples[sound_id].is_playing) {
        Audio_LockDevice();
        m_Samples[sound_id].is_playing = false;
        Audio_UnlockDevice();
    }

    return true;
}

bool Audio_Sample_PauseAll(void)
{
    if (!g_AudioDeviceID) {
        return false;
    }

    for (int32_t sound_id = 0; sound_id < AUDIO_MAX_ACTIVE_SAMPLES;
         sound_id++) {
        if (m_Samples[sound_id].is_used) {
            Audio_Sample_Pause(sound_id);
        }
    }

    return true;
}

bool Audio_Sample_Unpause(int32_t sound_id)
{
    if (!g_AudioDeviceID) {
        return false;
    }

    if (!m_Samples[sound_id].is_playing) {
        Audio_LockDevice();
        m_Samples[sound_id].is_playing = true;
        Audio_UnlockDevice();
    }

    return true;
}

bool Audio_Sample_UnpauseAll(void)
{
    if (!g_AudioDeviceID) {
        return false;
    }

    for (int32_t sound_id = 0; sound_id < AUDIO_MAX_ACTIVE_SAMPLES;
         sound_id++) {
        if (m_Samples[sound_id].is_used) {
            Audio_Sample_Unpause(sound_id);
        }
    }

    return true;
}

bool Audio_Sample_Close(int32_t sound_id)
{
    if (!g_AudioDeviceID || sound_id < 0
        || sound_id >= AUDIO_MAX_ACTIVE_SAMPLES) {
        return false;
    }

    Audio_LockDevice();
    m_Samples[sound_id].is_used = false;
    m_Samples[sound_id].is_playing = false;
    Audio_UnlockDevice();

    return true;
}

bool Audio_Sample_CloseAll(void)
{
    if (!g_AudioDeviceID) {
        return false;
    }

    for (int32_t sound_id = 0; sound_id < AUDIO_MAX_ACTIVE_SAMPLES;
         sound_id++) {
        if (m_Samples[sound_id].is_used) {
            Audio_Sample_Close(sound_id);
        }
    }

    return true;
}

bool Audio_Sample_SetPan(int32_t sound_id, int32_t pan)
{
    if (!g_AudioDeviceID || sound_id < 0
        || sound_id >= AUDIO_MAX_ACTIVE_SAMPLES) {
        return false;
    }

    Audio_LockDevice();
    m_Samples[sound_id].pan = pan;
    M_RecalculateChannelVolumes(sound_id);
    Audio_UnlockDevice();

    return true;
}

bool Audio_Sample_SetVolume(int32_t sound_id, int32_t volume)
{
    if (!g_AudioDeviceID || sound_id < 0
        || sound_id >= AUDIO_MAX_ACTIVE_SAMPLES) {
        return false;
    }

    Audio_LockDevice();
    m_Samples[sound_id].volume = volume;
    M_RecalculateChannelVolumes(sound_id);
    Audio_UnlockDevice();

    return true;
}

bool Audio_Sample_SetPitch(int32_t sound_id, float pitch)
{
    if (!g_AudioDeviceID || sound_id < 0
        || sound_id >= AUDIO_MAX_ACTIVE_SAMPLES) {
        return false;
    }

    Audio_LockDevice();
    m_Samples[sound_id].pitch = pitch;
    M_RecalculateChannelVolumes(sound_id);
    Audio_UnlockDevice();

    return true;
}

void Audio_Sample_Mix(float *dst_buffer, size_t len)
{
    for (int32_t sound_id = 0; sound_id < AUDIO_MAX_ACTIVE_SAMPLES;
         sound_id++) {
        AUDIO_SAMPLE_SOUND *sound = &m_Samples[sound_id];
        if (!sound->is_playing) {
            continue;
        }

        int32_t samples_requested =
            len / sizeof(AUDIO_WORKING_FORMAT) / AUDIO_WORKING_CHANNELS;
        float src_sample_idx = sound->current_sample;
        const float *src_buffer = sound->sample->sample_data;
        float *dst_ptr = dst_buffer;

        while ((dst_ptr - dst_buffer) / AUDIO_WORKING_CHANNELS
               < samples_requested) {

            // because we handle 3d sound ourselves, downmix to mono
            float src_sample = 0.0f;
            for (int32_t i = 0; i < sound->sample->channels; i++) {
                src_sample += src_buffer
                    [(int32_t)src_sample_idx * sound->sample->channels + i];
            }
            src_sample /= (float)sound->sample->channels;

            *dst_ptr++ += src_sample * sound->volume_l;
            *dst_ptr++ += src_sample * sound->volume_r;
            src_sample_idx += sound->pitch;

            if ((int32_t)src_sample_idx >= sound->sample->num_samples) {
                if (sound->is_looped) {
                    src_sample_idx = 0.0f;
                } else {
                    break;
                }
            }
        }

        sound->current_sample = src_sample_idx;
        if (sound->current_sample >= sound->sample->num_samples
            && !sound->is_looped) {
            Audio_Sample_Close(sound_id);
        }
    }
}
