#pragma once

#include <libavutil/pixfmt.h>
#include <stdint.h>

typedef struct {
    const char *path;
    bool is_playing;
    void *priv;
} VIDEO;

typedef void *(*VIDEO_SURFACE_ALLOCATOR_FUNC)(
    int32_t width, int32_t height, void *user_data);

VIDEO *Video_Open(const char *path);
void Video_SetAudioEnabled(VIDEO *video, bool enabled);
void Video_SetVolume(VIDEO *video, double volume);
void Video_SetSurfaceSize(VIDEO *video, int32_t width, int32_t height);
void Video_SetSurfacePixelFormat(VIDEO *video, enum AVPixelFormat pixel_format);
void Video_SetSurfaceStride(VIDEO *video, int32_t stride);
void Video_SetSurfaceAllocatorFunc(
    VIDEO *video, VIDEO_SURFACE_ALLOCATOR_FUNC func, void *user_data);
void Video_SetSurfaceDeallocatorFunc(
    VIDEO *video, void (*func)(void *surface, void *user_data),
    void *user_data);
void Video_SetSurfaceClearFunc(
    VIDEO *video, void (*func)(void *surface, void *user_data),
    void *user_data);
void Video_SetSurfaceLockFunc(
    VIDEO *video, void *(*func)(void *surface, void *user_data),
    void *user_data);
void Video_SetSurfaceUnlockFunc(
    VIDEO *video, void (*func)(void *surface, void *user_data),
    void *user_data);
void Video_SetSurfaceUploadFunc(
    VIDEO *video, void (*func)(void *surface, void *user_data),
    void *user_data);
void Video_SetRenderBeginFunc(
    VIDEO *video, void (*func)(void *surface, void *user_data),
    void *user_data);
void Video_SetRenderEndFunc(
    VIDEO *video, void (*func)(void *surface, void *user_data),
    void *user_data);
void Video_SetExternalAudioClock(VIDEO *video, double timestamp);
void Video_Start(VIDEO *video);
void Video_Stop(VIDEO *video);
void Video_PumpEvents(VIDEO *video);
void Video_Close(VIDEO *video);
