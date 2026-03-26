#pragma once

#include <trx/core/math.h>
#include <trx/game/sound/enum.h>
#include <trx/game/sound/ids.h>
#include <trx/game/sound/types.h>
#include <trx/game/types.h>

#include <stddef.h>

#define SOUND_DEFAULT_PITCH 0x10000

bool Sound_Init(void);
void Sound_Shutdown(void);
bool Sound_IsInitialised(void);

void Sound_SetMasterVolume(float volume);
uint8_t Sound_GetReverbType(void);
void Sound_SetReverbType(uint8_t reverb_type);

void Sound_ResetSamples(void);

bool Sound_LoadSampleData(
    int32_t sample_data_id, const char *sample_data, size_t size);

void Sound_InitialiseSources(int32_t num_sources);
int32_t Sound_GetSourceCount(void);
OBJECT_VECTOR *Sound_GetSource(int32_t source_idx);
void Sound_ResetSources(void);

// Reserve a contiguous block of sample data IDs for loading audio samples.
// Returns the starting sample_data_id for the reserved block of size how_many.
int32_t Sound_ReserveSampleData(int32_t index, int32_t how_many);

// Look up an existing SAMPLE_INFO by SAMPLE_ID. Returns nullptr if not found.
SAMPLE_INFO *Sound_GetSample(SAMPLE_ID sample_id);

// Get or create a SAMPLE_INFO for the given SAMPLE_ID.
// If no sample is found, a new sample slot is created.
SAMPLE_INFO *Sound_GetOrCreateSample(SAMPLE_ID sample_id);

// Returns true if a SAMPLE_INFO exists for the given SAMPLE_ID.
bool Sound_IsAvailable_Direct(SAMPLE_ID sample_id);
bool Sound_IsAvailable(SAMPLE_TRX_ID sample_id);

// Get the maximum direct SAMPLE_ID loaded for playback.
// Returns SFX_INVALID if no samples are available.
SAMPLE_ID Sound_GetMaxDirectSampleID(void);

// Play a sample with the given number.
// pos is an optional argument that takes the world position to play the sound
// at and can be nullptr.
bool Sound_Effect_Direct(SAMPLE_ID sfx_num, const XYZ_32 *pos, uint32_t flags);
bool Sound_Effect(SAMPLE_TRX_ID sfx_num, const XYZ_32 *pos, uint32_t flags);

void Sound_StopEffect_Direct(SAMPLE_ID sfx_num);
void Sound_StopEffect(SAMPLE_TRX_ID sfx_num);

void Sound_ResetAmbient(void);
void Sound_UpdateEffects(void);

void Sound_PauseAll(void);
void Sound_UnpauseAll(void);
void Sound_StopAll(void);
