#pragma once

#include <trx/game/music/enum.h>
#include <trx/game/music/ids.h>

#include <stdint.h>

#define MUSIC_MAX_OVERLAY_TRACKS 3

typedef struct {
    MUSIC_ID track_id;
    MUSIC_PLAY_MODE mode;
    double timestamp;
} MUSIC_STREAM_STATE;

bool Music_Init(void);
void Music_Shutdown(void);

// Stops playing current track and plays a single track.
//
// MPM_ONCE:
//   Plays the track once. Once playback is done, if there is an active looped
//   track, the playback resumes from the start of the looped track.
// MPM_LOOP:
//   Activates looped playback for the chosen track.
// MPM_NO_REPEAT:
//   A track with this play mode will not trigger in succession.
// MPM_DELAY:
//   A track does not get played and instead is only marked for later playback.
//   The track to play is available with Music_GetDelayedTrack().
// MPM_OVERLAY:
//   Plays a non-looping track without interrupting active background music.
bool Music_Play_Direct(MUSIC_ID track, MUSIC_PLAY_MODE mode);

// Stops the provided single track and restarts the looped track if applicable.
void Music_StopTrack_Direct(MUSIC_ID track);

// Play a music track with a semantical ID that will get mapped to a specific
// music track slot depending on the game.
bool Music_Play(MUSIC_TRX_ID track, MUSIC_PLAY_MODE mode);

// Stops all music streams, including looped, active, and overlay tracks.
void Music_Stop(void);

// Pauses the music.
void Music_Pause(void);

// Unpauses the music.
void Music_Unpause(void);

// Get the current timestamp of the current stream in seconds.
double Music_GetTimestamp(void);

// Seek to timestamp of current stream.
bool Music_SeekTimestamp(double timestamp);

// Seeks to the given timestamp if the drift is too big.
bool Music_SyncTimestamp(double timestamp);

// Returns the number of currently active serializable streams.
int32_t Music_GetStreamCount(void);

// Returns stream state by active index [0..Music_GetStreamCount()).
bool Music_GetStreamState(int32_t index, MUSIC_STREAM_STATE *state);

// Seeks timestamp for the active stream that matches track and mode.
bool Music_SeekTrackTimestamp(
    MUSIC_ID track, MUSIC_PLAY_MODE mode, double timestamp);

// Returns the delayed track. Ignores looped tracks.
MUSIC_ID Music_GetDelayedTrack(void);

// Returns the currently playing track. Includes looped music.
MUSIC_ID Music_GetCurrentPlayingTrack(void);

// Returns the looped track.
MUSIC_ID Music_GetCurrentLoopedTrack(void);

// Sets the game volume.
void Music_SetVolume(float volume);

// Resets all track trigger mask flags.
void Music_ResetTrackFlags(void);

// Returns trigger mask flags for the given track.
uint16_t Music_GetTrackFlags(MUSIC_ID track_id);

// Sets the trigger mask flags for the given track.
void Music_SetTrackFlags(MUSIC_ID track_id, uint16_t flags);
