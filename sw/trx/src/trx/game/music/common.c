#include <trx/game/music/common.h>

#include <trx/av/audio.h>
#include <trx/config.h>
#include <trx/core/log.h>
#include <trx/core/memory.h>
#include <trx/core/vector.h>
#include <trx/game/game_flow.h>
#include <trx/game/level.h>
#include <trx/game/music.h>
#include <trx/game/music/backend_cdaudio.h>
#include <trx/game/music/backend_cdaudio_wad.h>
#include <trx/game/music/backend_files.h>
#include <trx/game/shell/paths.h>
#include <trx/version.h>

static bool m_Initialised = false;
static uint16_t m_MusicTrackFlags[MAX_MUSIC_TRACKS] = {};
static MUSIC_ID m_TrackCurrent = MX_INACTIVE;
static MUSIC_ID m_TrackDelayed = MX_INACTIVE;
static MUSIC_ID m_TrackLooped = MX_INACTIVE;
// Remember the last played track, whether normal or looped, to prevent
// immediately restarting it if Lara remains on the same trigger.
static MUSIC_ID m_TrackLastPlayed = MX_INACTIVE;
static MUSIC_ID m_TrackLastLooped = MX_INACTIVE;

typedef struct {
    int32_t audio_stream_id;
    MUSIC_ID track_id;
    MUSIC_PLAY_MODE mode;
    bool active;
} M_MUSIC_STREAM;

static float m_MusicVolume = 0.0f;
static MUSIC_BACKEND *m_Backend = nullptr;
static M_MUSIC_STREAM m_MainStream = {
    .audio_stream_id = -1,
    .track_id = MX_INACTIVE,
    .mode = MPM_ONCE,
    .active = false,
};
static M_MUSIC_STREAM m_OverlayStreams[MUSIC_MAX_OVERLAY_TRACKS] = {};

static MUSIC_BACKEND *M_FindBackend(void)
{
    VECTOR *all_backends = Vector_Create(sizeof(MUSIC_BACKEND *));
    const char *const music_dir =
        TRXPath_PeekResolve(TRX_DYNAMIC_PATH_MUSIC_DIR, nullptr);
    if (music_dir != nullptr) {
        Vector_Add(
            all_backends,
            &(MUSIC_BACKEND *) { Music_Backend_Files_Factory(music_dir) });
    }

    if (g_TRVersion >= 2) {
        const char *const cdaudio_dat_path =
            TRXPath_PeekResolve(TRX_DYNAMIC_PATH_CDAUDIO_FILE, "cdaudio.dat");
        const char *const cdaudio_wav_path =
            TRXPath_PeekResolve(TRX_DYNAMIC_PATH_CDAUDIO_FILE, "cdaudio.wav");
        const char *const cdaudio_mp3_path =
            TRXPath_PeekResolve(TRX_DYNAMIC_PATH_CDAUDIO_FILE, "cdaudio.mp3");

        if (cdaudio_dat_path != nullptr && cdaudio_wav_path != nullptr) {
            Vector_Add(
                all_backends,
                &(MUSIC_BACKEND *) { Music_Backend_CDAudio_Factory(
                    cdaudio_wav_path, cdaudio_dat_path) });
        }
        if (cdaudio_dat_path != nullptr && cdaudio_mp3_path != nullptr) {
            Vector_Add(
                all_backends,
                &(MUSIC_BACKEND *) { Music_Backend_CDAudio_Factory(
                    cdaudio_mp3_path, cdaudio_dat_path) });
        }
    }
    if (g_TRVersion >= 3) {
        const char *const cdaudio_wad_path =
            TRXPath_PeekResolve(TRX_DYNAMIC_PATH_CDAUDIO_FILE, "cdaudio.wad");
        if (cdaudio_wad_path != nullptr) {
            Vector_Add(
                all_backends,
                &(MUSIC_BACKEND *) {
                    Music_Backend_CDAudioWad_Factory(cdaudio_wad_path) });
        }
    }

    MUSIC_BACKEND *backend = nullptr;
    for (int32_t i = 0; i < all_backends->count; i++) {
        MUSIC_BACKEND *const tmp_backend =
            *(MUSIC_BACKEND **)Vector_Get(all_backends, i);
        if (tmp_backend->init(tmp_backend)) {
            backend = tmp_backend;
            break;
        }
    }

    for (int32_t i = 0; i < all_backends->count; i++) {
        MUSIC_BACKEND *const tmp_backend =
            *(MUSIC_BACKEND **)Vector_Get(all_backends, i);
        if (tmp_backend != backend) {
            tmp_backend->shutdown(tmp_backend);
        }
    }
    Vector_Free(all_backends);

    return backend;
}

static void M_StreamReset(M_MUSIC_STREAM *const stream)
{
    stream->audio_stream_id = -1;
    stream->track_id = MX_INACTIVE;
    stream->mode = MPM_ONCE;
    stream->active = false;
}

static void M_StreamClose(M_MUSIC_STREAM *const stream)
{
    if (!stream->active || stream->audio_stream_id < 0) {
        M_StreamReset(stream);
        return;
    }

    // We are only interested in calling M_StreamFinished if a stream
    // finished by itself. In cases where we end the streams early by hand,
    // we clear the finish callback in order to avoid resuming the BGM playback
    // just after we stop it.
    Audio_Stream_SetFinishCallback(stream->audio_stream_id, nullptr, nullptr);
    Audio_Stream_Close(stream->audio_stream_id);
    M_StreamReset(stream);
}

static void M_StopMainStream(void)
{
    M_StreamClose(&m_MainStream);
}

static void M_StopOverlayStreams(void)
{
    for (int32_t i = 0; i < MUSIC_MAX_OVERLAY_TRACKS; i++) {
        M_StreamClose(&m_OverlayStreams[i]);
    }
}

static void M_ResetStreamState(void)
{
    M_StreamReset(&m_MainStream);
    for (int32_t i = 0; i < MUSIC_MAX_OVERLAY_TRACKS; i++) {
        M_StreamReset(&m_OverlayStreams[i]);
    }
}

static void M_StreamFinished(const int32_t stream_id, void *const user_data)
{
    M_MUSIC_STREAM *const stream = user_data;
    if (stream == nullptr) {
        return;
    }
    if (!stream->active || stream->audio_stream_id != stream_id) {
        return;
    }

    if (stream == &m_MainStream) {
        // When the main stream finishes, play the remembered BGM.
        m_TrackCurrent = MX_INACTIVE;
        M_StreamReset(stream);
        if (m_TrackLooped >= 0) {
            m_TrackLastLooped = MX_INACTIVE;
            Music_Play_Direct(m_TrackLooped, MPM_LOOP);
        }
    } else {
        M_StreamReset(stream);
    }
}

static bool M_IsBrokenTrack(const MUSIC_ID track_id)
{
    if (track_id < 0) {
        return true;
    }
    if (g_TRVersion > 1) {
        return false;
    }
    const MUSIC_TRX_ID track = Music_FromGameID(track_id);
    return track == MX_UNUSED_0 || track == MX_UNUSED_1 || track == MX_UNUSED_2;
}

static bool M_IsAmbientTrack(const MUSIC_ID track_id)
{
    const GF_LEVEL *const level = GF_GetCurrentLevel();
    if (level != nullptr && level->music_track == track_id) {
        return true;
    }

    const GF_AMBIENT_DATA *const ambient_data = &g_GameFlow.ambient_tracks;
    if (ambient_data == nullptr) {
        return false;
    }
    for (int32_t i = 0; i < ambient_data->count; i++) {
        if (ambient_data->ids[i] == track_id) {
            return true;
        }
    }
    return false;
}

static void M_SyncVolume(const M_MUSIC_STREAM *const stream)
{
    if (stream == nullptr || !stream->active || stream->audio_stream_id < 0) {
        return;
    }
    const float volume = stream->mode == MPM_OVERLAY
        ? g_Config.audio.music_volume * g_Config.audio.master_volume
        : m_MusicVolume;
    Audio_Stream_SetVolume(stream->audio_stream_id, volume);
}

static void M_SyncVolumes(void)
{
    M_SyncVolume(&m_MainStream);
    for (int32_t i = 0; i < MUSIC_MAX_OVERLAY_TRACKS; i++) {
        M_SyncVolume(&m_OverlayStreams[i]);
    }
}

static int32_t M_GetFreeOverlaySlot(void)
{
    for (int32_t i = 0; i < MUSIC_MAX_OVERLAY_TRACKS; i++) {
        if (!m_OverlayStreams[i].active) {
            return i;
        }
    }
    return -1;
}

static bool M_PlayOverlayTrack(const MUSIC_ID track_id)
{
    if (m_Backend == nullptr) {
        LOG_WARNING(
            "Not playing overlay track %d because no backend is available",
            track_id);
        return false;
    }

    const int32_t slot = M_GetFreeOverlaySlot();
    if (slot < 0) {
        LOG_WARNING(
            "Not playing overlay track %d because all %d overlay slots are in "
            "use",
            track_id, MUSIC_MAX_OVERLAY_TRACKS);
        return false;
    }

    const int32_t stream_id = m_Backend->play(m_Backend, track_id);
    if (stream_id < 0) {
        LOG_ERROR("Failed to create overlay stream for track %d", track_id);
        return false;
    }

    m_OverlayStreams[slot].audio_stream_id = stream_id;
    m_OverlayStreams[slot].track_id = track_id;
    m_OverlayStreams[slot].mode = MPM_OVERLAY;
    m_OverlayStreams[slot].active = true;
    M_SyncVolume(&m_OverlayStreams[slot]);
    Audio_Stream_SetIsLooped(stream_id, false);
    Audio_Stream_SetFinishCallback(
        stream_id, M_StreamFinished, &m_OverlayStreams[slot]);
    return true;
}

static bool M_GetMainTrackState(MUSIC_STREAM_STATE *const state)
{
    if (!m_MainStream.active || state == nullptr) {
        return false;
    }

    state->track_id = MX_INACTIVE;
    state->mode = MPM_ONCE;
    state->timestamp = Audio_Stream_GetTimestamp(m_MainStream.audio_stream_id);

    if (m_TrackCurrent != MX_INACTIVE) {
        state->track_id = m_TrackCurrent;
        return true;
    }
    if (m_TrackLooped != MX_INACTIVE) {
        state->track_id = m_TrackLooped;
        state->mode = MPM_LOOP;
        return true;
    }
    return false;
}

bool Music_Init(void)
{
    m_Initialised = true;
    if (m_Backend != nullptr) {
        m_Backend->shutdown(m_Backend);
        m_Backend = nullptr;
    }
    m_Backend = M_FindBackend();
    if (m_Backend == nullptr) {
        LOG_ERROR("No music backend is available");
        goto finish;
    }

    LOG_INFO("Chosen music backend: %s", m_Backend->describe(m_Backend));
    Music_SetVolume(g_Config.audio.music_volume);

finish:
    m_TrackCurrent = MX_INACTIVE;
    m_TrackLastPlayed = MX_INACTIVE;
    m_TrackDelayed = MX_INACTIVE;
    m_TrackLooped = MX_INACTIVE;
    m_TrackLastLooped = MX_INACTIVE;
    M_ResetStreamState();
    return Audio_Init();
}

void Music_Shutdown(void)
{
    m_Initialised = false;
    M_StopMainStream();
    M_StopOverlayStreams();
    M_ResetStreamState();
    if (m_Backend != nullptr) {
        m_Backend->shutdown(m_Backend);
        m_Backend = nullptr;
    }
    Audio_Shutdown();
}

bool Music_Play_Direct(const MUSIC_ID track_id, const MUSIC_PLAY_MODE mode)
{
    if (!m_Initialised) {
        return false;
    }

    if (M_IsBrokenTrack(track_id)) {
        return false;
    }

    if (mode == MPM_OVERLAY) {
        LOG_INFO("Playing overlay track %d", track_id);
        return M_PlayOverlayTrack(track_id);
    }

    if (track_id == m_TrackCurrent) {
        return true;
    }

    if (mode == MPM_NO_REPEAT && track_id == m_TrackLastPlayed) {
        return true;
    }

    const bool is_looped = mode == MPM_LOOP || M_IsAmbientTrack(track_id);
    if (is_looped && track_id == m_TrackLastLooped) {
        return true;
    }

    if (mode == MPM_DELAY) {
        m_TrackDelayed = track_id;
        return true;
    }

    if (is_looped && m_TrackCurrent != MX_INACTIVE) {
        // OG TR3 behaviour: do not interrupt a regular track when the ambient
        // changes; remember the new ambient and restore it when the track ends.
        m_TrackDelayed = MX_INACTIVE;
        m_TrackLooped = track_id;
        m_TrackLastLooped = track_id;
        return true;
    }

    M_StopMainStream();

    if (m_Backend == nullptr) {
        LOG_WARNING(
            "Not playing track %d because no backend is available", track_id);
        goto finish;
    }

    LOG_INFO("Playing track %d, mode: %d", track_id, mode);

    const int32_t stream_id = m_Backend->play(m_Backend, track_id);
    if (stream_id < 0) {
        LOG_ERROR("Failed to create music stream for track %d", track_id);
        goto finish;
    }

    m_MainStream.audio_stream_id = stream_id;
    m_MainStream.track_id = track_id;
    m_MainStream.mode = is_looped ? MPM_LOOP : MPM_ONCE;
    m_MainStream.active = true;
    M_SyncVolume(&m_MainStream);
    Audio_Stream_SetIsLooped(stream_id, is_looped);
    Audio_Stream_SetFinishCallback(stream_id, M_StreamFinished, &m_MainStream);

finish:
    m_TrackDelayed = MX_INACTIVE;
    if (is_looped) {
        // Reset the regular track outside of M_StreamFinished so that
        // Music_GetCurrentPlayingTrack returns the looped track; otherwise, the
        // stopped track could be stored in the savegame despite being inactive.
        m_TrackCurrent = MX_INACTIVE;
        m_TrackLooped = track_id;
        m_TrackLastLooped = track_id;
    } else {
        m_TrackCurrent = track_id;
        m_TrackLastPlayed = track_id;
    }
    return true;
}

bool Music_Play(const MUSIC_TRX_ID track, const MUSIC_PLAY_MODE mode)
{
    return Music_Play_Direct(Music_ToGameID(track), mode);
}

void Music_Stop(void)
{
    m_TrackCurrent = MX_INACTIVE;
    m_TrackLastPlayed = MX_INACTIVE;
    m_TrackDelayed = MX_INACTIVE;
    m_TrackLooped = MX_INACTIVE;
    m_TrackLastLooped = MX_INACTIVE;
    M_StopMainStream();
    M_StopOverlayStreams();
    M_ResetStreamState();
}

void Music_StopTrack_Direct(const MUSIC_ID track)
{
    if (track != m_TrackCurrent || M_IsBrokenTrack(track)) {
        return;
    }

    M_StopMainStream();
    m_TrackCurrent = MX_INACTIVE;
    if (m_TrackLooped >= 0) {
        Music_Play_Direct(m_TrackLooped, MPM_LOOP);
    }
}

void Music_Pause(void)
{
    if (m_MainStream.active && m_MainStream.audio_stream_id >= 0) {
        Audio_Stream_Pause(m_MainStream.audio_stream_id);
    }
    for (int32_t i = 0; i < MUSIC_MAX_OVERLAY_TRACKS; i++) {
        if (m_OverlayStreams[i].active
            && m_OverlayStreams[i].audio_stream_id >= 0) {
            Audio_Stream_Pause(m_OverlayStreams[i].audio_stream_id);
        }
    }
}

void Music_Unpause(void)
{
    if (m_MainStream.active && m_MainStream.audio_stream_id >= 0) {
        Audio_Stream_Unpause(m_MainStream.audio_stream_id);
    }
    for (int32_t i = 0; i < MUSIC_MAX_OVERLAY_TRACKS; i++) {
        if (m_OverlayStreams[i].active
            && m_OverlayStreams[i].audio_stream_id >= 0) {
            Audio_Stream_Unpause(m_OverlayStreams[i].audio_stream_id);
        }
    }
}

double Music_GetTimestamp(void)
{
    if (!m_MainStream.active || m_MainStream.audio_stream_id < 0) {
        return -1.0;
    }
    return Audio_Stream_GetTimestamp(m_MainStream.audio_stream_id);
}

bool Music_SeekTimestamp(const double timestamp)
{
    if (!m_MainStream.active || m_MainStream.audio_stream_id < 0) {
        return false;
    }
    return Audio_Stream_SeekTimestamp(m_MainStream.audio_stream_id, timestamp);
}

bool Music_SyncTimestamp(const double timestamp)
{
    if (!m_MainStream.active || m_MainStream.audio_stream_id < 0) {
        return false;
    }
    return Audio_Stream_SyncTimestamp(m_MainStream.audio_stream_id, timestamp);
}

int32_t Music_GetStreamCount(void)
{
    int32_t count = 0;
    if (m_MainStream.active) {
        count++;
    }
    for (int32_t i = 0; i < MUSIC_MAX_OVERLAY_TRACKS; i++) {
        if (m_OverlayStreams[i].active) {
            count++;
        }
    }
    return count;
}

bool Music_GetStreamState(
    const int32_t index, MUSIC_STREAM_STATE *const out_state)
{
    if (index < 0 || out_state == nullptr) {
        return false;
    }

    int32_t stream_index = 0;
    if (m_MainStream.active) {
        if (stream_index == index) {
            return M_GetMainTrackState(out_state);
        }
        stream_index++;
    }

    for (int32_t i = 0; i < MUSIC_MAX_OVERLAY_TRACKS; i++) {
        if (!m_OverlayStreams[i].active) {
            continue;
        }

        if (stream_index == index) {
            out_state->track_id = m_OverlayStreams[i].track_id;
            out_state->mode = m_OverlayStreams[i].mode;
            out_state->timestamp =
                Audio_Stream_GetTimestamp(m_OverlayStreams[i].audio_stream_id);
            return true;
        }
        stream_index++;
    }

    return false;
}

bool Music_SeekTrackTimestamp(
    const MUSIC_ID track_id, const MUSIC_PLAY_MODE mode, const double timestamp)
{
    if (mode == MPM_OVERLAY) {
        for (int32_t i = MUSIC_MAX_OVERLAY_TRACKS - 1; i >= 0; i--) {
            if (!m_OverlayStreams[i].active
                || m_OverlayStreams[i].track_id != track_id) {
                continue;
            }
            return Audio_Stream_SeekTimestamp(
                m_OverlayStreams[i].audio_stream_id, timestamp);
        }
        return false;
    }

    MUSIC_STREAM_STATE state = {};
    if (!M_GetMainTrackState(&state)) {
        return false;
    }
    if (state.track_id != track_id || state.mode != mode) {
        return false;
    }
    return Audio_Stream_SeekTimestamp(m_MainStream.audio_stream_id, timestamp);
}

MUSIC_ID Music_GetDelayedTrack(void)
{
    return m_TrackDelayed;
}

MUSIC_ID Music_GetCurrentPlayingTrack(void)
{
    return m_TrackCurrent == MX_INACTIVE ? m_TrackLooped : m_TrackCurrent;
}

MUSIC_ID Music_GetCurrentLoopedTrack(void)
{
    return m_TrackLooped;
}

void Music_SetVolume(float volume)
{
    volume *= g_Config.audio.master_volume;
    if (volume != m_MusicVolume) {
        m_MusicVolume = volume;
        M_SyncVolumes();
    }
}

void Music_ResetTrackFlags(void)
{
    for (int32_t i = 0; i < MAX_MUSIC_TRACKS; i++) {
        m_MusicTrackFlags[i] = 0;
    }
}

uint16_t Music_GetTrackFlags(const MUSIC_ID track_id)
{
    return m_MusicTrackFlags[track_id];
}

void Music_SetTrackFlags(const MUSIC_ID track_id, const uint16_t flags)
{
    m_MusicTrackFlags[track_id] = flags;
}
