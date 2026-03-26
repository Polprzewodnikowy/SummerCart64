#include <trx/game/music/backend_files.h>

#include <trx/av/audio.h>
#include <trx/core/filesystem.h>
#include <trx/core/log.h>
#include <trx/core/memory.h>
#include <trx/core/strings.h>
#include <trx/debug.h>
#include <trx/game/shell/paths.h>

#include <stdio.h>

typedef struct {
    const char *dir;
    const char *description;
} M_BACKEND_DATA;

static const char *m_ExtensionsToTry[] = {
    ".flac", ".ogg", ".mp3", ".wav", ".wma", nullptr,
};

static char *M_GetTrackFileName(const char *base_dir, int32_t track)
{
    char *tmp_path = String_Format("%s/track%02d.flac", base_dir, track);
    char *result = TRXPath_GuessExtension(tmp_path, m_ExtensionsToTry);
    Memory_FreePointer(&tmp_path);

    if (result == nullptr) {
        tmp_path = String_Format("%s/%d.flac", base_dir, track);
        result = TRXPath_GuessExtension(tmp_path, m_ExtensionsToTry);
        Memory_FreePointer(&tmp_path);
    }
    return result;
}

static bool M_Init(MUSIC_BACKEND *const backend)
{
    ASSERT(backend != nullptr);
    const M_BACKEND_DATA *data = backend->data;
    ASSERT(data->dir != nullptr);
    return File_DirExists(data->dir);
}

static const char *M_Describe(const MUSIC_BACKEND *const backend)
{
    ASSERT(backend != nullptr);
    const M_BACKEND_DATA *const data = backend->data;
    ASSERT(data != nullptr);
    return data->description;
}

static int32_t M_Play(
    const MUSIC_BACKEND *const backend, const int32_t track_id)
{
    ASSERT(backend != nullptr);
    const M_BACKEND_DATA *const data = backend->data;
    ASSERT(data != nullptr);

    char *file_path = M_GetTrackFileName(data->dir, track_id);
    if (file_path == nullptr) {
        LOG_ERROR("Invalid track: %d", track_id);
        return -1;
    }

    const int32_t stream_id = Audio_Stream_CreateFromFile(file_path);
    Memory_Free(file_path);
    return stream_id;
}

static void M_Shutdown(MUSIC_BACKEND *backend)
{
    if (backend == nullptr) {
        return;
    }

    if (backend->data != nullptr) {
        M_BACKEND_DATA *const data = backend->data;
        Memory_FreePointer(&data->dir);
        Memory_FreePointer(&data->description);
    }
    Memory_FreePointer(&backend->data);
    Memory_FreePointer(&backend);
}

MUSIC_BACKEND *Music_Backend_Files_Factory(const char *path)
{
    ASSERT(path != nullptr);

    const char *description_fmt = "Directory (directory: %s)";
    const size_t description_size = snprintf(nullptr, 0, description_fmt, path);
    char *description = Memory_Alloc(description_size + 1);
    sprintf(description, description_fmt, path);

    M_BACKEND_DATA *const data = Memory_Alloc(sizeof(M_BACKEND_DATA));
    data->dir = Memory_DupStr(path);
    data->description = description;

    MUSIC_BACKEND *const backend = Memory_Alloc(sizeof(MUSIC_BACKEND));
    backend->data = data;
    backend->init = M_Init;
    backend->describe = M_Describe;
    backend->play = M_Play;
    backend->shutdown = M_Shutdown;
    return backend;
}
