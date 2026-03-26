#include <trx/av/image.h>

#include <trx/core/filesystem.h>
#include <trx/core/log.h>
#include <trx/core/memory.h>
#include <trx/debug.h>

#include <errno.h>
#include <libavcodec/avcodec.h>
#include <libavcodec/codec.h>
#include <libavcodec/codec_id.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libavutil/mem.h>
#include <libavutil/pixfmt.h>
#include <libavutil/rational.h>
#include <libswscale/swscale.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    struct {
        int32_t x;
        int32_t y;
        int32_t width;
        int32_t height;
    } src, dst;
} IMAGE_BLIT;

typedef struct {
    int error_code;
    AVFormatContext *format_ctx;
    AVCodecContext *codec_ctx;
    const AVCodec *codec;
    AVFrame *frame;
    AVPacket *packet;
} IMAGE_READER_CONTEXT;

static void M_Free(IMAGE_READER_CONTEXT *const ctx)
{
    if (ctx->packet != nullptr) {
        av_packet_free(&ctx->packet);
    }

    if (ctx->frame != nullptr) {
        av_frame_free(&ctx->frame);
    }

    if (ctx->codec_ctx != nullptr) {
        avcodec_free_context(&ctx->codec_ctx);
    }

    if (ctx->format_ctx != nullptr) {
        avformat_close_input(&ctx->format_ctx);
    }
}

static bool M_Init(const char *const path, IMAGE_READER_CONTEXT *const ctx)
{
    ASSERT(ctx != nullptr);
    ctx->format_ctx = nullptr;
    ctx->codec = nullptr;
    ctx->codec_ctx = nullptr;
    ctx->frame = nullptr;
    ctx->packet = nullptr;

    int32_t error_code = 0;
    error_code = avformat_open_input(&ctx->format_ctx, path, nullptr, nullptr);

    if (error_code != 0) {
        goto finish;
    }

#if 0
    error_code = avformat_find_stream_info(format_ctx, nullptr);
    if (error_code < 0) {
        goto finish;
    }
#endif

    AVStream *video_stream = nullptr;
    for (unsigned int i = 0; i < ctx->format_ctx->nb_streams; i++) {
        AVStream *current_stream = ctx->format_ctx->streams[i];
        if (current_stream->codecpar == nullptr) {
            continue;
        }
        if (current_stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream = current_stream;
            break;
        }
    }
    if (video_stream == nullptr) {
        error_code = AVERROR_STREAM_NOT_FOUND;
        goto finish;
    }

    ctx->codec = avcodec_find_decoder(video_stream->codecpar->codec_id);
    if (ctx->codec == nullptr) {
        error_code = AVERROR_DEMUXER_NOT_FOUND;
        goto finish;
    }

    ctx->codec_ctx = avcodec_alloc_context3(ctx->codec);
    if (ctx->codec_ctx == nullptr) {
        error_code = AVERROR(ENOMEM);
        goto finish;
    }

    error_code =
        avcodec_parameters_to_context(ctx->codec_ctx, video_stream->codecpar);
    if (error_code) {
        goto finish;
    }

#if 0
    ctx->codec_ctx->thread_count = 0;
    if (ctx->codec->capabilities & AV_CODEC_CAP_FRAME_THREADS)
        ctx->codec_ctx->thread_type = FF_THREAD_FRAME;
    else if (ctx->codec->capabilities & AV_CODEC_CAP_SLICE_THREADS)
        ctx->codec_ctx->thread_type = FF_THREAD_SLICE;
    else
        ctx->codec_ctx->thread_count = 1; //don't use multithreading
#endif

    error_code = avcodec_open2(ctx->codec_ctx, ctx->codec, nullptr);
    if (error_code < 0) {
        goto finish;
    }

    ctx->packet = av_packet_alloc();
    error_code = av_read_frame(ctx->format_ctx, ctx->packet);
    if (error_code < 0) {
        goto finish;
    }

    error_code = avcodec_send_packet(ctx->codec_ctx, ctx->packet);
    if (error_code < 0) {
        goto finish;
    }

    ctx->frame = av_frame_alloc();
    if (ctx->frame == nullptr) {
        error_code = AVERROR(ENOMEM);
        goto finish;
    }

    error_code = avcodec_receive_frame(ctx->codec_ctx, ctx->frame);
    if (error_code < 0) {
        goto finish;
    }
    error_code = 0;

finish:
    if (error_code != 0) {
        LOG_ERROR(
            "Error while opening image %s: %s", path, av_err2str(error_code));
        M_Free(ctx);
        return false;
    }

    return true;
}

static IMAGE_BLIT M_GetBlit(
    const int32_t source_width, const int32_t source_height,
    const int32_t target_width, const int32_t target_height,
    IMAGE_FIT_MODE fit_mode)
{
    const float source_ratio = source_width / (float)source_height;
    const float target_ratio = target_width / (float)target_height;

    if (fit_mode == IMAGE_FIT_SMART) {
        const float ar_diff =
            (source_ratio > target_ratio ? source_ratio / target_ratio
                                         : target_ratio / source_ratio)
            - 1.0f;
        if (ar_diff <= 0.1f) {
            // if the difference between aspect ratios is under 10%, just
            // stretch it
            fit_mode = IMAGE_FIT_STRETCH;
        } else if (source_ratio <= target_ratio) {
            // if the viewport is too wide, center the image
            fit_mode = IMAGE_FIT_LETTERBOX;
        } else {
            // if the image is too wide, crop the image
            fit_mode = IMAGE_FIT_CROP;
        }
    }

    IMAGE_BLIT blit;

    switch (fit_mode) {
    case IMAGE_FIT_STRETCH:
        blit.src.width = source_width;
        blit.src.height = source_height;
        blit.src.x = 0;
        blit.src.y = 0;

        blit.dst.width = target_width;
        blit.dst.height = target_height;
        blit.dst.x = 0;
        blit.dst.y = 0;
        break;

    case IMAGE_FIT_CROP:
        blit.src.width = source_ratio < target_ratio
            ? source_width
            : source_height * target_ratio;
        blit.src.height = source_ratio < target_ratio
            ? source_width / target_ratio
            : source_height;
        blit.src.x = (source_width - blit.src.width) / 2;
        blit.src.y = (source_height - blit.src.height) / 2;

        blit.dst.width = target_width;
        blit.dst.height = target_height;
        blit.dst.x = 0;
        blit.dst.y = 0;
        break;

    case IMAGE_FIT_LETTERBOX:
        blit.src.width = source_width;
        blit.src.height = source_height;
        blit.src.x = 0;
        blit.src.y = 0;

        blit.dst.width = (source_ratio > target_ratio)
            ? target_width
            : target_height * source_ratio;
        blit.dst.height = (source_ratio > target_ratio)
            ? target_width / source_ratio
            : target_height;
        blit.dst.x = (target_width - blit.dst.width) / 2;
        blit.dst.y = (target_height - blit.dst.height) / 2;
        break;

    default:
        ASSERT_FAIL();
        break;
    }
    return blit;
}

static IMAGE *M_ConstructImage(
    IMAGE_READER_CONTEXT *const ctx, const int32_t target_width,
    const int32_t target_height, IMAGE_FIT_MODE fit_mode)
{
    ASSERT(ctx != nullptr);
    ASSERT(target_width > 0);
    ASSERT(target_height > 0);

    IMAGE_BLIT blit = M_GetBlit(
        ctx->frame->width, ctx->frame->height, target_width, target_height,
        fit_mode);

    if (blit.src.y != 0 || blit.src.x != 0) {
        ctx->frame->crop_top = blit.src.y;
        ctx->frame->crop_left = blit.src.x;
        av_frame_apply_cropping(ctx->frame, AV_FRAME_CROP_UNALIGNED);
    }

    struct SwsContext *const sws_ctx = sws_getContext(
        blit.src.width, blit.src.height, ctx->frame->format, blit.dst.width,
        blit.dst.height, AV_PIX_FMT_RGB24, SWS_BILINEAR, nullptr, nullptr,
        nullptr);
    if (sws_ctx == nullptr) {
        LOG_ERROR("Failed to get SWS context");
        return nullptr;
    }

    IMAGE *const target_image = Image_Create(target_width, target_height);

    uint8_t *dst_planes[4] = { (uint8_t *)target_image->data
                                   + (blit.dst.y * target_image->width
                                      + blit.dst.x)
                                       * sizeof(IMAGE_PIXEL),
                               nullptr, nullptr, nullptr };
    int dst_linesize[4] = { target_image->width * sizeof(IMAGE_PIXEL), 0, 0,
                            0 };

    sws_scale(
        sws_ctx, (const uint8_t *const *)ctx->frame->data, ctx->frame->linesize,
        0, blit.src.height, dst_planes, dst_linesize);

    sws_freeContext(sws_ctx);
    return target_image;
}

IMAGE *Image_Create(const int width, const int height)
{
    IMAGE *image = Memory_Alloc(sizeof(IMAGE));
    image->width = width;
    image->height = height;
    image->data = Memory_Alloc(width * height * sizeof(IMAGE_PIXEL));
    return image;
}

IMAGE *Image_CreateFromFile(const char *const path)
{
    ASSERT(path != nullptr);

    IMAGE_READER_CONTEXT ctx;
    if (!M_Init(path, &ctx)) {
        return nullptr;
    }

    IMAGE *target_image = M_ConstructImage(
        &ctx, ctx.frame->width, ctx.frame->height, IMAGE_FIT_STRETCH);

    M_Free(&ctx);

    return target_image;
}

IMAGE *Image_CreateFromFileInto(
    const char *const path, const int32_t target_width,
    const int32_t target_height, const IMAGE_FIT_MODE fit_mode)
{
    ASSERT(path != nullptr);

    IMAGE_READER_CONTEXT ctx;
    if (!M_Init(path, &ctx)) {
        return nullptr;
    }

    IMAGE *target_image =
        M_ConstructImage(&ctx, target_width, target_height, fit_mode);

    M_Free(&ctx);

    return target_image;
}

bool Image_SaveToFile(const IMAGE *const image, const char *const path)
{
    ASSERT(image != nullptr);
    ASSERT(path != nullptr);

    bool result = false;

    int error_code = 0;
    const AVCodec *codec = nullptr;
    AVCodecContext *codec_ctx = nullptr;
    AVFrame *frame = nullptr;
    AVPacket *packet = nullptr;
    struct SwsContext *sws_ctx = nullptr;
    MYFILE *fp = nullptr;

    enum AVPixelFormat src_pix_fmt = AV_PIX_FMT_RGB24;
    enum AVPixelFormat dst_pix_fmt;
    enum AVCodecID codec_id;

    if (strstr(path, ".jpg")) {
        dst_pix_fmt = AV_PIX_FMT_YUVJ420P;
        codec_id = AV_CODEC_ID_MJPEG;
    } else if (strstr(path, ".png")) {
        dst_pix_fmt = AV_PIX_FMT_RGB24;
        codec_id = AV_CODEC_ID_PNG;
    } else {
        LOG_ERROR("Cannot determine image format based on path '%s'", path);
        goto cleanup;
    }

    File_EnsureParentDirectories(path);
    fp = File_Open(path, FILE_OPEN_WRITE);
    if (fp == nullptr) {
        LOG_ERROR("Cannot create image file: %s", path);
        goto cleanup;
    }

    codec = avcodec_find_encoder(codec_id);
    if (codec == nullptr) {
        error_code = AVERROR_MUXER_NOT_FOUND;
        goto cleanup;
    }

    codec_ctx = avcodec_alloc_context3(codec);
    if (codec_ctx == nullptr) {
        error_code = AVERROR(ENOMEM);
        goto cleanup;
    }

    codec_ctx->bit_rate = 400000;
    codec_ctx->width = image->width;
    codec_ctx->height = image->height;
    codec_ctx->time_base = (AVRational) { 1, 25 };
    codec_ctx->pix_fmt = dst_pix_fmt;

    if (codec_id == AV_CODEC_ID_MJPEG) {
        // 9 JPEG quality
        codec_ctx->flags |= AV_CODEC_FLAG_QSCALE;
        codec_ctx->global_quality = FF_QP2LAMBDA * 9;
    }

    error_code = avcodec_open2(codec_ctx, codec, nullptr);
    if (error_code < 0) {
        goto cleanup;
    }

    frame = av_frame_alloc();
    if (frame == nullptr) {
        error_code = AVERROR(ENOMEM);
        goto cleanup;
    }
    frame->format = codec_ctx->pix_fmt;
    frame->width = codec_ctx->width;
    frame->height = codec_ctx->height;
    frame->pts = 0;

    error_code = av_image_alloc(
        frame->data, frame->linesize, codec_ctx->width, codec_ctx->height,
        codec_ctx->pix_fmt, 32);
    if (error_code < 0) {
        goto cleanup;
    }

    packet = av_packet_alloc();

    sws_ctx = sws_getContext(
        image->width, image->height, src_pix_fmt, frame->width, frame->height,
        dst_pix_fmt, SWS_BILINEAR, nullptr, nullptr, nullptr);

    if (sws_ctx == nullptr) {
        LOG_ERROR("Failed to get SWS context");
        error_code = AVERROR_EXTERNAL;
        goto cleanup;
    }

    uint8_t *src_planes[4];
    int src_linesize[4];
    av_image_fill_arrays(
        src_planes, src_linesize, (const uint8_t *)image->data, src_pix_fmt,
        image->width, image->height, 1);

    sws_scale(
        sws_ctx, (const uint8_t *const *)src_planes, src_linesize, 0,
        image->height, frame->data, frame->linesize);

    error_code = avcodec_send_frame(codec_ctx, frame);
    if (error_code < 0) {
        goto cleanup;
    }

    while (error_code >= 0) {
        error_code = avcodec_receive_packet(codec_ctx, packet);
        if (error_code == AVERROR(EAGAIN) || error_code == AVERROR_EOF) {
            error_code = 0;
            break;
        }
        if (error_code < 0) {
            goto cleanup;
        }

        File_WriteData(fp, packet->data, packet->size);
        av_packet_unref(packet);
    }

cleanup:
    if (error_code) {
        LOG_ERROR(
            "Error while saving image %s: %s", path, av_err2str(error_code));
    } else {
        result = true;
    }

    if (fp) {
        File_Close(fp);
        fp = nullptr;
    }

    if (sws_ctx) {
        sws_freeContext(sws_ctx);
    }

    if (packet) {
        av_packet_free(&packet);
    }

    if (codec) {
        avcodec_free_context(&codec_ctx);
    }

    if (frame) {
        av_freep(&frame->data[0]);
        av_frame_free(&frame);
    }

    return result;
}

IMAGE *Image_Scale(
    const IMAGE *const source_image, size_t target_width, size_t target_height,
    IMAGE_FIT_MODE fit_mode)
{
    ASSERT(source_image != nullptr);
    ASSERT(source_image->data != nullptr);
    ASSERT(target_width > 0);
    ASSERT(target_height > 0);

    IMAGE_BLIT blit = M_GetBlit(
        source_image->width, source_image->height, target_width, target_height,
        fit_mode);

    struct SwsContext *const sws_ctx = sws_getContext(
        blit.src.width, blit.src.height, AV_PIX_FMT_RGB24, blit.dst.width,
        blit.dst.height, AV_PIX_FMT_RGB24, SWS_BILINEAR, nullptr, nullptr,
        nullptr);
    if (sws_ctx == nullptr) {
        LOG_ERROR("Failed to get SWS context");
        return nullptr;
    }

    IMAGE *const target_image = Image_Create(target_width, target_height);

    uint8_t *src_planes[4] = { (uint8_t *)source_image->data
                                   + (blit.src.y * source_image->width
                                      + blit.src.x)
                                       * sizeof(IMAGE_PIXEL),
                               nullptr, nullptr, nullptr };
    int src_linesize[4] = { source_image->width * sizeof(IMAGE_PIXEL), 0, 0,
                            0 };

    uint8_t *dst_planes[4] = { (uint8_t *)target_image->data
                                   + (blit.dst.y * target_image->width
                                      + blit.dst.x)
                                       * sizeof(IMAGE_PIXEL),
                               nullptr, nullptr, nullptr };
    int dst_linesize[4] = { target_image->width * sizeof(IMAGE_PIXEL), 0, 0,
                            0 };

    sws_scale(
        sws_ctx, (const uint8_t *const *)src_planes, src_linesize, 0,
        blit.src.height, (uint8_t *const *)dst_planes, dst_linesize);

    sws_freeContext(sws_ctx);
    return target_image;
}

void Image_Free(IMAGE *image)
{
    if (image) {
        Memory_FreePointer(&image->data);
    }
    Memory_FreePointer(&image);
}
