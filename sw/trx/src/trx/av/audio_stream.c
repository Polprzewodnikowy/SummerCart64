#include <trx/av/audio_internal.h>

#include <trx/core/filesystem.h>
#include <trx/core/log.h>
#include <trx/core/memory.h>
#include <trx/core/utils.h>
#include <trx/debug.h>

#include <SDL2/SDL_audio.h>
#include <SDL2/SDL_error.h>
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
#include <libavutil/rational.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define READ_BUFFER_SIZE                                                       \
    (AUDIO_SAMPLES * AUDIO_WORKING_CHANNELS * sizeof(AUDIO_WORKING_FORMAT))

typedef enum {
    M_STREAM_SRC_NONE,
    M_STREAM_SRC_MEMORY,
} M_STREAM_SOURCE_TYPE;

typedef struct {
    uint8_t *data;
    size_t size;
    size_t pos;
} M_MEM_SOURCE;

typedef struct {
    bool is_used;
    bool is_playing;
    bool is_read_done;
    bool is_looped;
    float volume;
    double duration;
    double decode_timestamp;
    int64_t played_samples;

    double start_at;
    double stop_at;

    void (*finish_callback)(int32_t sound_id, void *user_data);
    void *finish_callback_user_data;

    M_STREAM_SOURCE_TYPE src_type;
    void *src;
    uint8_t *avio_ctx_buffer;
    AVIOContext *avio_ctx;

    struct {
        AVStream *stream;
        AVFormatContext *format_ctx;
        const AVCodec *codec;
        AVCodecContext *codec_ctx;
        AVPacket *packet;
        AVFrame *frame;
    } av;

    struct {
        struct {
            int32_t format;
            AVChannelLayout ch_layout;
            int32_t sample_rate;
        } src, dst;
        SwrContext *ctx;
    } swr;

    struct {
        SDL_AudioStream *stream;
    } sdl;
} AUDIO_STREAM_SOUND;

extern SDL_AudioDeviceID g_AudioDeviceID;

static AUDIO_STREAM_SOUND m_Streams[AUDIO_MAX_ACTIVE_STREAMS] = {};
static float m_MixBuffer[AUDIO_SAMPLES * AUDIO_WORKING_CHANNELS] = {};
static size_t m_DecodeBufferCapacity = 0;
static float *m_DecodeBuffer = nullptr;

static int32_t M_MemoryRead(
    void *const opaque, uint8_t *const buf, const int32_t buf_size)
{
    ASSERT(opaque != nullptr);
    ASSERT(buf != nullptr);

    if (buf_size <= 0) {
        return 0;
    }

    M_MEM_SOURCE *const s = opaque;
    if (s->pos >= s->size) {
        return AVERROR_EOF;
    }

    size_t to_copy = s->size - s->pos;
    if (to_copy > (size_t)buf_size) {
        to_copy = (size_t)buf_size;
    }

    memcpy(buf, s->data + s->pos, to_copy);
    s->pos += to_copy;
    return (int32_t)to_copy;
}

static int64_t M_MemorySeek(
    void *const opaque, const int64_t offset, const int32_t whence)
{
    ASSERT(opaque != nullptr);

    M_MEM_SOURCE *const s = opaque;
    if ((whence & AVSEEK_SIZE) != 0) {
        return (int64_t)s->size;
    }

    const int32_t base_whence = whence & ~AVSEEK_FORCE;
    int64_t base;
    if (base_whence == SEEK_SET) {
        base = 0;
    } else if (base_whence == SEEK_CUR) {
        base = (int64_t)s->pos;
    } else if (base_whence == SEEK_END) {
        base = (int64_t)s->size;
    } else {
        return AVERROR(EINVAL);
    }

    int64_t new_pos = base + offset;
    if (new_pos < 0) {
        new_pos = 0;
    }
    if (new_pos > (int64_t)s->size) {
        new_pos = (int64_t)s->size;
    }
    s->pos = (size_t)new_pos;
    return new_pos;
}

static void M_ResetPlaybackState(
    AUDIO_STREAM_SOUND *const stream, const double relative_timestamp)
{
    ASSERT(stream != nullptr);

    const double clamped = MAX(0.0, relative_timestamp);
    stream->played_samples = (int64_t)(clamped * (double)AUDIO_WORKING_RATE);
}

static void M_DiscardSDLStreamData(AUDIO_STREAM_SOUND *const stream)
{
    ASSERT(stream != nullptr);

    if (stream->sdl.stream != nullptr) {
        while (SDL_AudioStreamAvailable(stream->sdl.stream) > 0) {
            const int32_t bytes_gotten = SDL_AudioStreamGet(
                stream->sdl.stream, m_MixBuffer, READ_BUFFER_SIZE);
            if (bytes_gotten <= 0) {
                break;
            }
        }
    }
}

static void M_SeekToStart(AUDIO_STREAM_SOUND *stream)
{
    ASSERT(stream != nullptr);

    stream->decode_timestamp = stream->start_at;
    M_ResetPlaybackState(stream, 0.0);
    int32_t error_code;
    if (stream->start_at <= 0.0) {
        // reset to start of file
        avio_seek(stream->av.format_ctx->pb, 0, SEEK_SET);
        error_code = avformat_seek_file(
            stream->av.format_ctx, -1, 0, 0, 0, AVSEEK_FLAG_FRAME);
    } else {
        // seek to specific timestamp
        AVFormatContext *const fmt = stream->av.format_ctx;
        if (fmt->pb != nullptr && (fmt->pb->seekable & AVIO_SEEKABLE_NORMAL)) {
            const int64_t ts = (int64_t)(stream->start_at * AV_TIME_BASE);
            error_code = avformat_seek_file(
                fmt, stream->av.stream->index, INT64_MIN, ts, INT64_MAX,
                AVSEEK_FLAG_BACKWARD);
        } else {
            // fallback to stream-based seek
            const double time_base_sec = av_q2d(stream->av.stream->time_base);
            error_code = av_seek_frame(
                fmt, stream->av.stream->index,
                (int64_t)(stream->start_at / time_base_sec), AVSEEK_FLAG_ANY);
        }
    }
    if (error_code < 0) {
        LOG_ERROR(
            "seek failed for timestamp %f: %s", stream->decode_timestamp,
            av_err2str(error_code));
    } else {
        avcodec_flush_buffers(stream->av.codec_ctx);
        M_DiscardSDLStreamData(stream);
        stream->is_read_done = false;
    }
}

static bool M_DecodeFrame(AUDIO_STREAM_SOUND *stream)
{
    ASSERT(stream != nullptr);

    if (stream->stop_at > 0.0 && stream->decode_timestamp >= stream->stop_at) {
        if (stream->is_looped) {
            M_SeekToStart(stream);
            return M_DecodeFrame(stream);
        } else {
            return false;
        }
    }

    // av_read_frame() overwrites the packet; always unref any previous content.
    av_packet_unref(stream->av.packet);
    int32_t error_code =
        av_read_frame(stream->av.format_ctx, stream->av.packet);

    if (error_code == AVERROR_EOF && stream->is_looped) {
        M_SeekToStart(stream);
        return M_DecodeFrame(stream);
    }

    if (error_code == AVERROR_EOF) {
        return false;
    }

    if (error_code < 0) {
        LOG_ERROR(
            "error while decoding audio stream: %d (%s)", error_code,
            av_err2str(error_code));
        return false;
    }

    if (stream->av.packet->stream_index != stream->av.stream->index) {
        return true;
    }

    error_code = avcodec_send_packet(stream->av.codec_ctx, stream->av.packet);
    if (error_code < 0) {
        av_packet_unref(stream->av.packet);
        LOG_ERROR(
            "Got an error when decoding frame: %s", av_err2str(error_code));
        return false;
    }

    return true;
}

static bool M_InitialiseFromFormatContext(
    int32_t sound_id, AVFormatContext *const fmt_ctx)
{
    if (!g_AudioDeviceID || sound_id < 0
        || sound_id >= AUDIO_MAX_ACTIVE_STREAMS) {
        return false;
    }

    bool ret = false;
    Audio_LockDevice();

    AUDIO_STREAM_SOUND *stream = &m_Streams[sound_id];
    int32_t error_code = 0;

    stream->av.format_ctx = fmt_ctx;

    error_code = avformat_find_stream_info(stream->av.format_ctx, nullptr);
    if (error_code < 0) {
        goto cleanup;
    }

    stream->av.stream = nullptr;
    for (uint32_t i = 0; i < stream->av.format_ctx->nb_streams; i++) {
        AVStream *current_stream = stream->av.format_ctx->streams[i];
        if (current_stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            stream->av.stream = current_stream;
            break;
        }
    }
    if (!stream->av.stream) {
        error_code = AVERROR_STREAM_NOT_FOUND;
        goto cleanup;
    }

    stream->av.codec =
        avcodec_find_decoder(stream->av.stream->codecpar->codec_id);
    if (!stream->av.codec) {
        error_code = AVERROR_DEMUXER_NOT_FOUND;
        goto cleanup;
    }

    stream->av.codec_ctx = avcodec_alloc_context3(stream->av.codec);
    if (!stream->av.codec_ctx) {
        error_code = AVERROR(ENOMEM);
        goto cleanup;
    }

    error_code = avcodec_parameters_to_context(
        stream->av.codec_ctx, stream->av.stream->codecpar);
    if (error_code != 0) {
        goto cleanup;
    }

    error_code = avcodec_open2(stream->av.codec_ctx, stream->av.codec, nullptr);
    if (error_code < 0) {
        goto cleanup;
    }

    stream->av.packet = av_packet_alloc();
    if (!stream->av.packet) {
        error_code = AVERROR(ENOMEM);
        goto cleanup;
    }

    stream->av.frame = av_frame_alloc();
    if (!stream->av.frame) {
        error_code = AVERROR(ENOMEM);
        goto cleanup;
    }

    M_DecodeFrame(stream);

    const int32_t sdl_channels = stream->av.codec_ctx->ch_layout.nb_channels;

    stream->is_read_done = false;
    stream->is_used = true;
    stream->is_playing = true;
    stream->is_looped = false;
    stream->volume = 1.0f;
    stream->decode_timestamp = 0.0;
    stream->played_samples = 0;
    stream->finish_callback = nullptr;
    stream->finish_callback_user_data = nullptr;
    stream->duration =
        (double)stream->av.format_ctx->duration / (double)AV_TIME_BASE;
    stream->start_at = -1.0; // negative value means unset
    stream->stop_at = -1.0; // negative value means unset

    stream->sdl.stream = SDL_NewAudioStream(
        AUDIO_WORKING_FORMAT, sdl_channels, AUDIO_WORKING_RATE,
        AUDIO_WORKING_FORMAT, sdl_channels, AUDIO_WORKING_RATE);
    if (!stream->sdl.stream) {
        LOG_ERROR("Failed to create SDL stream: %s", SDL_GetError());
        goto cleanup;
    }

    ret = true;

cleanup:
    if (error_code != 0) {
        LOG_ERROR(
            "Error while opening audio stream: %s", av_err2str(error_code));
    }

    if (!ret) {
        Audio_Stream_Close(sound_id);
    }

    Audio_UnlockDevice();
    return ret;
}

static bool M_EnqueueFrame(AUDIO_STREAM_SOUND *stream)
{
    ASSERT(stream != nullptr);

    int32_t error_code;

    if (!stream->swr.ctx) {
        stream->swr.src.sample_rate = stream->av.codec_ctx->sample_rate;
        stream->swr.src.ch_layout = stream->av.codec_ctx->ch_layout;
        stream->swr.src.format = stream->av.codec_ctx->sample_fmt;
        stream->swr.dst.sample_rate = AUDIO_WORKING_RATE;
        av_channel_layout_default(
            &stream->swr.dst.ch_layout, AUDIO_WORKING_CHANNELS);
        stream->swr.dst.format = Audio_GetAVAudioFormat(AUDIO_WORKING_FORMAT);
        swr_alloc_set_opts2(
            &stream->swr.ctx, &stream->swr.dst.ch_layout,
            stream->swr.dst.format, stream->swr.dst.sample_rate,
            &stream->swr.src.ch_layout, stream->swr.src.format,
            stream->swr.src.sample_rate, 0, 0);
        if (!stream->swr.ctx) {
            av_packet_unref(stream->av.packet);
            error_code = AVERROR(ENOMEM);
            goto cleanup;
        }

        error_code = swr_init(stream->swr.ctx);
        if (error_code != 0) {
            av_packet_unref(stream->av.packet);
            goto cleanup;
        }
    }

    while (1) {
        error_code =
            avcodec_receive_frame(stream->av.codec_ctx, stream->av.frame);
        if (error_code == AVERROR(EAGAIN)) {
            av_frame_unref(stream->av.frame);
            error_code = 0;
            break;
        }

        if (error_code < 0) {
            av_frame_unref(stream->av.frame);
            break;
        }

        uint8_t *out_buffer = nullptr;
        const int32_t out_samples =
            swr_get_out_samples(stream->swr.ctx, stream->av.frame->nb_samples);
        av_samples_alloc(
            &out_buffer, nullptr, stream->swr.dst.ch_layout.nb_channels,
            out_samples, stream->swr.dst.format, 1);
        int32_t resampled_size = swr_convert(
            stream->swr.ctx, &out_buffer, out_samples,
            (const uint8_t **)stream->av.frame->data,
            stream->av.frame->nb_samples);

        size_t out_pos = 0;
        while (resampled_size > 0) {
            const size_t out_buffer_size = av_samples_get_buffer_size(
                nullptr, stream->swr.dst.ch_layout.nb_channels, resampled_size,
                stream->swr.dst.format, 1);

            if (out_pos + out_buffer_size > m_DecodeBufferCapacity) {
                m_DecodeBufferCapacity = out_pos + out_buffer_size;
                m_DecodeBuffer =
                    Memory_Realloc(m_DecodeBuffer, m_DecodeBufferCapacity);
            }
            if (m_DecodeBuffer != nullptr && out_buffer != nullptr) {
                memcpy(
                    (uint8_t *)m_DecodeBuffer + out_pos, out_buffer,
                    out_buffer_size);
            }
            out_pos += out_buffer_size;

            resampled_size = swr_convert(
                stream->swr.ctx, &out_buffer, out_samples, nullptr, 0);
        }

        if (SDL_AudioStreamPut(stream->sdl.stream, m_DecodeBuffer, out_pos)) {
            LOG_ERROR("Got an error when decoding frame: %s", SDL_GetError());
            av_frame_unref(stream->av.frame);
            break;
        }

        ASSERT(stream->av.format_ctx != nullptr);
        ASSERT(stream->av.codec_ctx != nullptr);
        ASSERT(stream->av.stream != nullptr);
        double time_base_sec = av_q2d(stream->av.stream->time_base);
        stream->decode_timestamp =
            stream->av.frame->best_effort_timestamp * time_base_sec;
        av_freep(&out_buffer);
        av_frame_unref(stream->av.frame);
    }

    av_packet_unref(stream->av.packet);

cleanup:
    if (error_code > 0) {
        LOG_ERROR(
            "Got an error when decoding frame: %d, %s", error_code,
            av_err2str(error_code));
    }

    return true;
}

static bool M_InitialiseFromPath(int32_t sound_id, const char *file_path)
{
    ASSERT(file_path != nullptr);

    if (!g_AudioDeviceID || sound_id < 0
        || sound_id >= AUDIO_MAX_ACTIVE_STREAMS) {
        return false;
    }

    int32_t error_code = 0;
    AVFormatContext *fmt_ctx = nullptr;
    error_code = avformat_open_input(&fmt_ctx, file_path, nullptr, nullptr);
    if (error_code != 0) {
        LOG_ERROR(
            "Error while opening audio %s: %s", file_path,
            av_err2str(error_code));
        return false;
    }

    return M_InitialiseFromFormatContext(sound_id, fmt_ctx);
}

static void M_Clear(AUDIO_STREAM_SOUND *stream)
{
    ASSERT(stream != nullptr);

    stream->is_used = false;
    stream->is_playing = false;
    stream->is_read_done = true;
    stream->is_looped = false;
    stream->volume = 0.0f;
    stream->duration = 0.0;
    stream->decode_timestamp = 0.0;
    stream->played_samples = 0;
    stream->sdl.stream = nullptr;
    stream->finish_callback = nullptr;
    stream->finish_callback_user_data = nullptr;

    stream->src_type = M_STREAM_SRC_NONE;
    stream->src = nullptr;
    stream->avio_ctx_buffer = nullptr;
    stream->avio_ctx = nullptr;
}

void Audio_Stream_Init(void)
{
    for (int32_t sound_id = 0; sound_id < AUDIO_MAX_ACTIVE_STREAMS;
         sound_id++) {
        M_Clear(&m_Streams[sound_id]);
    }
}

void Audio_Stream_Shutdown(void)
{
    Memory_FreePointer(&m_DecodeBuffer);
    m_DecodeBufferCapacity = 0;
    if (!g_AudioDeviceID) {
        return;
    }

    for (int32_t sound_id = 0; sound_id < AUDIO_MAX_ACTIVE_STREAMS;
         sound_id++) {
        if (m_Streams[sound_id].is_used) {
            Audio_Stream_Close(sound_id);
        }
    }
}

bool Audio_Stream_SyncTimestamp(const int32_t sound_id, const double timestamp)
{
    if (!g_AudioDeviceID || sound_id < 0
        || sound_id >= AUDIO_MAX_ACTIVE_STREAMS) {
        return false;
    }

    AUDIO_STREAM_SOUND *const stream = &m_Streams[sound_id];
    double drift = Audio_Stream_GetTimestamp(sound_id) - timestamp;
    if (drift < 0) {
        drift = -drift;
    }
    if (drift >= AUDIO_DRIFT_THRESHOLD) {
        LOG_DEBUG("Detected audio drift: %f s", drift);
        Audio_Stream_SeekTimestamp(sound_id, timestamp);
        return true;
    }
    return false;
}

bool Audio_Stream_Pause(int32_t sound_id)
{
    if (!g_AudioDeviceID || sound_id < 0
        || sound_id >= AUDIO_MAX_ACTIVE_STREAMS) {
        return false;
    }

    AUDIO_STREAM_SOUND *const stream = &m_Streams[sound_id];
    if (stream->is_playing) {
        Audio_LockDevice();
        stream->is_playing = false;
        Audio_UnlockDevice();
    }

    return true;
}

bool Audio_Stream_Unpause(int32_t sound_id)
{
    if (!g_AudioDeviceID || sound_id < 0
        || sound_id >= AUDIO_MAX_ACTIVE_STREAMS) {
        return false;
    }

    AUDIO_STREAM_SOUND *const stream = &m_Streams[sound_id];
    if (!stream->is_playing) {
        Audio_LockDevice();
        stream->is_playing = true;
        Audio_UnlockDevice();
    }

    return true;
}

int32_t Audio_Stream_CreateFromFile(const char *file_path)
{
    if (!g_AudioDeviceID) {
        return AUDIO_NO_SOUND;
    }

    ASSERT(file_path != nullptr);

    for (int32_t sound_id = 0; sound_id < AUDIO_MAX_ACTIVE_STREAMS;
         sound_id++) {
        AUDIO_STREAM_SOUND *stream = &m_Streams[sound_id];
        if (stream->is_used) {
            continue;
        }

        if (!M_InitialiseFromPath(sound_id, file_path)) {
            return AUDIO_NO_SOUND;
        }

        return sound_id;
    }

    return AUDIO_NO_SOUND;
}

int32_t Audio_Stream_CreateFromMemory(uint8_t *const data, const size_t size)
{
    if (!g_AudioDeviceID) {
        return AUDIO_NO_SOUND;
    }

    ASSERT(data != nullptr);
    ASSERT(size != 0);

    for (int32_t sound_id = 0; sound_id < AUDIO_MAX_ACTIVE_STREAMS;
         sound_id++) {
        AUDIO_STREAM_SOUND *stream = &m_Streams[sound_id];
        if (stream->is_used) {
            continue;
        }

        M_MEM_SOURCE *const src = Memory_Alloc(sizeof(M_MEM_SOURCE));
        *src = (M_MEM_SOURCE) {
            .data = data,
            .size = size,
            .pos = 0,
        };

        stream->src_type = M_STREAM_SRC_MEMORY;
        stream->src = src;

        stream->avio_ctx_buffer = av_malloc(4096);
        if (stream->avio_ctx_buffer == nullptr) {
            Audio_Stream_Close(sound_id);
            return AUDIO_NO_SOUND;
        }

        stream->avio_ctx = avio_alloc_context(
            stream->avio_ctx_buffer, 4096, 0, src, M_MemoryRead, nullptr,
            M_MemorySeek);
        if (stream->avio_ctx == nullptr) {
            Audio_Stream_Close(sound_id);
            return AUDIO_NO_SOUND;
        }

        stream->av.format_ctx = avformat_alloc_context();
        if (stream->av.format_ctx == nullptr) {
            Audio_Stream_Close(sound_id);
            return AUDIO_NO_SOUND;
        }
        stream->av.format_ctx->pb = stream->avio_ctx;
        stream->av.format_ctx->flags |= AVFMT_FLAG_CUSTOM_IO;

        int32_t error_code = avformat_open_input(
            &stream->av.format_ctx, nullptr, nullptr, nullptr);
        if (error_code != 0) {
            LOG_ERROR(
                "Error while opening audio memory stream: %s",
                av_err2str(error_code));
            Audio_Stream_Close(sound_id);
            return AUDIO_NO_SOUND;
        }

        if (!M_InitialiseFromFormatContext(sound_id, stream->av.format_ctx)) {
            Audio_Stream_Close(sound_id);
            return AUDIO_NO_SOUND;
        }

        return sound_id;
    }

    return AUDIO_NO_SOUND;
}

bool Audio_Stream_Close(int32_t sound_id)
{
    if (!g_AudioDeviceID || sound_id < 0
        || sound_id >= AUDIO_MAX_ACTIVE_STREAMS) {
        return false;
    }

    Audio_LockDevice();

    AUDIO_STREAM_SOUND *stream = &m_Streams[sound_id];

    if (stream->av.codec_ctx) {
        // XXX: potential libav bug - avcodec_close should free this info
        if (stream->av.codec_ctx->extradata != nullptr) {
            av_freep(&stream->av.codec_ctx->extradata);
        }

        avcodec_free_context(&stream->av.codec_ctx);
        stream->av.codec_ctx = nullptr;
    }

    if (stream->av.format_ctx) {
        avformat_close_input(&stream->av.format_ctx);
        stream->av.format_ctx = nullptr;
    }

    if (stream->avio_ctx != nullptr) {
        av_freep(&stream->avio_ctx->buffer);
        avio_context_free(&stream->avio_ctx);
        stream->avio_ctx = nullptr;
    } else if (stream->avio_ctx_buffer != nullptr) {
        av_freep(&stream->avio_ctx_buffer);
    }
    stream->avio_ctx_buffer = nullptr;

    if (stream->src_type == M_STREAM_SRC_MEMORY && stream->src != nullptr) {
        M_MEM_SOURCE *const src = stream->src;
        Memory_FreePointer(&src->data);
        Memory_FreePointer(&stream->src);
    }

    if (stream->swr.ctx) {
        swr_free(&stream->swr.ctx);
    }

    if (stream->av.frame) {
        av_frame_free(&stream->av.frame);
        stream->av.frame = nullptr;
    }

    if (stream->av.packet) {
        av_packet_free(&stream->av.packet);
        stream->av.packet = nullptr;
    }

    stream->av.stream = nullptr;
    stream->av.codec = nullptr;

    if (stream->sdl.stream) {
        SDL_FreeAudioStream(stream->sdl.stream);
        stream->sdl.stream = nullptr;
    }

    void (*finish_callback)(int32_t, void *) = stream->finish_callback;
    void *finish_callback_user_data = stream->finish_callback_user_data;

    M_Clear(stream);

    Audio_UnlockDevice();

    if (finish_callback) {
        finish_callback(sound_id, finish_callback_user_data);
    }

    return true;
}

bool Audio_Stream_SetVolume(int32_t sound_id, float volume)
{
    if (!g_AudioDeviceID || sound_id < 0
        || sound_id >= AUDIO_MAX_ACTIVE_STREAMS) {
        return false;
    }

    m_Streams[sound_id].volume = volume;

    return true;
}

bool Audio_Stream_IsLooped(int32_t sound_id)
{
    if (!g_AudioDeviceID || sound_id < 0
        || sound_id >= AUDIO_MAX_ACTIVE_STREAMS) {
        return false;
    }

    return m_Streams[sound_id].is_looped;
}

bool Audio_Stream_SetIsLooped(int32_t sound_id, bool is_looped)
{
    if (!g_AudioDeviceID || sound_id < 0
        || sound_id >= AUDIO_MAX_ACTIVE_STREAMS) {
        return false;
    }

    m_Streams[sound_id].is_looped = is_looped;

    return true;
}

bool Audio_Stream_SetFinishCallback(
    int32_t sound_id, void (*callback)(int32_t sound_id, void *user_data),
    void *user_data)
{
    if (!g_AudioDeviceID || sound_id < 0
        || sound_id >= AUDIO_MAX_ACTIVE_STREAMS) {
        return false;
    }

    m_Streams[sound_id].finish_callback = callback;
    m_Streams[sound_id].finish_callback_user_data = user_data;

    return true;
}

void Audio_Stream_Mix(float *dst_buffer, size_t len)
{
    for (int32_t sound_id = 0; sound_id < AUDIO_MAX_ACTIVE_STREAMS;
         sound_id++) {
        AUDIO_STREAM_SOUND *const stream = &m_Streams[sound_id];
        if (!stream->is_playing) {
            continue;
        }

        while ((SDL_AudioStreamAvailable(stream->sdl.stream) < (int32_t)len)
               && !stream->is_read_done) {
            if (M_DecodeFrame(stream)) {
                M_EnqueueFrame(stream);
            } else {
                stream->is_read_done = true;
            }
        }

        memset(m_MixBuffer, 0, READ_BUFFER_SIZE);
        int32_t bytes_gotten = SDL_AudioStreamGet(
            stream->sdl.stream, m_MixBuffer, READ_BUFFER_SIZE);
        if (bytes_gotten < 0) {
            LOG_ERROR("Error reading from sdl.stream: %s", SDL_GetError());
            stream->is_playing = false;
            stream->is_used = false;
            stream->is_read_done = true;
        } else if (bytes_gotten == 0) {
            // legit end of stream. looping is handled in
            // M_DecodeFrame
            stream->is_playing = false;
            stream->is_used = false;
            stream->is_read_done = true;
        } else {
            int32_t samples_gotten = bytes_gotten
                / (AUDIO_WORKING_CHANNELS * sizeof(AUDIO_WORKING_FORMAT));
            stream->played_samples += (int64_t)samples_gotten;

            const float *src_ptr = &m_MixBuffer[0];
            float *dst_ptr = dst_buffer;

            for (int32_t s = 0; s < samples_gotten; s++) {
                for (int32_t c = 0; c < AUDIO_WORKING_CHANNELS; c++) {
                    *dst_ptr++ += *src_ptr++ * stream->volume;
                }
            }
        }

        if (!stream->is_used) {
            Audio_Stream_Close(sound_id);
        }
    }
}

double Audio_Stream_GetTimestamp(int32_t sound_id)
{
    if (!g_AudioDeviceID || sound_id < 0
        || sound_id >= AUDIO_MAX_ACTIVE_STREAMS) {
        return -1.0;
    }

    double timestamp = -1.0;
    AUDIO_STREAM_SOUND *stream = &m_Streams[sound_id];

    if (stream->duration > 0.0) {
        Audio_LockDevice();
        timestamp = (double)stream->played_samples / (double)AUDIO_WORKING_RATE;
        Audio_UnlockDevice();
    }

    return timestamp;
}

double Audio_Stream_GetDuration(int32_t sound_id)
{
    if (!g_AudioDeviceID || sound_id < 0
        || sound_id >= AUDIO_MAX_ACTIVE_STREAMS) {
        return -1.0;
    }

    Audio_LockDevice();
    AUDIO_STREAM_SOUND *stream = &m_Streams[sound_id];
    double duration = stream->duration;
    Audio_UnlockDevice();
    return duration;
}

bool Audio_Stream_SeekTimestamp(const int32_t sound_id, const double timestamp)
{
    if (!g_AudioDeviceID || sound_id < 0
        || sound_id >= AUDIO_MAX_ACTIVE_STREAMS) {
        return false;
    }

    AUDIO_STREAM_SOUND *const stream = &m_Streams[sound_id];
    if (!stream->is_used) {
        return false;
    }

    Audio_LockDevice();
    const double time_base_sec = av_q2d(stream->av.stream->time_base);
    if (time_base_sec <= 0.0) {
        LOG_ERROR(
            "Audio_Stream_SeekTimestamp: invalid time_base %f", time_base_sec);
        Audio_UnlockDevice();
        return false;
    }

    const int32_t stream_index = stream->av.stream->index;
    const int64_t seek_target =
        (int64_t)((MAX(0.0f, stream->start_at) + timestamp) / time_base_sec);
    const int32_t error_code = av_seek_frame(
        stream->av.format_ctx, stream_index, seek_target, AVSEEK_FLAG_ANY);
    if (error_code < 0) {
        LOG_ERROR(
            "seek failed for timestamp %f: %s", timestamp,
            av_err2str(error_code));
        Audio_UnlockDevice();
        return false;
    }

    avcodec_flush_buffers(stream->av.codec_ctx);
    if (stream->sdl.stream != nullptr) {
        M_DiscardSDLStreamData(stream);
    }

    stream->decode_timestamp = timestamp + MAX(stream->start_at, 0.0f);
    M_ResetPlaybackState(stream, timestamp);
    stream->is_read_done = false;

    Audio_UnlockDevice();
    return true;
}

bool Audio_Stream_SetStartTimestamp(int32_t sound_id, double timestamp)
{
    if (!g_AudioDeviceID || sound_id < 0
        || sound_id >= AUDIO_MAX_ACTIVE_STREAMS) {
        return false;
    }

    m_Streams[sound_id].start_at = timestamp;
    return true;
}

bool Audio_Stream_SetStopTimestamp(int32_t sound_id, double timestamp)
{
    if (!g_AudioDeviceID || sound_id < 0
        || sound_id >= AUDIO_MAX_ACTIVE_STREAMS) {
        return false;
    }

    m_Streams[sound_id].stop_at = timestamp;
    return true;
}
