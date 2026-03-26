#include <trx/game/music/backend_cdaudio.h>

#include <trx/av/audio.h>
#include <trx/core/filesystem.h>
#include <trx/core/log.h>
#include <trx/core/memory.h>
#include <trx/core/vector.h>
#include <trx/debug.h>
#include <trx/game/music/const.h>

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint64_t from;
    uint64_t to;
    bool active;
} M_CDAUDIO_TRACK;

typedef struct {
    const char *path;
    const char *control_path;
    const char *description;
    int32_t num_tracks;
    M_CDAUDIO_TRACK *tracks;
} M_BACKEND_DATA;

static bool M_Parse(M_BACKEND_DATA *const data)
{
    ASSERT(data != nullptr);

    char *track_content = nullptr;
    size_t track_content_size;
    if (!File_Load(data->control_path, &track_content, &track_content_size)) {
        LOG_WARNING("Cannot find CDAudio control file: %s", data->control_path);
        return false;
    }

    VECTOR *const tracks = Vector_Create(sizeof(M_CDAUDIO_TRACK));
    size_t offset = 0;
    while (offset < track_content_size) {
        while (track_content[offset] == '\n' || track_content[offset] == '\r') {
            if (++offset >= track_content_size) {
                goto parse_end;
            }
        }

        uint64_t track_num;
        uint64_t from;
        uint64_t to;
        const int32_t result = sscanf(
            &track_content[offset], "%" PRIu64 " %" PRIu64 " %" PRIu64,
            &track_num, &from, &to);

        M_CDAUDIO_TRACK track = {};
        if (result == 3 && track_num > 0) {
            track.active = true;
            track.from = from;
            track.to = to;
        }
        Vector_Add(tracks, (void *)&track);

        while (track_content[offset] != '\n' && track_content[offset] != '\r') {
            if (++offset >= track_content_size) {
                goto parse_end;
            }
        }
    }

parse_end:
    Memory_Free(track_content);

    data->num_tracks = tracks->count;
    const size_t data_size = sizeof(M_CDAUDIO_TRACK) * data->num_tracks;
    data->tracks = Memory_Alloc(data_size);
    memcpy(data->tracks, Vector_GetData(tracks), data_size);
    Vector_Free(tracks);

    // reindex wrong track boundaries
    for (int32_t i = 0; i < data->num_tracks; i++) {
        if (!data->tracks[i].active) {
            continue;
        }

        if (i < data->num_tracks - 1
            && data->tracks[i].from >= data->tracks[i].to) {
            for (int32_t j = i + 1; j < data->num_tracks; j++) {
                if (data->tracks[j].active) {
                    data->tracks[i].to = data->tracks[j].from;
                    break;
                }
            }
        }

        if (data->tracks[i].from >= data->tracks[i].to && i > 0) {
            for (int32_t j = i - 1; j >= 0; j--) {
                if (data->tracks[j].active) {
                    data->tracks[i].from = data->tracks[j].to;
                    break;
                }
            }
        }
    }

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
    File_Close(fp);

    if (!M_Parse(data)) {
        LOG_ERROR("Failed to parse CDAudio data");
        return false;
    }

    return true;
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

    const int32_t track_idx = track_id - 1;
    const M_CDAUDIO_TRACK *track = &data->tracks[track_idx];
    if (track_idx < 0 || track_idx >= data->num_tracks) {
        LOG_ERROR("Invalid track: %d", track_id);
        return -1;
    }

    if (!track->active) {
        LOG_ERROR("Invalid track: %d", track_id);
        return -1;
    }

    const int32_t audio_stream_id = Audio_Stream_CreateFromFile(data->path);
    Audio_Stream_SetStartTimestamp(audio_stream_id, track->from / 1000.0);
    Audio_Stream_SetStopTimestamp(audio_stream_id, track->to / 1000.0);
    Audio_Stream_SeekTimestamp(audio_stream_id, 0.0f);
    return audio_stream_id;
}

static void M_Shutdown(MUSIC_BACKEND *backend)
{
    if (backend == nullptr) {
        return;
    }

    if (backend->data != nullptr) {
        M_BACKEND_DATA *const data = backend->data;
        Memory_FreePointer(&data->path);
        Memory_FreePointer(&data->control_path);
        Memory_FreePointer(&data->description);
        Memory_FreePointer(&data->tracks);
    }
    Memory_FreePointer(&backend->data);
    Memory_FreePointer(&backend);
}

MUSIC_BACKEND *Music_Backend_CDAudio_Factory(
    const char *const path, const char *const control_path)
{
    ASSERT(path != nullptr);
    ASSERT(control_path != nullptr);

    const char *description_fmt = "CDAudio (path: %s)";
    const size_t description_size = snprintf(nullptr, 0, description_fmt, path);
    char *description = Memory_Alloc(description_size + 1);
    sprintf(description, description_fmt, path);

    M_BACKEND_DATA *const data = Memory_Alloc(sizeof(M_BACKEND_DATA));
    data->path = Memory_DupStr(path);
    data->control_path = Memory_DupStr(control_path);
    data->description = description;

    MUSIC_BACKEND *const backend = Memory_Alloc(sizeof(MUSIC_BACKEND));
    backend->data = data;
    backend->init = M_Init;
    backend->describe = M_Describe;
    backend->play = M_Play;
    backend->shutdown = M_Shutdown;
    return backend;
}
