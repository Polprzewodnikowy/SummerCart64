/*
 * Copyright (c) 2003 Fabrice Bellard
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <trx/av/video.h>

#include <trx/core/filesystem.h>
#include <trx/core/log.h>
#include <trx/core/memory.h>
#include <trx/core/strings.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>
#include <SDL2/SDL_error.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_mutex.h>
#include <SDL2/SDL_pixels.h>
#include <SDL2/SDL_rect.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_stdinc.h>
#include <SDL2/SDL_thread.h>
#include <SDL2/SDL_video.h>
#include <errno.h>
#include <libavcodec/avcodec.h>
#include <libavcodec/codec.h>
#include <libavcodec/codec_id.h>
#include <libavcodec/codec_par.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/attributes.h>
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libavutil/common.h>
#include <libavutil/dict.h>
#include <libavutil/error.h>
#include <libavutil/fifo.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libavutil/macros.h>
#include <libavutil/mathematics.h>
#include <libavutil/mem.h>
#include <libavutil/pixfmt.h>
#include <libavutil/rational.h>
#include <libavutil/samplefmt.h>
#include <libavutil/time.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MAX_QUEUE_SIZE (15 * 1024 * 1024)
#define MIN_FRAMES 25
#define SDL_AUDIO_MIN_BUFFER_SIZE 512
#define SDL_AUDIO_MAX_CALLBACKS_PER_SEC 30
#define AV_SYNC_THRESHOLD_MIN 0.04
#define AV_SYNC_THRESHOLD_MAX 0.1
#define AV_SYNC_FRAMEDUP_THRESHOLD 0.1
#define AV_NOSYNC_THRESHOLD 10.0
#define SAMPLE_CORRECTION_PERCENT_MAX 10
#define AUDIO_DIFF_AVG_NB 20
#define REFRESH_RATE 0.01
#define SAMPLE_ARRAY_SIZE (8 * 65536)
#define VIDEO_PICTURE_QUEUE_SIZE 3
#define SAMPLE_QUEUE_SIZE 9
#define FRAME_QUEUE_SIZE FFMAX(SAMPLE_QUEUE_SIZE, VIDEO_PICTURE_QUEUE_SIZE)
#define FF_QUIT_EVENT (SDL_USEREVENT + 2)

typedef struct {
    AVPacket *pkt;
    int serial;
} M_PACKET_LIST;

typedef struct {
    AVFifo *pkt_list;
    int nb_packets;
    int size;
    int64_t duration;
    bool abort_request;
    int serial;
    SDL_mutex *mutex;
    SDL_cond *cond;
} M_PACKET_QUEUE;

typedef struct {
    int freq;
    int channels;
    AVChannelLayout ch_layout;
    enum AVSampleFormat fmt;
    int frame_size;
    int bytes_per_sec;
} M_AUDIO_PARAMS;

typedef struct {
    double pts;
    double pts_drift;
    double last_updated;
    double speed;
    int serial;
    bool paused;
    int *queue_serial;
} M_CLOCK;

typedef struct {
    AVFrame *frame;
    int serial;
    double pts;
    double duration;
    int64_t pos;
    int width;
    int height;
    int format;
    AVRational sar;
} M_FRAME;

typedef struct {
    int64_t pkt_pos;
} M_FRAME_DATA;

typedef struct {
    M_FRAME queue[FRAME_QUEUE_SIZE];
    int rindex;
    int windex;
    int size;
    int max_size;
    int keep_last;
    int rindex_shown;
    SDL_mutex *mutex;
    SDL_cond *cond;
    M_PACKET_QUEUE *pktq;
} M_FRAME_QUEUE;

typedef enum {
    AV_SYNC_AUDIO_MASTER,
    AV_SYNC_VIDEO_MASTER,
    AV_SYNC_EXTERNAL_CLOCK,
} M_CLOCK_TYPE;

typedef struct {
    AVPacket *pkt;
    M_PACKET_QUEUE *queue;
    AVCodecContext *avctx;
    int pkt_serial;
    int finished;
    bool packet_pending;
    SDL_cond *empty_queue_cond;
    int64_t start_pts;
    AVRational start_pts_tb;
    int64_t next_pts;
    AVRational next_pts_tb;
    SDL_Thread *decoder_tid;
} M_DECODER;

typedef struct {
    SDL_Thread *read_tid;
    AVInputFormat *iformat;
    bool abort_request;
    bool playback_finished;
    bool force_refresh;
    bool paused;
    bool last_paused;
    bool queue_attachments_req;
    int read_pause_return;
    AVFormatContext *ic;
    double remaining_time;

    M_CLOCK audclk;
    M_CLOCK vidclk;
    M_CLOCK extclk;

    M_FRAME_QUEUE pictq;
    M_FRAME_QUEUE sampq;

    M_DECODER auddec;
    M_DECODER viddec;

    int audio_stream;

    int av_sync_type;

    double audio_clock;
    int audio_clock_serial;
    double audio_diff_cum;
    double audio_diff_avg_coef;
    double audio_diff_threshold;
    int audio_diff_avg_count;
    AVStream *audio_st;
    M_PACKET_QUEUE audioq;
    int audio_hw_buf_size;
    uint8_t *audio_buf;
    uint8_t *audio_buf1;
    unsigned int audio_buf_size;
    unsigned int audio_buf1_size;
    int audio_buf_index;
    int audio_write_buf_size;
    int audio_volume;
    M_AUDIO_PARAMS audio_src;
    M_AUDIO_PARAMS audio_tgt;
    struct SwrContext *swr_ctx;
    int frame_drops_early;
    int frame_drops_late;

    // surface size at the size of the display buffer
    int surface_width;
    int surface_height;
    // target surface coordinates, keeping the video A:R
    int target_surface_x;
    int target_surface_y;
    int target_surface_width;
    int target_surface_height;

    double frame_timer;
    double frame_last_returned_time;
    int video_stream;
    AVStream *video_st;
    M_PACKET_QUEUE videoq;
    double max_frame_duration; // maximum duration of a frame - above this, we
                               // consider the jump a timestamp discontinuity
    struct SwsContext *img_convert_ctx;
    bool eof;

    char *filename;
    int display_width;
    int display_height;

    SDL_cond *continue_read_thread;

    void *primary_surface;
    enum AVPixelFormat primary_surface_pixel_format;
    int32_t primary_surface_stride;

    VIDEO_SURFACE_ALLOCATOR_FUNC surface_allocator_func;
    void *surface_allocator_func_user_data;

    void (*surface_deallocator_func)(void *surface, void *user_data);
    void *surface_deallocator_func_user_data;

    void (*surface_clear_func)(void *surface, void *user_data);
    void *surface_clear_func_user_data;

    void (*render_begin_func)(void *surface, void *user_data);
    void *render_begin_func_user_data;

    void (*render_end_func)(void *surface, void *user_data);
    void *render_end_func_user_data;

    void *(*surface_lock_func)(void *surface, void *user_data);
    void *surface_lock_func_user_data;

    void (*surface_unlock_func)(void *surface, void *user_data);
    void *surface_unlock_func_user_data;

    void (*surface_upload_func)(void *surface, void *user_data);
    void *surface_upload_func_user_data;

    bool audio_enabled;
} M_STATE;

static int64_t m_AudioCallbackTime;
static SDL_AudioDeviceID m_AudioDevice = 0;

static int M_PacketQueuePutPrivate(M_PACKET_QUEUE *q, AVPacket *pkt)
{
    M_PACKET_LIST pkt1;

    if (q->abort_request) {
        return -1;
    }

    pkt1.pkt = pkt;
    pkt1.serial = q->serial;

    int32_t ret = av_fifo_write(q->pkt_list, &pkt1, 1);
    if (ret < 0) {
        return ret;
    }
    q->nb_packets++;
    q->size += pkt1.pkt->size + sizeof(pkt1);
    q->duration += pkt1.pkt->duration;
    SDL_CondSignal(q->cond);
    return 0;
}

static int M_PacketQueuePut(M_PACKET_QUEUE *q, AVPacket *pkt)
{
    AVPacket *pkt1;
    int ret;

    pkt1 = av_packet_alloc();
    if (!pkt1) {
        av_packet_unref(pkt);
        return -1;
    }
    av_packet_move_ref(pkt1, pkt);

    SDL_LockMutex(q->mutex);
    ret = M_PacketQueuePutPrivate(q, pkt1);
    SDL_UnlockMutex(q->mutex);

    if (ret < 0) {
        av_packet_free(&pkt1);
    }

    return ret;
}

static int M_PacketQueuePutNullPacket(
    M_PACKET_QUEUE *q, AVPacket *pkt, int stream_index)
{
    pkt->stream_index = stream_index;
    return M_PacketQueuePut(q, pkt);
}

static int M_PacketQueueInit(M_PACKET_QUEUE *q)
{
    memset(q, 0, sizeof(M_PACKET_QUEUE));
    q->pkt_list =
        av_fifo_alloc2(1, sizeof(M_PACKET_LIST), AV_FIFO_FLAG_AUTO_GROW);
    if (q->pkt_list == nullptr) {
        return AVERROR(ENOMEM);
    }

    q->mutex = SDL_CreateMutex();
    if (!q->mutex) {
        LOG_ERROR("SDL_CreateMutex(): %s", SDL_GetError());
        return AVERROR(ENOMEM);
    }

    q->cond = SDL_CreateCond();
    if (!q->cond) {
        LOG_ERROR("SDL_CreateCond(): %s", SDL_GetError());
        return AVERROR(ENOMEM);
    }

    q->abort_request = true;
    return 0;
}

static void M_PacketQueueFlush(M_PACKET_QUEUE *q)
{
    if (q == nullptr || q->mutex == nullptr || q->pkt_list == nullptr) {
        return;
    }

    M_PACKET_LIST pkt1;

    SDL_LockMutex(q->mutex);
    while (av_fifo_read(q->pkt_list, &pkt1, 1) >= 0) {
        av_packet_free(&pkt1.pkt);
    }
    q->nb_packets = 0;
    q->size = 0;
    q->duration = 0;
    q->serial++;
    SDL_UnlockMutex(q->mutex);
}

static void M_PacketQueueDestroy(M_PACKET_QUEUE *q)
{
    if (q == nullptr) {
        return;
    }

    M_PacketQueueFlush(q);
    av_fifo_freep2(&q->pkt_list);

    if (q->mutex != nullptr) {
        SDL_DestroyMutex(q->mutex);
        q->mutex = nullptr;
    }
    if (q->cond != nullptr) {
        SDL_DestroyCond(q->cond);
        q->cond = nullptr;
    }
}

static void M_PacketQueueAbort(M_PACKET_QUEUE *q)
{
    SDL_LockMutex(q->mutex);
    q->abort_request = true;
    SDL_CondSignal(q->cond);
    SDL_UnlockMutex(q->mutex);
}

static void M_PacketQueueStart(M_PACKET_QUEUE *q)
{
    SDL_LockMutex(q->mutex);
    q->abort_request = false;
    q->serial++;
    SDL_UnlockMutex(q->mutex);
}

static int M_PacketQueueGet(
    M_PACKET_QUEUE *q, AVPacket *pkt, int block, int *serial)
{
    M_PACKET_LIST pkt1;
    int ret;

    SDL_LockMutex(q->mutex);

    while (1) {
        if (q->abort_request) {
            ret = -1;
            break;
        }

        if (av_fifo_read(q->pkt_list, &pkt1, 1) >= 0) {
            q->nb_packets--;
            q->size -= pkt1.pkt->size + sizeof(pkt1);
            q->duration -= pkt1.pkt->duration;
            av_packet_move_ref(pkt, pkt1.pkt);
            if (serial) {
                *serial = pkt1.serial;
            }
            av_packet_free(&pkt1.pkt);
            ret = 1;
            break;
        } else if (!block) {
            ret = 0;
            break;
        } else {
            SDL_CondWait(q->cond, q->mutex);
        }
    }
    SDL_UnlockMutex(q->mutex);
    return ret;
}

static int M_DecoderInit(
    M_DECODER *d, AVCodecContext *avctx, M_PACKET_QUEUE *queue,
    SDL_cond *empty_queue_cond)
{
    memset(d, 0, sizeof(M_DECODER));
    d->pkt = av_packet_alloc();
    if (!d->pkt) {
        return AVERROR(ENOMEM);
    }
    d->avctx = avctx;
    d->queue = queue;
    d->empty_queue_cond = empty_queue_cond;
    d->start_pts = AV_NOPTS_VALUE;
    d->pkt_serial = -1;
    return 0;
}

static int M_DecoderDecodeFrame(M_DECODER *d, AVFrame *frame)
{
    int ret = AVERROR(EAGAIN);

    while (1) {
        if (d->queue->serial == d->pkt_serial) {
            do {
                if (d->queue->abort_request) {
                    return -1;
                }

                switch (d->avctx->codec_type) {
                case AVMEDIA_TYPE_VIDEO:
                    ret = avcodec_receive_frame(d->avctx, frame);
                    if (ret >= 0) {
                        frame->pts = frame->best_effort_timestamp;
                    }
                    break;

                case AVMEDIA_TYPE_AUDIO:
                    ret = avcodec_receive_frame(d->avctx, frame);
                    if (ret >= 0) {
                        AVRational tb = (AVRational) { 1, frame->sample_rate };
                        if (frame->pts != AV_NOPTS_VALUE) {
                            frame->pts = av_rescale_q(
                                frame->pts, d->avctx->pkt_timebase, tb);
                        } else if (d->next_pts != AV_NOPTS_VALUE) {
                            frame->pts =
                                av_rescale_q(d->next_pts, d->next_pts_tb, tb);
                        }
                        if (frame->pts != AV_NOPTS_VALUE) {
                            d->next_pts = frame->pts + frame->nb_samples;
                            d->next_pts_tb = tb;
                        }
                    }
                    break;

                default:
                    break;
                }

                if (ret == AVERROR_EOF) {
                    d->finished = d->pkt_serial;
                    avcodec_flush_buffers(d->avctx);
                    return 0;
                }
                if (ret >= 0) {
                    return 1;
                }
            } while (ret != AVERROR(EAGAIN));
        }

        while (1) {
            if (d->queue->nb_packets == 0) {
                SDL_CondSignal(d->empty_queue_cond);
            }
            if (d->packet_pending) {
                d->packet_pending = false;
            } else {
                int old_serial = d->pkt_serial;
                if (M_PacketQueueGet(d->queue, d->pkt, 1, &d->pkt_serial) < 0) {
                    return -1;
                }
                if (old_serial != d->pkt_serial) {
                    avcodec_flush_buffers(d->avctx);
                    d->finished = 0;
                    d->next_pts = d->start_pts;
                    d->next_pts_tb = d->start_pts_tb;
                }
            }
            if (d->queue->serial == d->pkt_serial) {
                break;
            }
            av_packet_unref(d->pkt);
        }

        if (d->pkt->buf && !d->pkt->opaque_ref) {
            d->pkt->opaque_ref = av_buffer_allocz(sizeof(M_FRAME_DATA));
            if (!d->pkt->opaque_ref) {
                return AVERROR(ENOMEM);
            }
            M_FRAME_DATA *fd = (M_FRAME_DATA *)d->pkt->opaque_ref->data;
            fd->pkt_pos = d->pkt->pos;
        }

        if (avcodec_send_packet(d->avctx, d->pkt) == AVERROR(EAGAIN)) {
            LOG_ERROR(
                "Receive_frame and send_packet both returned EAGAIN, "
                "which is an API violation.");
            d->packet_pending = true;
        } else {
            av_packet_unref(d->pkt);
        }
    }
}

static void M_DecoderShutdown(M_DECODER *d)
{
    av_packet_free(&d->pkt);
    avcodec_free_context(&d->avctx);
}

static void M_FrameQueueUnrefItem(M_FRAME *vp)
{
    av_frame_unref(vp->frame);
}

static int M_FrameQueueInit(
    M_FRAME_QUEUE *f, M_PACKET_QUEUE *pktq, int max_size, int keep_last)
{
    memset(f, 0, sizeof(M_FRAME_QUEUE));
    if (!(f->mutex = SDL_CreateMutex())) {
        LOG_ERROR("SDL_CreateMutex(): %s", SDL_GetError());
        return AVERROR(ENOMEM);
    }
    if (!(f->cond = SDL_CreateCond())) {
        LOG_ERROR("SDL_CreateCond(): %s", SDL_GetError());
        return AVERROR(ENOMEM);
    }
    f->pktq = pktq;
    f->max_size = FFMIN(max_size, FRAME_QUEUE_SIZE);
    f->keep_last = !!keep_last;
    for (int i = 0; i < f->max_size; i++) {
        if (!(f->queue[i].frame = av_frame_alloc())) {
            return AVERROR(ENOMEM);
        }
    }
    return 0;
}

static void M_FrameQueueShutdown(M_FRAME_QUEUE *f)
{
    for (int i = 0; i < f->max_size; i++) {
        M_FRAME *vp = &f->queue[i];
        if (vp->frame != nullptr) {
            M_FrameQueueUnrefItem(vp);
            av_frame_free(&vp->frame);
        }
    }
    if (f->mutex != nullptr) {
        SDL_DestroyMutex(f->mutex);
        f->mutex = nullptr;
    }
    if (f->cond != nullptr) {
        SDL_DestroyCond(f->cond);
        f->cond = nullptr;
    }
}

static void M_FrameQueueSignal(M_FRAME_QUEUE *f)
{
    SDL_LockMutex(f->mutex);
    SDL_CondSignal(f->cond);
    SDL_UnlockMutex(f->mutex);
}

static M_FRAME *M_FrameQueuePeek(M_FRAME_QUEUE *f)
{
    return &f->queue[(f->rindex + f->rindex_shown) % f->max_size];
}

static M_FRAME *M_FrameQueuePeekNext(M_FRAME_QUEUE *f)
{
    return &f->queue[(f->rindex + f->rindex_shown + 1) % f->max_size];
}

static M_FRAME *M_FrameQueuePeekLast(M_FRAME_QUEUE *f)
{
    return &f->queue[f->rindex];
}

static M_FRAME *M_FrameQueuePeekWritable(M_FRAME_QUEUE *f)
{
    SDL_LockMutex(f->mutex);
    while (f->size >= f->max_size && !f->pktq->abort_request) {
        SDL_CondWait(f->cond, f->mutex);
    }
    SDL_UnlockMutex(f->mutex);

    if (f->pktq->abort_request) {
        return nullptr;
    }

    return &f->queue[f->windex];
}

static M_FRAME *M_FrameQueuePeekReadable(M_FRAME_QUEUE *f)
{
    SDL_LockMutex(f->mutex);
    while (f->size - f->rindex_shown <= 0 && !f->pktq->abort_request) {
        SDL_CondWait(f->cond, f->mutex);
    }
    SDL_UnlockMutex(f->mutex);

    if (f->pktq->abort_request) {
        return nullptr;
    }

    return &f->queue[(f->rindex + f->rindex_shown) % f->max_size];
}

static void M_FrameQueuePush(M_FRAME_QUEUE *f)
{
    if (++f->windex == f->max_size) {
        f->windex = 0;
    }
    SDL_LockMutex(f->mutex);
    f->size++;
    SDL_CondSignal(f->cond);
    SDL_UnlockMutex(f->mutex);
}

static void M_FrameQueueNext(M_FRAME_QUEUE *f)
{
    if (f->keep_last && !f->rindex_shown) {
        f->rindex_shown = 1;
        return;
    }
    M_FrameQueueUnrefItem(&f->queue[f->rindex]);
    if (++f->rindex == f->max_size) {
        f->rindex = 0;
    }
    SDL_LockMutex(f->mutex);
    f->size--;
    SDL_CondSignal(f->cond);
    SDL_UnlockMutex(f->mutex);
}

static int M_FrameQueueNBRemaining(M_FRAME_QUEUE *f)
{
    return f->size - f->rindex_shown;
}

static void M_DecoderAbort(M_DECODER *d, M_FRAME_QUEUE *fq)
{
    M_PacketQueueAbort(d->queue);
    M_FrameQueueSignal(fq);
    SDL_WaitThread(d->decoder_tid, nullptr);
    d->decoder_tid = nullptr;
    M_PacketQueueFlush(d->queue);
}

static void M_ReallocPrimarySurface(
    M_STATE *is, int surface_width, int surface_height, bool clear)
{
    is->surface_width = surface_width;
    is->surface_height = surface_height;

    if (is->primary_surface != nullptr) {
        is->surface_deallocator_func(
            is->primary_surface, is->surface_deallocator_func_user_data);
        is->primary_surface = nullptr;
    }

    {
        is->primary_surface = is->surface_allocator_func(
            is->surface_width, is->surface_height,
            is->surface_allocator_func_user_data);
    }

    if (clear) {
        is->surface_clear_func(
            is->primary_surface, is->surface_clear_func_user_data);
    }
}

static void M_RecalcSurfaceTargetRect(
    M_STATE *is, int32_t frame_width, int32_t frame_height)
{
    const float source_ratio = frame_width / (float)frame_height;
    const float target_ratio = is->surface_width / (float)is->surface_height;

    is->target_surface_width = source_ratio < target_ratio
        ? is->surface_height * source_ratio
        : is->surface_width;
    is->target_surface_height = source_ratio < target_ratio
        ? is->surface_height
        : is->surface_width / source_ratio;
    is->target_surface_x = (is->surface_width - is->target_surface_width) / 2;
    is->target_surface_y = (is->surface_height - is->target_surface_height) / 2;
}

static int M_UploadTexture(M_STATE *is, AVFrame *frame)
{
    int ret = 0;

    is->img_convert_ctx = sws_getCachedContext(
        is->img_convert_ctx, frame->width, frame->height, frame->format,
        is->target_surface_width, is->target_surface_height,
        is->primary_surface_pixel_format, SWS_BILINEAR, nullptr, nullptr,
        nullptr);

    if (is->img_convert_ctx) {
        is->render_begin_func(
            is->primary_surface, is->render_begin_func_user_data);

        void *pixels = is->surface_lock_func(
            is->primary_surface, is->surface_lock_func_user_data);

        if (pixels != nullptr) {
            uint8_t *surf_planes[4] = { pixels, nullptr, nullptr, nullptr };
            int surf_linesize[4] = {};
            if (is->primary_surface_stride > 0) {
                surf_linesize[0] = is->primary_surface_stride;
            } else {
                surf_linesize[0] = av_image_get_linesize(
                    is->primary_surface_pixel_format, is->surface_width, 0);
            }

            surf_planes[0] += is->target_surface_y * surf_linesize[0];
            surf_planes[0] += av_image_get_linesize(
                is->primary_surface_pixel_format, is->target_surface_x, 0);

            sws_scale(
                is->img_convert_ctx, (const uint8_t *const *)frame->data,
                frame->linesize, 0, frame->height, surf_planes, surf_linesize);

            is->surface_unlock_func(
                is->primary_surface, is->surface_unlock_func_user_data);
            is->surface_upload_func(
                is->primary_surface, is->surface_upload_func_user_data);
        }

        is->render_end_func(is->primary_surface, is->render_end_func_user_data);
    } else {
        LOG_ERROR("Cannot initialize the conversion context");
        ret = -1;
    }

    return ret;
}

static void M_VideoImageDisplay(M_STATE *is)
{
    M_FRAME *vp = M_FrameQueuePeekLast(&is->pictq);

    M_RecalcSurfaceTargetRect(is, vp->frame->width, vp->frame->height);
    M_UploadTexture(is, vp->frame);
}

static void M_StreamComponentClose(M_STATE *is, int stream_index)
{
    AVFormatContext *ic = is->ic;
    AVCodecParameters *codecpar;

    if (stream_index < 0 || stream_index >= (signed)ic->nb_streams) {
        return;
    }
    codecpar = ic->streams[stream_index]->codecpar;

    switch (codecpar->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
        M_DecoderAbort(&is->auddec, &is->sampq);
        M_DecoderShutdown(&is->auddec);
        swr_free(&is->swr_ctx);
        av_freep(&is->audio_buf1);
        if (m_AudioDevice > 0) {
            SDL_CloseAudioDevice(m_AudioDevice);
            m_AudioDevice = 0;
        }
        is->audio_buf1_size = 0;
        is->audio_buf = nullptr;

        break;
    case AVMEDIA_TYPE_VIDEO:
        M_DecoderAbort(&is->viddec, &is->pictq);
        M_DecoderShutdown(&is->viddec);
        break;
    default:
        break;
    }

    ic->streams[stream_index]->discard = AVDISCARD_ALL;
    switch (codecpar->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
        is->audio_st = nullptr;
        is->audio_stream = -1;
        break;
    case AVMEDIA_TYPE_VIDEO:
        is->video_st = nullptr;
        is->video_stream = -1;
        break;
    default:
        break;
    }
}

static void M_StreamClose(M_STATE *is)
{
    if (is == nullptr) {
        return;
    }

    if (is->read_tid != nullptr) {
        SDL_WaitThread(is->read_tid, nullptr);
        is->read_tid = nullptr;
    }

    if (is->ic != nullptr && is->audio_stream >= 0) {
        M_StreamComponentClose(is, is->audio_stream);
    }
    if (is->ic != nullptr && is->video_stream >= 0) {
        M_StreamComponentClose(is, is->video_stream);
    }

    if (is->ic != nullptr) {
        avformat_close_input(&is->ic);
    }

    M_PacketQueueDestroy(&is->videoq);
    M_PacketQueueDestroy(&is->audioq);

    if (is->pictq.mutex != nullptr || is->pictq.cond != nullptr) {
        M_FrameQueueShutdown(&is->pictq);
    }
    if (is->sampq.mutex != nullptr || is->sampq.cond != nullptr) {
        M_FrameQueueShutdown(&is->sampq);
    }
    if (is->continue_read_thread != nullptr) {
        SDL_DestroyCond(is->continue_read_thread);
        is->continue_read_thread = nullptr;
    }
    sws_freeContext(is->img_convert_ctx);
    av_free(is->filename);
    if (is->primary_surface) {
        is->surface_deallocator_func(
            is->primary_surface, is->surface_deallocator_func_user_data);
    }
    av_free(is);
}

static void M_VideoDisplay(M_STATE *is)
{
    if (is->video_st) {
        M_VideoImageDisplay(is);
    }
}

static double M_GetClock(M_CLOCK *c)
{
    if (*c->queue_serial != c->serial) {
        return NAN;
    }
    if (c->paused) {
        return c->pts;
    } else {
        double time = av_gettime_relative() / 1000000.0;
        return c->pts_drift + time
            - (time - c->last_updated) * (1.0 - c->speed);
    }
}

static void M_SetClockAt(M_CLOCK *c, double pts, int serial, double time)
{
    c->pts = pts;
    c->last_updated = time;
    c->pts_drift = c->pts - time;
    c->serial = serial;
}

static void M_SetClock(M_CLOCK *c, double pts, int serial)
{
    double time = av_gettime_relative() / 1000000.0;
    M_SetClockAt(c, pts, serial, time);
}

static void M_InitClock(M_CLOCK *c, int *queue_serial)
{
    c->speed = 1.0;
    c->paused = false;
    c->queue_serial = queue_serial;
    M_SetClock(c, NAN, -1);
}

static void M_SyncClockToSlave(M_CLOCK *const c, M_CLOCK *const slave)
{
    double clock = M_GetClock(c);
    double slave_clock = M_GetClock(slave);
    if (!isnan(slave_clock)
        && (isnan(clock) || fabs(clock - slave_clock) > AV_NOSYNC_THRESHOLD)) {
        M_SetClock(c, slave_clock, slave->serial);
    }
}

static int M_GetMasterSyncType(M_STATE *is)
{
    if (is->av_sync_type == AV_SYNC_VIDEO_MASTER) {
        if (is->video_st) {
            return AV_SYNC_VIDEO_MASTER;
        } else {
            return AV_SYNC_AUDIO_MASTER;
        }
    } else if (is->av_sync_type == AV_SYNC_AUDIO_MASTER) {
        if (is->audio_st) {
            return AV_SYNC_AUDIO_MASTER;
        } else {
            return AV_SYNC_EXTERNAL_CLOCK;
        }
    } else {
        return AV_SYNC_EXTERNAL_CLOCK;
    }
}

static double M_GetMasterClock(M_STATE *is)
{
    switch (M_GetMasterSyncType(is)) {
    case AV_SYNC_VIDEO_MASTER:
        return M_GetClock(&is->vidclk);

    case AV_SYNC_AUDIO_MASTER:
        return M_GetClock(&is->audclk);

    default:
        return M_GetClock(&is->extclk);
    }
}

static double M_ComputeTargetDelay(double delay, M_STATE *is)
{
    double sync_threshold, diff = 0;

    if (M_GetMasterSyncType(is) != AV_SYNC_VIDEO_MASTER) {
        diff = M_GetClock(&is->vidclk) - M_GetMasterClock(is);

        sync_threshold =
            FFMAX(AV_SYNC_THRESHOLD_MIN, FFMIN(AV_SYNC_THRESHOLD_MAX, delay));
        if (!isnan(diff) && fabs(diff) < is->max_frame_duration) {
            if (diff <= -sync_threshold) {
                delay = FFMAX(0, delay + diff);
            } else if (
                diff >= sync_threshold && delay > AV_SYNC_FRAMEDUP_THRESHOLD) {
                delay = delay + diff;
            } else if (diff >= sync_threshold) {
                delay = 2 * delay;
            }
        }
    }

    return delay;
}

static double M_VPDuration(M_STATE *is, M_FRAME *vp, M_FRAME *nextvp)
{
    if (vp->serial == nextvp->serial) {
        double duration = nextvp->pts - vp->pts;
        if (isnan(duration) || duration <= 0
            || duration > is->max_frame_duration) {
            return vp->duration;
        } else {
            return duration;
        }
    } else {
        return 0.0;
    }
}

static void M_UpdateVideoPTS(M_STATE *is, double pts, int64_t pos, int serial)
{
    M_SetClock(&is->vidclk, pts, serial);
    M_SyncClockToSlave(&is->extclk, &is->vidclk);
}

static void M_VideoRefresh(void *opaque, double *remaining_time)
{
    M_STATE *is = opaque;
    double time;

    if (is->video_st) {
    retry:
        if (M_FrameQueueNBRemaining(&is->pictq) != 0) {
            double last_duration, duration, delay;
            M_FRAME *vp, *lastvp;

            lastvp = M_FrameQueuePeekLast(&is->pictq);
            vp = M_FrameQueuePeek(&is->pictq);

            if (vp->serial != is->videoq.serial) {
                M_FrameQueueNext(&is->pictq);
                goto retry;
            }

            if (lastvp->serial != vp->serial) {
                is->frame_timer = av_gettime_relative() / 1000000.0;
            }

            if (is->paused) {
                goto display;
            }

            last_duration = M_VPDuration(is, lastvp, vp);
            delay = M_ComputeTargetDelay(last_duration, is);

            time = av_gettime_relative() / 1000000.0;
            if (time < is->frame_timer + delay) {
                *remaining_time =
                    FFMIN(is->frame_timer + delay - time, *remaining_time);
                goto display;
            }

            is->frame_timer += delay;
            if (delay > 0 && time - is->frame_timer > AV_SYNC_THRESHOLD_MAX) {
                is->frame_timer = time;
            }

            SDL_LockMutex(is->pictq.mutex);
            if (!isnan(vp->pts)) {
                M_UpdateVideoPTS(is, vp->pts, vp->pos, vp->serial);
            }
            SDL_UnlockMutex(is->pictq.mutex);

            if (M_FrameQueueNBRemaining(&is->pictq) > 1) {
                M_FRAME *nextvp = M_FrameQueuePeekNext(&is->pictq);
                duration = M_VPDuration(is, vp, nextvp);
                if (M_GetMasterSyncType(is) != AV_SYNC_VIDEO_MASTER
                    && time > is->frame_timer + duration) {
                    is->frame_drops_late++;
                    M_FrameQueueNext(&is->pictq);
                    goto retry;
                }
            }

            M_FrameQueueNext(&is->pictq);
            is->force_refresh = true;
        }

    display:
        if (is->force_refresh && is->pictq.rindex_shown) {
            M_VideoDisplay(is);
        }
    }
    is->force_refresh = false;
}

static int M_QueuePicture(
    M_STATE *is, AVFrame *src_frame, double pts, double duration, int64_t pos,
    int serial)
{
    M_FRAME *vp;

    if (!(vp = M_FrameQueuePeekWritable(&is->pictq))) {
        return -1;
    }

    vp->sar = src_frame->sample_aspect_ratio;

    vp->width = src_frame->width;
    vp->height = src_frame->height;
    vp->format = src_frame->format;

    vp->pts = pts;
    vp->duration = duration;
    vp->pos = pos;
    vp->serial = serial;

    av_frame_move_ref(vp->frame, src_frame);
    M_FrameQueuePush(&is->pictq);
    return 0;
}

static int M_GetVideoFrame(M_STATE *is, AVFrame *frame)
{
    int got_picture;

    if ((got_picture = M_DecoderDecodeFrame(&is->viddec, frame)) < 0) {
        return -1;
    }

    if (got_picture) {
        double dpts = NAN;

        if (frame->pts != AV_NOPTS_VALUE) {
            dpts = av_q2d(is->video_st->time_base) * frame->pts;
        }

        frame->sample_aspect_ratio =
            av_guess_sample_aspect_ratio(is->ic, is->video_st, frame);

        if (M_GetMasterSyncType(is) != AV_SYNC_VIDEO_MASTER) {
            if (frame->pts != AV_NOPTS_VALUE) {
                double diff = dpts - M_GetMasterClock(is);
                if (!isnan(diff) && fabs(diff) < AV_NOSYNC_THRESHOLD && diff < 0
                    && is->viddec.pkt_serial == is->vidclk.serial
                    && is->videoq.nb_packets) {
                    is->frame_drops_early++;
                    av_frame_unref(frame);
                    got_picture = 0;
                }
            }
        }
    }

    return got_picture;
}

static int M_AudioThread(void *arg)
{
    M_STATE *is = arg;
    AVFrame *frame = av_frame_alloc();
    M_FRAME *af;
    int last_serial = -1;
    int got_frame = 0;
    AVRational tb;
    int ret = 0;

    if (frame == nullptr) {
        return AVERROR(ENOMEM);
    }

    do {
        got_frame = M_DecoderDecodeFrame(&is->auddec, frame);
        if (got_frame < 0) {
            goto the_end;
        }

        if (got_frame) {
            tb = (AVRational) { 1, frame->sample_rate };

            M_FRAME_DATA *fd = frame->opaque_ref
                ? (M_FRAME_DATA *)frame->opaque_ref->data
                : nullptr;
            af = M_FrameQueuePeekWritable(&is->sampq);
            if (af == nullptr) {
                goto the_end;
            }

            af->pts =
                (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb);
            af->pos = fd ? fd->pkt_pos : -1;
            af->serial = is->auddec.pkt_serial;
            af->duration =
                av_q2d((AVRational) { frame->nb_samples, frame->sample_rate });

            av_frame_move_ref(af->frame, frame);
            M_FrameQueuePush(&is->sampq);

            if (is->audioq.serial != is->auddec.pkt_serial) {
                break;
            }

            if (ret == AVERROR_EOF) {
                is->auddec.finished = is->auddec.pkt_serial;
            }
        }
    } while (ret >= 0 || ret == AVERROR(EAGAIN) || ret == AVERROR_EOF);

the_end:
    av_frame_free(&frame);
    return ret;
}

static int M_DecoderStart(
    M_DECODER *d, int (*fn)(void *), const char *thread_name, void *arg)
{
    M_PacketQueueStart(d->queue);
    d->decoder_tid = SDL_CreateThread(fn, thread_name, arg);
    if (!d->decoder_tid) {
        LOG_ERROR("SDL_CreateThread(): %s", SDL_GetError());
        return AVERROR(ENOMEM);
    }
    return 0;
}

static int M_VideoThread(void *arg)
{
    M_STATE *is = arg;
    AVFrame *frame = av_frame_alloc();
    double pts;
    double duration;
    int ret;
    AVRational tb = is->video_st->time_base;
    AVRational frame_rate = av_guess_frame_rate(is->ic, is->video_st, nullptr);

    if (!frame) {
        return AVERROR(ENOMEM);
    }

    while (1) {
        ret = M_GetVideoFrame(is, frame);
        if (ret < 0) {
            goto the_end;
        }
        if (!ret) {
            continue;
        }

        is->frame_last_returned_time = av_gettime_relative() / 1000000.0;

        M_FRAME_DATA *fd = frame->opaque_ref
            ? (M_FRAME_DATA *)frame->opaque_ref->data
            : nullptr;

        duration =
            (frame_rate.num && frame_rate.den
                 ? av_q2d((AVRational) { frame_rate.den, frame_rate.num })
                 : 0);
        pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb);
        ret = M_QueuePicture(
            is, frame, pts, duration, fd ? fd->pkt_pos : -1,
            is->viddec.pkt_serial);
        av_frame_unref(frame);
        if (is->videoq.serial != is->viddec.pkt_serial) {
            break;
        }
    }
the_end:
    av_frame_free(&frame);
    return 0;
}

static int M_SynchronizeAudio(M_STATE *is, int nb_samples)
{
    int wanted_nb_samples = nb_samples;

    if (M_GetMasterSyncType(is) != AV_SYNC_AUDIO_MASTER) {
        double diff, avg_diff;
        int min_nb_samples, max_nb_samples;

        diff = M_GetClock(&is->audclk) - M_GetMasterClock(is);

        if (!isnan(diff) && fabs(diff) < AV_NOSYNC_THRESHOLD) {
            is->audio_diff_cum =
                diff + is->audio_diff_avg_coef * is->audio_diff_cum;
            if (is->audio_diff_avg_count < AUDIO_DIFF_AVG_NB) {
                is->audio_diff_avg_count++;
            } else {
                avg_diff = is->audio_diff_cum * (1.0 - is->audio_diff_avg_coef);

                if (fabs(avg_diff) >= is->audio_diff_threshold) {
                    wanted_nb_samples =
                        nb_samples + (int)(diff * is->audio_src.freq);
                    min_nb_samples =
                        ((nb_samples * (100 - SAMPLE_CORRECTION_PERCENT_MAX)
                          / 100));
                    max_nb_samples =
                        ((nb_samples * (100 + SAMPLE_CORRECTION_PERCENT_MAX)
                          / 100));
                    wanted_nb_samples = av_clip(
                        wanted_nb_samples, min_nb_samples, max_nb_samples);
                }
            }
        } else {
            is->audio_diff_avg_count = 0;
            is->audio_diff_cum = 0;
        }
    }

    return wanted_nb_samples;
}

static int M_AudioDecodeFrame(M_STATE *is)
{

    int data_size, resampled_data_size;
    av_unused double audio_clock0;
    int wanted_nb_samples;
    M_FRAME *af;

    if (is->paused) {
        return -1;
    }

    do {
#if defined(_WIN32)
        while (M_FrameQueueNBRemaining(&is->sampq) == 0) {
            if ((av_gettime_relative() - m_AudioCallbackTime) > 1000000LL
                    * is->audio_hw_buf_size / is->audio_tgt.bytes_per_sec / 2) {
                return -1;
            }
            av_usleep(1000);
        }
#endif
        if (!(af = M_FrameQueuePeekReadable(&is->sampq))) {
            return -1;
        }
        M_FrameQueueNext(&is->sampq);
    } while (af->serial != is->audioq.serial);

    data_size = av_samples_get_buffer_size(
        nullptr, af->frame->ch_layout.nb_channels, af->frame->nb_samples,
        af->frame->format, 1);

    wanted_nb_samples = M_SynchronizeAudio(is, af->frame->nb_samples);

    if (af->frame->format != is->audio_src.fmt
        || av_channel_layout_compare(
            &af->frame->ch_layout, &is->audio_src.ch_layout)
        || af->frame->sample_rate != is->audio_src.freq
        || (wanted_nb_samples != af->frame->nb_samples && !is->swr_ctx)) {
        int ret;
        swr_free(&is->swr_ctx);
        ret = swr_alloc_set_opts2(
            &is->swr_ctx, &is->audio_tgt.ch_layout, is->audio_tgt.fmt,
            is->audio_tgt.freq, &af->frame->ch_layout, af->frame->format,
            af->frame->sample_rate, 0, nullptr);
        if (ret < 0 || swr_init(is->swr_ctx) < 0) {
            LOG_ERROR(
                "Cannot create sample rate converter for conversion of %d Hz "
                "%s %d channels to %d Hz %s %d channels!",
                af->frame->sample_rate,
                av_get_sample_fmt_name(af->frame->format),
                af->frame->ch_layout.nb_channels, is->audio_tgt.freq,
                av_get_sample_fmt_name(is->audio_tgt.fmt),
                is->audio_tgt.ch_layout.nb_channels);
            swr_free(&is->swr_ctx);
            return -1;
        }
        if (av_channel_layout_copy(
                &is->audio_src.ch_layout, &af->frame->ch_layout)
            < 0) {
            return -1;
        }
        is->audio_src.freq = af->frame->sample_rate;
        is->audio_src.fmt = af->frame->format;
    }

    if (is->swr_ctx) {
        const uint8_t **in = (const uint8_t **)af->frame->extended_data;
        uint8_t **out = &is->audio_buf1;
        int out_count = (int64_t)wanted_nb_samples * is->audio_tgt.freq
                / af->frame->sample_rate
            + 256;
        int out_size = av_samples_get_buffer_size(
            nullptr, is->audio_tgt.ch_layout.nb_channels, out_count,
            is->audio_tgt.fmt, 0);
        int len2;
        if (out_size < 0) {
            LOG_ERROR("av_samples_get_buffer_size() failed");
            return -1;
        }
        if (wanted_nb_samples != af->frame->nb_samples) {
            if (swr_set_compensation(
                    is->swr_ctx,
                    (wanted_nb_samples - af->frame->nb_samples)
                        * is->audio_tgt.freq / af->frame->sample_rate,
                    wanted_nb_samples * is->audio_tgt.freq
                        / af->frame->sample_rate)
                < 0) {
                LOG_ERROR("swr_set_compensation() failed");
                return -1;
            }
        }
        av_fast_malloc(&is->audio_buf1, &is->audio_buf1_size, out_size);
        if (!is->audio_buf1) {
            return AVERROR(ENOMEM);
        }
        len2 =
            swr_convert(is->swr_ctx, out, out_count, in, af->frame->nb_samples);
        if (len2 < 0) {
            LOG_ERROR("swr_convert() failed");
            return -1;
        }
        if (len2 == out_count) {
            LOG_ERROR("audio buffer is probably too small");
            if (swr_init(is->swr_ctx) < 0) {
                swr_free(&is->swr_ctx);
            }
        }
        is->audio_buf = is->audio_buf1;
        resampled_data_size = len2 * is->audio_tgt.ch_layout.nb_channels
            * av_get_bytes_per_sample(is->audio_tgt.fmt);
    } else {
        is->audio_buf = af->frame->data[0];
        resampled_data_size = data_size;
    }

    audio_clock0 = is->audio_clock;
    if (!isnan(af->pts)) {
        is->audio_clock =
            af->pts + (double)af->frame->nb_samples / af->frame->sample_rate;
    } else {
        is->audio_clock = NAN;
    }
    is->audio_clock_serial = af->serial;
    return resampled_data_size;
}

static void M_SDLAudioCallback(void *opaque, Uint8 *stream, int len)
{
    M_STATE *is = opaque;
    int audio_size, len1;

    m_AudioCallbackTime = av_gettime_relative();

    while (len > 0) {
        if (is->audio_buf_index >= (signed)is->audio_buf_size) {
            audio_size = M_AudioDecodeFrame(is);
            if (audio_size < 0) {
                is->audio_buf = nullptr;
                is->audio_buf_size = SDL_AUDIO_MIN_BUFFER_SIZE
                    / is->audio_tgt.frame_size * is->audio_tgt.frame_size;
            } else {
                is->audio_buf_size = audio_size;
            }
            is->audio_buf_index = 0;
        }
        len1 = is->audio_buf_size - is->audio_buf_index;
        if (len1 > len) {
            len1 = len;
        }
        if (is->audio_buf && is->audio_volume == SDL_MIX_MAXVOLUME) {
            memcpy(
                stream, (uint8_t *)is->audio_buf + is->audio_buf_index, len1);
        } else {
            memset(stream, 0, len1);
            if (is->audio_buf) {
                SDL_MixAudioFormat(
                    stream, (uint8_t *)is->audio_buf + is->audio_buf_index,
                    AUDIO_S16SYS, len1, is->audio_volume);
            }
        }
        len -= len1;
        stream += len1;
        is->audio_buf_index += len1;
    }
    is->audio_write_buf_size = is->audio_buf_size - is->audio_buf_index;
    if (!isnan(is->audio_clock)) {
        M_SetClockAt(
            &is->audclk,
            is->audio_clock
                - (double)(2 * is->audio_hw_buf_size + is->audio_write_buf_size)
                    / is->audio_tgt.bytes_per_sec,
            is->audio_clock_serial, m_AudioCallbackTime / 1000000.0);
        M_SyncClockToSlave(&is->extclk, &is->audclk);
    }
}

static int M_AudioOpen(
    M_STATE *is, AVChannelLayout *wanted_channel_layout, int wanted_sample_rate,
    M_AUDIO_PARAMS *audio_hw_params)
{
    SDL_AudioSpec wanted_spec, spec;
    const char *env;
    static const int next_nb_channels[] = { 0, 0, 1, 6, 2, 6, 4, 6 };
    static const int next_sample_rates[] = { 0, 44100, 48000, 96000, 192000 };
    int next_sample_rate_idx = FF_ARRAY_ELEMS(next_sample_rates) - 1;
    int wanted_nb_channels = wanted_channel_layout->nb_channels;

    env = SDL_getenv("SDL_AUDIO_CHANNELS");
    if (env) {
        wanted_nb_channels = atoi(env);
        av_channel_layout_uninit(wanted_channel_layout);
        av_channel_layout_default(wanted_channel_layout, wanted_nb_channels);
    }
    if (wanted_channel_layout->order != AV_CHANNEL_ORDER_NATIVE) {
        av_channel_layout_uninit(wanted_channel_layout);
        av_channel_layout_default(wanted_channel_layout, wanted_nb_channels);
    }
    wanted_nb_channels = wanted_channel_layout->nb_channels;
    wanted_spec.channels = wanted_nb_channels;
    wanted_spec.freq = wanted_sample_rate;
    if (wanted_spec.freq <= 0 || wanted_spec.channels <= 0) {
        LOG_ERROR("Invalid sample rate or channel count!");
        return -1;
    }
    while (next_sample_rate_idx
           && next_sample_rates[next_sample_rate_idx] >= wanted_spec.freq) {
        next_sample_rate_idx--;
    }
    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.silence = 0;
    wanted_spec.samples = FFMAX(
        SDL_AUDIO_MIN_BUFFER_SIZE,
        2 << av_log2(wanted_spec.freq / SDL_AUDIO_MAX_CALLBACKS_PER_SEC));
    wanted_spec.callback = M_SDLAudioCallback;
    wanted_spec.userdata = is;
    while (
        !(m_AudioDevice = SDL_OpenAudioDevice(
              nullptr, 0, &wanted_spec, &spec,
              SDL_AUDIO_ALLOW_FREQUENCY_CHANGE
                  | SDL_AUDIO_ALLOW_CHANNELS_CHANGE))) {
        LOG_WARNING(
            "SDL_OpenAudio (%d channels, %d Hz): %s", wanted_spec.channels,
            wanted_spec.freq, SDL_GetError());
        wanted_spec.channels = next_nb_channels[FFMIN(7, wanted_spec.channels)];
        if (!wanted_spec.channels) {
            wanted_spec.freq = next_sample_rates[next_sample_rate_idx--];
            wanted_spec.channels = wanted_nb_channels;
            if (!wanted_spec.freq) {
                LOG_ERROR("No more combinations to try, audio open failed");
                return -1;
            }
        }
        av_channel_layout_default(wanted_channel_layout, wanted_spec.channels);
    }
    if (spec.format != AUDIO_S16SYS) {
        LOG_ERROR("SDL advised audio format %d is not supported!", spec.format);
        return -1;
    }
    if (spec.channels != wanted_spec.channels) {
        av_channel_layout_uninit(wanted_channel_layout);
        av_channel_layout_default(wanted_channel_layout, spec.channels);
        if (wanted_channel_layout->order != AV_CHANNEL_ORDER_NATIVE) {
            LOG_ERROR(
                "SDL advised channel count %d is not supported!",
                spec.channels);
            return -1;
        }
    }

    audio_hw_params->fmt = AV_SAMPLE_FMT_S16;
    audio_hw_params->freq = spec.freq;
    if (av_channel_layout_copy(
            &audio_hw_params->ch_layout, wanted_channel_layout)
        < 0) {
        return -1;
    }
    audio_hw_params->frame_size = av_samples_get_buffer_size(
        nullptr, audio_hw_params->ch_layout.nb_channels, 1,
        audio_hw_params->fmt, 1);
    audio_hw_params->bytes_per_sec = av_samples_get_buffer_size(
        nullptr, audio_hw_params->ch_layout.nb_channels, audio_hw_params->freq,
        audio_hw_params->fmt, 1);
    if (audio_hw_params->bytes_per_sec <= 0
        || audio_hw_params->frame_size <= 0) {
        LOG_ERROR("av_samples_get_buffer_size failed");
        return -1;
    }
    return spec.size;
}

static int M_StreamComponentOpen(M_STATE *is, int stream_index)
{
    AVFormatContext *ic = is->ic;
    AVCodecContext *avctx = nullptr;
    const AVCodec *codec = nullptr;
    const char *forced_codec_name = nullptr;
    AVDictionary *opts = nullptr;
    const AVDictionaryEntry *t = nullptr;
    int sample_rate;
    int nb_channels;
    AVChannelLayout ch_layout = {};
    bool has_ch_layout = false;
    int ret = 0;

    if (stream_index < 0 || stream_index >= (signed)ic->nb_streams) {
        return -1;
    }

    avctx = avcodec_alloc_context3(nullptr);
    if (!avctx) {
        return AVERROR(ENOMEM);
    }

    ret = avcodec_parameters_to_context(
        avctx, ic->streams[stream_index]->codecpar);
    if (ret < 0) {
        goto fail;
    }
    avctx->pkt_timebase = ic->streams[stream_index]->time_base;

    codec = avcodec_find_decoder(avctx->codec_id);

    if (!codec) {
        LOG_ERROR(
            "No decoder could be found for codec %s",
            avcodec_get_name(avctx->codec_id));
        ret = AVERROR(EINVAL);
        goto fail;
    }

    avctx->codec_id = codec->id;
    avctx->lowres = 0;

    if ((ret = avcodec_open2(avctx, codec, nullptr)) < 0) {
        goto fail;
    }

    is->eof = false;
    ic->streams[stream_index]->discard = AVDISCARD_DEFAULT;
    switch (avctx->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
        sample_rate = avctx->sample_rate;
        ret = av_channel_layout_copy(&ch_layout, &avctx->ch_layout);
        if (ret < 0) {
            goto fail;
        }
        has_ch_layout = true;

        if ((ret = M_AudioOpen(is, &ch_layout, sample_rate, &is->audio_tgt))
            < 0) {
            goto fail;
        }
        is->audio_hw_buf_size = ret;
        is->audio_src = is->audio_tgt;
        is->audio_buf_size = 0;
        is->audio_buf_index = 0;

        is->audio_diff_avg_coef = exp(log(0.01) / AUDIO_DIFF_AVG_NB);
        is->audio_diff_avg_count = 0;
        is->audio_diff_threshold =
            (double)(is->audio_hw_buf_size) / is->audio_tgt.bytes_per_sec;

        is->audio_stream = stream_index;
        is->audio_st = ic->streams[stream_index];

        if ((ret = M_DecoderInit(
                 &is->auddec, avctx, &is->audioq, is->continue_read_thread))
            < 0) {
            goto fail;
        }
        if (is->ic->iformat->flags & AVFMT_NOTIMESTAMPS) {
            is->auddec.start_pts = is->audio_st->start_time;
            is->auddec.start_pts_tb = is->audio_st->time_base;
        }
        if (M_DecoderStart(&is->auddec, M_AudioThread, "audio_decoder", is)
            < 0) {
            LOG_ERROR("Error starting audio decoder");
            goto fail;
        }
        SDL_PauseAudioDevice(m_AudioDevice, 0);
        break;

    case AVMEDIA_TYPE_VIDEO:
        is->video_stream = stream_index;
        is->video_st = ic->streams[stream_index];

        if ((ret = M_DecoderInit(
                 &is->viddec, avctx, &is->videoq, is->continue_read_thread))
            < 0) {
            goto fail;
        }
        is->queue_attachments_req = true;
        if ((M_DecoderStart(&is->viddec, M_VideoThread, "video_decoder", is))
            < 0) {
            LOG_ERROR("Error starting video decoder");
            goto fail;
        }
        break;

    default:
        break;
    }
    goto out;

fail:
    avcodec_free_context(&avctx);
out:
    if (has_ch_layout) {
        av_channel_layout_uninit(&ch_layout);
    }

    return ret;
}

static int M_DecodeInterruptCB(void *ctx)
{
    M_STATE *is = ctx;
    return is->abort_request;
}

static int M_StreamHasEnoughPackets(
    AVStream *st, int stream_id, M_PACKET_QUEUE *queue)
{
    return stream_id < 0 || queue->abort_request
        || (st->disposition & AV_DISPOSITION_ATTACHED_PIC)
        || (queue->nb_packets > MIN_FRAMES
            && (!queue->duration
                || av_q2d(st->time_base) * queue->duration > 1.0));
}

static int M_ReadThread(void *arg)
{
    M_STATE *is = arg;
    AVFormatContext *ic = nullptr;
    int err;
    int ret;
    int st_index[AVMEDIA_TYPE_NB];
    AVPacket *pkt = nullptr;
    SDL_mutex *wait_mutex = SDL_CreateMutex();
    int64_t pkt_ts;

    if (!wait_mutex) {
        LOG_ERROR("SDL_CreateMutex(): %s", SDL_GetError());
        ret = AVERROR(ENOMEM);
        goto fail;
    }

    memset(st_index, -1, sizeof(st_index));
    is->eof = false;

    pkt = av_packet_alloc();
    if (!pkt) {
        LOG_ERROR("Could not allocate packet.");
        ret = AVERROR(ENOMEM);
        goto fail;
    }
    ic = avformat_alloc_context();
    if (!ic) {
        LOG_ERROR("Could not allocate context.");
        ret = AVERROR(ENOMEM);
        goto fail;
    }
    ic->interrupt_callback.callback = M_DecodeInterruptCB;
    ic->interrupt_callback.opaque = is;
    err = avformat_open_input(&ic, is->filename, nullptr, nullptr);
    if (err < 0) {
        LOG_ERROR(
            "Error while opening file %s: %s", is->filename, av_err2str(err));
        ret = -1;
        goto fail;
    }

    is->ic = ic;

    avformat_find_stream_info(ic, nullptr);
#if LIBAVFORMAT_VERSION_MAJOR < 59
    av_format_inject_global_side_data(ic);
#endif

    if (ic->pb) {
        ic->pb->eof_reached = 0;
    }

    is->max_frame_duration =
        (ic->iformat->flags & AVFMT_TS_DISCONT) ? 10.0 : 3600.0;

    for (int i = 0; i < (signed)ic->nb_streams; i++) {
        AVStream *st = ic->streams[i];
        enum AVMediaType type = st->codecpar->codec_type;
        st->discard = AVDISCARD_ALL;
    }

    st_index[AVMEDIA_TYPE_VIDEO] = av_find_best_stream(
        ic, AVMEDIA_TYPE_VIDEO, st_index[AVMEDIA_TYPE_VIDEO], -1, nullptr, 0);

    st_index[AVMEDIA_TYPE_AUDIO] = av_find_best_stream(
        ic, AVMEDIA_TYPE_AUDIO, st_index[AVMEDIA_TYPE_AUDIO],
        st_index[AVMEDIA_TYPE_VIDEO], nullptr, 0);

    if (st_index[AVMEDIA_TYPE_VIDEO] >= 0) {
        AVStream *st = ic->streams[st_index[AVMEDIA_TYPE_VIDEO]];
        AVCodecParameters *codecpar = st->codecpar;
        AVRational sar = av_guess_sample_aspect_ratio(ic, st, nullptr);
    }

    if (is->audio_enabled && st_index[AVMEDIA_TYPE_AUDIO] >= 0) {
        M_StreamComponentOpen(is, st_index[AVMEDIA_TYPE_AUDIO]);
    }

    ret = -1;
    if (st_index[AVMEDIA_TYPE_VIDEO] >= 0) {
        ret = M_StreamComponentOpen(is, st_index[AVMEDIA_TYPE_VIDEO]);
    }

    if (is->video_stream < 0 && is->audio_stream < 0) {
        LOG_ERROR("Failed to decode file");
        ret = -1;
        goto fail;
    }

    while (1) {
        if (is->abort_request) {
            break;
        }
        if (is->paused != is->last_paused) {
            is->last_paused = is->paused;
            if (is->paused) {
                is->read_pause_return = av_read_pause(ic);
            } else {
                av_read_play(ic);
            }
        }
        if (is->queue_attachments_req) {
            if (is->video_st
                && is->video_st->disposition & AV_DISPOSITION_ATTACHED_PIC) {
                if ((ret = av_packet_ref(pkt, &is->video_st->attached_pic))
                    < 0) {
                    goto fail;
                }
                M_PacketQueuePut(&is->videoq, pkt);
                M_PacketQueuePutNullPacket(&is->videoq, pkt, is->video_stream);
            }
            is->queue_attachments_req = false;
        }

        if (is->audioq.size + is->videoq.size > MAX_QUEUE_SIZE
            || (M_StreamHasEnoughPackets(
                    is->audio_st, is->audio_stream, &is->audioq)
                && M_StreamHasEnoughPackets(
                    is->video_st, is->video_stream, &is->videoq))) {
            SDL_LockMutex(wait_mutex);
            SDL_CondWaitTimeout(is->continue_read_thread, wait_mutex, 10);
            SDL_UnlockMutex(wait_mutex);
            continue;
        }
        if (!is->paused
            && (!is->audio_st
                || (is->auddec.finished == is->audioq.serial
                    && M_FrameQueueNBRemaining(&is->sampq) == 0))
            && (!is->video_st
                || (is->viddec.finished == is->videoq.serial
                    && M_FrameQueueNBRemaining(&is->pictq) == 0))) {
            ret = AVERROR_EOF;
            goto fail;
        }
        ret = av_read_frame(ic, pkt);
        if (ret < 0) {
            if ((ret == AVERROR_EOF || avio_feof(ic->pb)) && !is->eof) {
                if (is->video_stream >= 0) {
                    M_PacketQueuePutNullPacket(
                        &is->videoq, pkt, is->video_stream);
                }
                if (is->audio_stream >= 0) {
                    M_PacketQueuePutNullPacket(
                        &is->audioq, pkt, is->audio_stream);
                }
                is->eof = true;
            }
            if (ic->pb && ic->pb->error) {
                goto fail;
            }
            SDL_LockMutex(wait_mutex);
            SDL_CondWaitTimeout(is->continue_read_thread, wait_mutex, 10);
            SDL_UnlockMutex(wait_mutex);
            continue;
        } else {
            is->eof = false;
        }
        pkt_ts = pkt->pts == AV_NOPTS_VALUE ? pkt->dts : pkt->pts;
        if (pkt->stream_index == is->audio_stream) {
            M_PacketQueuePut(&is->audioq, pkt);
        } else if (
            pkt->stream_index == is->video_stream
            && !(is->video_st->disposition & AV_DISPOSITION_ATTACHED_PIC)) {
            M_PacketQueuePut(&is->videoq, pkt);
        } else {
            av_packet_unref(pkt);
        }
    }

    ret = 0;
fail:
    if (ic && !is->ic) {
        avformat_close_input(&ic);
    }

    av_packet_free(&pkt);
    is->playback_finished = true;
    SDL_DestroyMutex(wait_mutex);
    return 0;
}

static M_STATE *M_StreamOpen(const char *filename)
{
    M_STATE *const is = av_mallocz(sizeof(M_STATE));
    if (is == nullptr) {
        return nullptr;
    }
    is->video_stream = -1;
    is->audio_stream = -1;

    is->filename = av_strdup(filename);
    if (is->filename == nullptr) {
        goto fail;
    }

    is->iformat = nullptr;

    if (M_FrameQueueInit(&is->pictq, &is->videoq, VIDEO_PICTURE_QUEUE_SIZE, 1)
        < 0) {
        goto fail;
    }
    if (M_FrameQueueInit(&is->sampq, &is->audioq, SAMPLE_QUEUE_SIZE, 1) < 0) {
        goto fail;
    }

    if (M_PacketQueueInit(&is->videoq) < 0) {
        goto fail;
    }
    if (M_PacketQueueInit(&is->audioq) < 0) {
        goto fail;
    }

    if (!(is->continue_read_thread = SDL_CreateCond())) {
        LOG_ERROR("SDL_CreateCond(): %s", SDL_GetError());
        goto fail;
    }

    M_InitClock(&is->vidclk, &is->videoq.serial);
    M_InitClock(&is->audclk, &is->audioq.serial);
    M_InitClock(&is->extclk, &is->extclk.serial);
    is->audio_clock_serial = -1;
    is->audio_volume = SDL_MIX_MAXVOLUME;
    is->av_sync_type = AV_SYNC_AUDIO_MASTER;
    return is;

fail:
    M_StreamClose(is);
    return nullptr;
}

VIDEO *Video_Open(const char *const file_path)
{
    if (file_path == nullptr || String_IsEmpty(file_path)) {
        LOG_ERROR("Cannot open video: empty file path");
        return nullptr;
    }

    LOG_DEBUG("Playing video: %s", file_path);
    int flags = SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER;
    if (SDL_Init(flags)) {
        LOG_ERROR("Could not initialize SDL - %s", SDL_GetError());
        return nullptr;
    }

    SDL_EventState(SDL_SYSWMEVENT, SDL_IGNORE);
    SDL_EventState(SDL_USEREVENT, SDL_IGNORE);

    VIDEO *const result = Memory_Alloc(sizeof(VIDEO));

    result->priv = M_StreamOpen(file_path);
    if (result->priv == nullptr) {
        Memory_Free(result);
        LOG_ERROR("Failed to initialize video!");
        return nullptr;
    }

    result->path = Memory_DupStr(file_path);
    result->is_playing = true;
    return result;
}

void Video_PumpEvents(VIDEO *video)
{
    M_STATE *const is = video->priv;

    if (is->remaining_time > 0.0) {
        av_usleep((int64_t)(is->remaining_time * 1000000.0));
    }
    is->remaining_time = REFRESH_RATE;
    if (!is->paused || is->force_refresh) {
        M_VideoRefresh(is, &is->remaining_time);
    }

    video->is_playing = !is->abort_request && !is->playback_finished;
}

void Video_SetAudioEnabled(VIDEO *const video, const bool enabled)
{
    M_STATE *const is = video->priv;
    is->audio_enabled = enabled;
}

void Video_SetVolume(VIDEO *const video, const double volume)
{
    M_STATE *const is = video->priv;
    is->audio_volume = volume * SDL_MIX_MAXVOLUME;
}

void Video_Start(VIDEO *const video)
{
    M_STATE *const is = video->priv;
    is->remaining_time = 0.0;
    is->read_tid = SDL_CreateThread(M_ReadThread, "read_thread", is);
    if (is->read_tid == nullptr) {
        LOG_ERROR("Error starting read thread: %s", SDL_GetError());
    }
}

void Video_Stop(VIDEO *const video)
{
    M_STATE *const is = video->priv;
    is->abort_request = true;
}

void Video_Close(VIDEO *const video)
{
    M_STATE *const is = video->priv;
    if (is) {
        M_StreamClose(is);
    }

    LOG_DEBUG("Finished playing video: %s", video->path);

    Memory_Free((char *)video->path);
    Memory_Free(video);
}

void Video_SetSurfaceSize(
    VIDEO *const video, const int32_t surface_width,
    const int32_t surface_height)
{
    M_STATE *const is = video->priv;
    if (is->surface_width == surface_width
        && is->surface_height == surface_height) {
        return;
    }

    M_ReallocPrimarySurface(is, surface_width, surface_height, false);
}

void Video_SetSurfacePixelFormat(VIDEO *video, enum AVPixelFormat pixel_format)
{
    M_STATE *const is = video->priv;
    if (is->primary_surface_pixel_format == pixel_format) {
        return;
    }

    is->primary_surface_pixel_format = pixel_format;
    M_ReallocPrimarySurface(is, is->surface_width, is->surface_height, false);
}

void Video_SetSurfaceStride(VIDEO *video, const int32_t stride)
{
    M_STATE *const is = video->priv;
    if (is->primary_surface_stride == stride) {
        return;
    }

    is->primary_surface_stride = stride;
    M_ReallocPrimarySurface(is, is->surface_width, is->surface_height, false);
}

void Video_SetSurfaceAllocatorFunc(
    VIDEO *const video, const VIDEO_SURFACE_ALLOCATOR_FUNC func,
    void *const user_data)
{
    M_STATE *const is = video->priv;
    is->surface_allocator_func = func;
    is->surface_allocator_func_user_data = user_data;
}

void Video_SetSurfaceDeallocatorFunc(
    VIDEO *const video, void (*func)(void *surface, void *user_data),
    void *const user_data)
{
    M_STATE *const is = video->priv;
    is->surface_deallocator_func = func;
    is->surface_deallocator_func_user_data = user_data;
}

void Video_SetSurfaceClearFunc(
    VIDEO *const video, void (*func)(void *surface, void *user_data),
    void *const user_data)
{
    M_STATE *const is = video->priv;
    is->surface_clear_func = func;
    is->surface_clear_func_user_data = user_data;
}

void Video_SetSurfaceLockFunc(
    VIDEO *const video, void *(*func)(void *surface, void *user_data),
    void *const user_data)
{
    M_STATE *const is = video->priv;
    is->surface_lock_func = func;
    is->surface_lock_func_user_data = user_data;
}

void Video_SetSurfaceUnlockFunc(
    VIDEO *const video, void (*func)(void *surface, void *user_data),
    void *const user_data)
{
    M_STATE *const is = video->priv;
    is->surface_unlock_func = func;
    is->surface_unlock_func_user_data = user_data;
}

void Video_SetSurfaceUploadFunc(
    VIDEO *const video, void (*func)(void *surface, void *user_data),
    void *const user_data)
{
    M_STATE *const is = video->priv;
    is->surface_upload_func = func;
    is->surface_upload_func_user_data = user_data;
}

void Video_SetRenderBeginFunc(
    VIDEO *const video, void (*func)(void *surface, void *user_data),
    void *const user_data)
{
    M_STATE *const is = video->priv;
    is->render_begin_func = func;
    is->render_begin_func_user_data = user_data;
}

void Video_SetRenderEndFunc(
    VIDEO *const video, void (*func)(void *surface, void *user_data),
    void *const user_data)
{
    M_STATE *const is = video->priv;
    is->render_end_func = func;
    is->render_end_func_user_data = user_data;
}

void Video_SetExternalAudioClock(VIDEO *const video, const double timestamp)
{
    M_STATE *const is = video->priv;
    M_SetClock(&is->extclk, timestamp, is->extclk.serial);
}
