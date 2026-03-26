#pragma once

#include <trx/config/types.h>
#include <trx/game/music/ids.h>

#include <stdint.h>

#define GYM_ASSAULT_TARGET_TIME (10 * LOGIC_FPS)

typedef enum {
    GYM_TRACK_NONE = -1,
    GYM_TRACK_QUAD,
    GYM_TRACK_ASSAULT,
    GYM_TRACK_NUMBER_OF
} GYM_TRACK_TYPE;

void Gym_SetInventoryOpenEnabled(bool enabled);
bool Gym_IsInventoryOpenEnabled(void);

GYM_TRACK_TYPE Gym_TrackManager_GetActiveTrackType(void);
bool Gym_TrackManager_HasStats(GYM_TRACK_TYPE track);
const GYM_TRACK_STATS *Gym_TrackManager_GetStats(GYM_TRACK_TYPE track);
bool Gym_TrackManager_IsTimerDisplay(GYM_TRACK_TYPE track);
bool Gym_TrackManager_IsTimerActive(GYM_TRACK_TYPE track);
void Gym_TrackManager_Reset(GYM_TRACK_TYPE track);
void Gym_TrackManager_Start(GYM_TRACK_TYPE track);
void Gym_TrackManager_Stop(GYM_TRACK_TYPE track);
void Gym_TrackManager_Finish(GYM_TRACK_TYPE track);

// Assault-only extensions (no-op / 0 for other tracks).
void Gym_TrackManager_AddPenaltySeconds(GYM_TRACK_TYPE track, int32_t seconds);
int32_t Gym_TrackManager_GetPenaltyDisplayTimer(GYM_TRACK_TYPE track);
int32_t Gym_TrackManager_GetPenaltyFrames(GYM_TRACK_TYPE track);
int32_t Gym_TrackManager_GetTargetPenaltyFrames(GYM_TRACK_TYPE track);
bool Gym_TrackManager_OnPadContact(GYM_TRACK_TYPE track, bool on_ground);

// Quad-only extensions (0 for other tracks).
int32_t Gym_TrackManager_GetLapTimeDisplayTimer(GYM_TRACK_TYPE track);
int32_t Gym_TrackManager_GetLapTime(GYM_TRACK_TYPE track);

// TR3 assault course extensions (targets + penalties).
void Gym_Control(void);

// Potentially converts the requested track id based on Lara's state. Returns
// true if the track should be played.
bool Gym_CanPlayMusicTrack(MUSIC_ID *track_id);
