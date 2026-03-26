#include <trx/game/fmv.h>

#include <trx/av/audio.h>
#include <trx/av/video.h>
#include <trx/config.h>
#include <trx/core/filesystem.h>
#include <trx/core/log.h>
#include <trx/core/memory.h>
#include <trx/core/strings.h>
#include <trx/debug.h>
#include <trx/game/console.h>
#include <trx/game/game_flow.h>
#include <trx/game/input.h>
#include <trx/game/music.h>
#include <trx/game/output.h>
#include <trx/game/output/quad.h>
#include <trx/game/shell.h>
#include <trx/game/sound.h>
#include <trx/game/ui.h>
#include <trx/game/viewport.h>

#include <string.h>

static bool m_IsPlaying = false;

static const char *const m_FallbackExts[] = {
    ".mp4", ".mpeg", ".webm", ".avi", ".fmv", ".rpl", nullptr,
};

typedef struct {
    OUTPUT_QUAD_SURFACE_DESC desc;
    uint8_t *buffer;
} M_SURFACE;

static OUTPUT_QUAD_SURFACE_DESC M_MakeSurfaceDesc(
    const int32_t width, const int32_t height)
{
    return (OUTPUT_QUAD_SURFACE_DESC) {
        .width = width,
        .height = height,
        .bit_count = 32,
        .tex_format = GL_BGRA,
        .tex_type = GL_UNSIGNED_INT_8_8_8_8_REV,
        .uv = {
            { .u = 0.0f, .v = 0.0f },
            { .u = 1.0f, .v = 0.0f },
            { .u = 1.0f, .v = 1.0f },
            { .u = 0.0f, .v = 1.0f },
        },
        .pitch = width * 4,
    };
}

static int32_t M_OpenAudioStream(const char *const file_name)
{
    int32_t audio_id = Audio_Stream_CreateFromFile(file_name);
    if (audio_id != AUDIO_NO_SOUND) {
        return audio_id;
    }

    // The video file may lack an audio stream (e.g. remastered .ogv).
    // Try other FMV extensions to find a file that contains audio.
    const char *const dot = strrchr(file_name, '.');
    if (dot == nullptr) {
        return AUDIO_NO_SOUND;
    }

    const size_t base_len = (size_t)(dot - file_name);
    for (const char *const *ext = m_FallbackExts; *ext != nullptr; ext++) {
        char *const candidate =
            String_Format("%.*s%s", (int)base_len, file_name, *ext);
        if (File_Exists(candidate)) {
            audio_id = Audio_Stream_CreateFromFile(candidate);
            Memory_Free(candidate);
            if (audio_id != AUDIO_NO_SOUND) {
                return audio_id;
            }
        } else {
            Memory_Free(candidate);
        }
    }
    return AUDIO_NO_SOUND;
}

static void *M_AllocateSurface(
    const int32_t width, const int32_t height, void *const user_data)
{
    M_SURFACE *const surface = Memory_Alloc(sizeof(M_SURFACE));
    surface->desc = M_MakeSurfaceDesc(width, height);
    surface->buffer = Memory_Alloc(surface->desc.pitch * surface->desc.height);
    return surface;
}

static void M_DeallocateSurface(void *const surface, void *const user_data)
{
    M_SURFACE *const surface_ = surface;
    Memory_Free(surface_->buffer);
    Memory_Free(surface_);
}

static void M_ClearSurface(void *const surface, void *const user_data)
{
    ASSERT(surface != nullptr);
    M_SURFACE *const surface_ = surface;
    memset(surface_->buffer, 0, surface_->desc.pitch * surface_->desc.height);
}

static void M_RenderBegin(void *const surface, void *const user_data)
{
    Output_BeginScene();
}

static void M_RenderEnd(void *const surface, void *const user_data)
{
    Output_EndScene();
    Output_FlipScreen();
}

static void *M_LockSurface(void *const surface, void *const user_data)
{
    ASSERT(surface != nullptr);
    M_SURFACE *const surface_ = surface;
    return surface_->buffer;
}

static void M_UnlockSurface(void *const surface, void *const user_data)
{
}

static void M_UploadSurface(void *const surface, void *const user_data)
{
    OUTPUT_QUAD *const renderer_2d = user_data;
    M_SURFACE *const surface_ = surface;
    Output_Quad_Upload(renderer_2d, &surface_->desc, surface_->buffer);
    Output_Quad_Render(renderer_2d);

    Output_SwitchViewport(VIEWPORT_UI);
    UI_BeginScene();
    Console_Draw();
    Console_Control();
    Console_Control();
    UI_EndScene();
    UI_Draw();
}

static bool M_Play(const char *const file_name)
{
    if (file_name == nullptr || String_IsEmpty(file_name)) {
        LOG_ERROR("Cannot play FMV: empty file path");
        return false;
    }

    VIDEO *const video = Video_Open(file_name);
    if (video == nullptr) {
        return false;
    }

    OUTPUT_QUAD *renderer_2d = Output_Quad_Create();

    Video_SetSurfaceAllocatorFunc(video, M_AllocateSurface, nullptr);
    Video_SetSurfaceDeallocatorFunc(video, M_DeallocateSurface, nullptr);
    Video_SetSurfaceClearFunc(video, M_ClearSurface, nullptr);
    Video_SetRenderBeginFunc(video, M_RenderBegin, nullptr);
    Video_SetRenderEndFunc(video, M_RenderEnd, nullptr);
    Video_SetSurfaceLockFunc(video, M_LockSurface, nullptr);
    Video_SetSurfaceUnlockFunc(video, M_UnlockSurface, nullptr);
    Video_SetSurfaceUploadFunc(video, M_UploadSurface, renderer_2d);
    Video_SetAudioEnabled(video, false);

    const int32_t audio_id = M_OpenAudioStream(file_name);

    g_OldInputDB = g_Input;
    Video_Start(video);
    while (video->is_playing) {
        Shell_ProcessEvents();

        const float volume = Audio_IsMuted()
            ? 0.0f
            : g_Config.audio.master_volume * g_Config.audio.fmv_volume;
        Audio_Stream_SetVolume(audio_id, volume);
        const double audio_ts = Audio_Stream_GetTimestamp(audio_id);
        if (audio_ts >= 0.0) {
            Video_SetExternalAudioClock(video, audio_ts);
        }

        Video_SetSurfaceSize(
            video, Viewport_GetWidth(VIEWPORT_GAME),
            Viewport_GetHeight(VIEWPORT_GAME));
        Video_SetSurfacePixelFormat(video, AV_PIX_FMT_BGRA);

        Video_PumpEvents(video);

        Input_Update();
        Shell_ProcessInput();
        if (g_InputDB.menu_back || g_InputDB.menu_confirm
            || GF_GetOverrideCommand().action != GF_NOOP || Shell_IsExiting()) {
            Video_Stop(video);
            break;
        }
    }

    Audio_Stream_Close(audio_id);
    Video_Close(video);

    Output_Quad_Destroy(renderer_2d);
    Output_ApplyRenderSettings();
    return true;
}

bool FMV_Play(const char *const file_path)
{
    Music_Stop();
    Sound_StopAll();

    if (!g_Config.gameplay.enable_fmv) {
        return false;
    }

    m_IsPlaying = true;
    const bool result = M_Play(file_path);
    m_IsPlaying = false;
    return result;
}

bool FMV_IsPlaying(void)
{
    return m_IsPlaying;
}
