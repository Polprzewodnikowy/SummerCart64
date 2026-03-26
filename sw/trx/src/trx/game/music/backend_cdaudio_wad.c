#include <trx/game/music/backend_cdaudio_wad.h>

#include <trx/av/audio.h>
#include <trx/core/filesystem.h>
#include <trx/core/log.h>
#include <trx/core/memory.h>
#include <trx/core/strings.h>
#include <trx/core/utils.h>
#include <trx/debug.h>

#include <stdio.h>
#include <string.h>

#define M_CDAUDIO_WAD_TRACK_COUNT 130
#define M_CDAUDIO_WAD_WFX_OFFSET 20
#define M_CDAUDIO_WAD_WFX_SIZE 50
#define M_CDAUDIO_WAD_WAV_HEADER_SIZE 90

typedef struct {
    char name[260];
    uint32_t size;
    uint32_t offset;
} M_CDAUDIO_WAD_TRACK_DESC;

_Static_assert(
    sizeof(M_CDAUDIO_WAD_TRACK_DESC) == 268,
    "Unexpected cdaudio.wad track info size");

typedef struct {
    uint32_t size;
    uint32_t offset;
    bool active;
} M_CDAUDIO_WAD_TRACK;

typedef struct {
    const char *path;
    const char *description;
    M_CDAUDIO_WAD_TRACK tracks[M_CDAUDIO_WAD_TRACK_COUNT];
    uint8_t wfx[M_CDAUDIO_WAD_WFX_SIZE];
    bool has_wfx;
} M_BACKEND_DATA;

static bool M_LoadTrackAsWaveFile(
    const MUSIC_BACKEND *const backend, const int32_t track_id,
    uint8_t **const out_data, size_t *const out_size)
{
    ASSERT(backend != nullptr);
    ASSERT(out_data != nullptr);
    ASSERT(out_size != nullptr);

    const M_BACKEND_DATA *const data = backend->data;
    ASSERT(data != nullptr);

    *out_data = nullptr;
    *out_size = 0;

    const int32_t track_idx = track_id - 1;
    if (track_idx < 0 || track_idx >= M_CDAUDIO_WAD_TRACK_COUNT) {
        LOG_ERROR("Invalid track: %d", track_id);
        return false;
    }

    const M_CDAUDIO_WAD_TRACK *const track = &data->tracks[track_idx];
    if (!track->active || track->size == 0) {
        LOG_ERROR("Invalid track: %d", track_id);
        return false;
    }

    MYFILE *const fp = File_Open(data->path, FILE_OPEN_READ);
    if (fp == nullptr) {
        return false;
    }

    const size_t file_size = File_Size(fp);
    if ((size_t)track->offset >= file_size) {
        LOG_ERROR(
            "Invalid track offset for %d: offset %lu, file size %zu", track_id,
            track->offset, file_size);
        File_Close(fp);
        return false;
    }

    // Some installations have invalid track sizes which would result in reading
    // beyond EOF if left unchecked. The data can still be valid, so clamp such
    // tracks to the logical remaining file length.
    const size_t remaining = file_size - (size_t)track->offset;
    const size_t track_size =
        M_CDAUDIO_WAD_WAV_HEADER_SIZE + (size_t)track->size;
    const size_t total_size = MIN(track_size, remaining);
    uint8_t *const buf = Memory_Alloc(total_size);

    File_Seek(fp, (size_t)track->offset, FILE_SEEK_SET);
    const bool ok = File_ReadData(fp, buf, total_size);
    File_Close(fp);
    if (!ok) {
        Memory_Free(buf);
        return false;
    }

    *out_data = buf;
    *out_size = total_size;
    return true;
}

static bool M_ReadAllTrackInfos(MYFILE *const fp, M_BACKEND_DATA *const data)
{
    ASSERT(fp != nullptr);
    ASSERT(data != nullptr);

    M_CDAUDIO_WAD_TRACK_DESC track_infos[M_CDAUDIO_WAD_TRACK_COUNT] = {};
    File_Skip(fp, sizeof(M_CDAUDIO_WAD_TRACK_DESC));
    if (!File_ReadItems(
            fp, track_infos, M_CDAUDIO_WAD_TRACK_COUNT,
            sizeof(M_CDAUDIO_WAD_TRACK_DESC))) {
        return false;
    }

    data->has_wfx = false;

    int32_t first_active_idx = -1;
    for (int32_t i = 0; i < M_CDAUDIO_WAD_TRACK_COUNT; i++) {
        const bool is_active = track_infos[i].size != 0;
        data->tracks[i].active = is_active;
        data->tracks[i].size = track_infos[i].size;
        data->tracks[i].offset = track_infos[i].offset;

        if (first_active_idx < 0 && is_active) {
            first_active_idx = i;
        }
    }

    if (first_active_idx < 0) {
        return true;
    }

    const size_t wfx_pos = (size_t)data->tracks[first_active_idx].offset
        + M_CDAUDIO_WAD_WFX_OFFSET;
    File_Seek(fp, wfx_pos, FILE_SEEK_SET);
    if (!File_ReadData(fp, data->wfx, M_CDAUDIO_WAD_WFX_SIZE)) {
        return false;
    }
    data->has_wfx = true;
    return true;
}

static bool M_Init(MUSIC_BACKEND *const backend)
{
    ASSERT(backend != nullptr);
    M_BACKEND_DATA *const data = backend->data;
    ASSERT(data != nullptr);

    MYFILE *const fp = File_Open(data->path, FILE_OPEN_READ);
    if (fp == nullptr) {
        return false;
    }

    const bool ok = M_ReadAllTrackInfos(fp, data);
    File_Close(fp);
    return ok;
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

    uint8_t *wav_data = nullptr;
    size_t wav_size = 0;
    if (!M_LoadTrackAsWaveFile(backend, track_id, &wav_data, &wav_size)) {
        return AUDIO_NO_SOUND;
    }

    const int32_t stream_id = Audio_Stream_CreateFromMemory(wav_data, wav_size);
    if (stream_id < 0) {
        Memory_Free(wav_data);
    }
    return stream_id;
}

static void M_Shutdown(MUSIC_BACKEND *backend)
{
    if (backend == nullptr) {
        return;
    }

    if (backend->data != nullptr) {
        M_BACKEND_DATA *const data = backend->data;
        Memory_FreePointer(&data->path);
        Memory_FreePointer(&data->description);
    }
    Memory_FreePointer(&backend->data);
    Memory_FreePointer(&backend);
}

MUSIC_BACKEND *Music_Backend_CDAudioWad_Factory(const char *const path)
{
    ASSERT(path != nullptr);

    M_BACKEND_DATA *const data = Memory_Alloc(sizeof(M_BACKEND_DATA));
    *data = (M_BACKEND_DATA) {
        .path = Memory_DupStr(path),
        .description = String_Format("CDAudio WAD (path: %s)", path),
    };

    MUSIC_BACKEND *const backend = Memory_Alloc(sizeof(MUSIC_BACKEND));
    backend->data = data;
    backend->init = M_Init;
    backend->describe = M_Describe;
    backend->play = M_Play;
    backend->shutdown = M_Shutdown;
    return backend;
}
