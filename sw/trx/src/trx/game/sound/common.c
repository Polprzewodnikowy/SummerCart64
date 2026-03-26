#include <trx/game/sound/common.h>

#include <trx/av/audio.h>
#include <trx/config.h>
#include <trx/core/log.h>
#include <trx/core/math/geom.h>
#include <trx/core/memory.h>
#include <trx/game/camera.h>
#include <trx/game/game_buf.h>
#include <trx/game/lara.h>
#include <trx/game/random.h>
#include <trx/game/rooms.h>
#include <trx/game/shell.h>
#include <trx/game/sound.h>
#include <trx/version.h>

#include <math.h>
#include <uthash.h>

typedef enum {
    SF_FLIP = 0x40,
    SF_UNFLIP = 0x80,
} SOUND_SOURCE_FLAG;

#define M_DECIBEL_LUT_SIZE 512
#define M_SOUND_CLOSE_RANGE (1 * WALL_L)

#define M_MAX_ACTIVE_SOUNDS AUDIO_MAX_ACTIVE_SAMPLES
#define M_SOUND_RANGE_MULT_CONSTANT 4
#define M_SOUND_MAX_VOLUME 0x8000
#define M_SOUND_MAX_PITCH_CHANGE 6000
#define M_SOUND_MAX_VOLUME_CHANGE (g_TRVersion >= 3 ? 0x1000 : 0x2000)

typedef struct {
    SAMPLE_ID sample_id;
    const SAMPLE_INFO *sample;
    int32_t handle;
    int32_t volume;
    int32_t pitch;
    int32_t pan;

    XYZ_32 initial_pos;
    const XYZ_32 *pos_ptr;
} M_ACTIVE_SOUND;

typedef struct {
    int number;
    int size;
    UT_hash_handle hh;
} M_SAMPLE_DATA_ENTRY;

typedef struct M_SAMPLE_ENTRY {
    SAMPLE_ID sample_id;
    SAMPLE_INFO sample;
    UT_hash_handle hh;
} M_SAMPLE_ENTRY;

static M_ACTIVE_SOUND m_ActiveSounds[M_MAX_ACTIVE_SOUNDS] = {};
static bool m_Initialised = false;
static float m_MasterVolume = 0.0f;
static M_SAMPLE_DATA_ENTRY *m_SampleDataMap = nullptr;
static M_SAMPLE_ENTRY *m_SampleMap = nullptr;
static int32_t m_DecibelLUT[M_DECIBEL_LUT_SIZE] = {};
static int32_t m_SourceCount = 0;
static OBJECT_VECTOR *m_Sources = nullptr;

static int M_SampleDataEntry_Cmp(
    const M_SAMPLE_DATA_ENTRY *const a, const M_SAMPLE_DATA_ENTRY *const b)
{
    return a->number - b->number;
}

static int32_t M_ConvertVolumeToDecibel(const int32_t volume)
{
    int32_t idx = volume * g_Config.audio.master_volume * m_MasterVolume
        * M_DECIBEL_LUT_SIZE / M_SOUND_MAX_VOLUME;
    CLAMP(idx, 0, M_DECIBEL_LUT_SIZE - 1);
    return m_DecibelLUT[idx];
}

static int32_t M_ConvertPanToDecibel(const uint16_t pan)
{
    const int32_t result =
        sin((pan / 32767.0) * M_PI) * (M_DECIBEL_LUT_SIZE / 2);
    if (result > 0) {
        return -m_DecibelLUT[M_DECIBEL_LUT_SIZE - result];
    } else if (result < 0) {
        return m_DecibelLUT[M_DECIBEL_LUT_SIZE + result];
    } else {
        return 0;
    }
}

static float M_ConvertPitch(const int32_t pitch)
{
    return pitch / 0x10000.p0;
}

static int32_t M_GetDistance(
    const SAMPLE_INFO *const sample, const XYZ_32 *const pos)
{
    if (pos == nullptr) {
        return 0;
    }
    const XYZ_32 delta = {
        .x = pos->x - g_Camera.mic_pos.x,
        .y = pos->y - g_Camera.mic_pos.y,
        .z = pos->z - g_Camera.mic_pos.z,
    };
    const int32_t distance = XYZ_32_GetLength(delta);
    if (distance > sample->range) {
        return INT32_MAX;
    } else if (distance < M_SOUND_CLOSE_RANGE) {
        return 0;
    } else {
        return distance - M_SOUND_CLOSE_RANGE;
    }
}

static int32_t M_GetVolume(
    const SAMPLE_INFO *const sample, const int32_t distance, const bool random)
{
    int32_t volume = sample->volume;
    if (random && sample->flags.randomize_volume) {
        volume -= Random_GetDraw() * M_SOUND_MAX_VOLUME_CHANGE / 0x8000;
    }

    if (g_TRVersion == 1) {
        return volume - distance * 3.5f;
    }

    const int32_t attenuation =
        SQUARE(distance) / (SQUARE(sample->range) / 0x10000);
    return (volume * (0x10000 - attenuation)) / 0x10000;
}

static int32_t M_GetPitch(const SAMPLE_INFO *const sample, const uint32_t flags)
{
    int32_t pitch = (flags & SPM_PITCH) != 0 ? (flags >> 8) & 0xFFFFFF
                                             : SOUND_DEFAULT_PITCH;
    pitch += sample->pitch * (1 << 9);
    if (!g_Config.audio.enable_pitched_sounds) {
        return pitch;
    }
    if (sample->flags.randomize_pitch) {
        pitch += ((Random_GetDraw() * M_SOUND_MAX_PITCH_CHANGE) / 0x4000)
            - M_SOUND_MAX_PITCH_CHANGE;
    }
    return pitch;
}

static int32_t M_GetPan(
    const SAMPLE_INFO *const sample, const XYZ_32 *const pos)
{
    if (pos == nullptr) {
        return 0;
    }
    const int32_t distance = M_GetDistance(sample, pos);
    if (distance > 0 && !sample->flags.no_pan) {
        const int32_t dx = pos->x - g_Camera.mic_pos.x;
        const int32_t dz = pos->z - g_Camera.mic_pos.z;
        return (int16_t)Math_Atan(dz, dx) - g_Camera.actual_angle;
    }
    return 0;
}

static M_ACTIVE_SOUND *M_SelectUnusedSound(void)
{
    // Try to get an unused slot
    for (int32_t i = 0; i < M_MAX_ACTIVE_SOUNDS; i++) {
        M_ACTIVE_SOUND *const sound = &m_ActiveSounds[i];
        if (sound->sample == nullptr) {
            return sound;
        }
    }

    // No sound found - try to find the most quiet track, and use this one
    M_ACTIVE_SOUND *best_sound = nullptr;
    int32_t min_volume = INT32_MAX;
    for (int32_t i = 0; i < M_MAX_ACTIVE_SOUNDS; i++) {
        M_ACTIVE_SOUND *const sound = &m_ActiveSounds[i];
        if (sound->sample != nullptr && sound->volume < min_volume) {
            min_volume = sound->volume;
            best_sound = sound;
        }
    }

    return best_sound;
}

static M_ACTIVE_SOUND *M_SelectUsedSound(const SAMPLE_ID sample_id)
{
    for (int32_t i = 0; i < M_MAX_ACTIVE_SOUNDS; i++) {
        M_ACTIVE_SOUND *const result = &m_ActiveSounds[i];
        if (result->sample_id == sample_id) {
            return result;
        }
    }
    return nullptr;
}

static M_ACTIVE_SOUND *M_SelectUsedSoundWithPos(
    const SAMPLE_ID sample_id, const XYZ_32 *const pos)
{
    for (int32_t i = 0; i < M_MAX_ACTIVE_SOUNDS; i++) {
        M_ACTIVE_SOUND *const result = &m_ActiveSounds[i];
        if (result->sample_id == sample_id && result->pos_ptr == pos) {
            return result;
        }
    }
    return nullptr;
}

static void M_ClearActiveSound(M_ACTIVE_SOUND *const sound)
{
    sound->sample = nullptr;
    sound->sample_id = SFX_INVALID;
    sound->handle = AUDIO_NO_SOUND;
}

static void M_ClearAllActiveSounds(void)
{
    for (int32_t i = 0; i < M_MAX_ACTIVE_SOUNDS; i++) {
        M_ACTIVE_SOUND *const sound = &m_ActiveSounds[i];
        M_ClearActiveSound(sound);
    }
}

static void M_CloseActiveSound(M_ACTIVE_SOUND *const sound)
{
    Audio_Sample_Close(sound->handle);
    M_ClearActiveSound(sound);
}

static void M_ClearActiveSoundHandles(const M_ACTIVE_SOUND *const sound)
{
    for (int32_t i = 0; i < M_MAX_ACTIVE_SOUNDS; i++) {
        M_ACTIVE_SOUND *const rsound = &m_ActiveSounds[i];
        if (rsound != sound && rsound->handle == sound->handle) {
            rsound->handle = AUDIO_NO_SOUND;
        }
    }
}

static void M_ClearSampleMaps(void)
{
    M_SAMPLE_DATA_ENTRY *sentry, *stmp;
    HASH_ITER(hh, m_SampleDataMap, sentry, stmp)
    {
        HASH_DEL(m_SampleDataMap, sentry);
        Memory_Free(sentry);
    }
    M_SAMPLE_ENTRY *entry, *tmp;
    HASH_ITER(hh, m_SampleMap, entry, tmp)
    {
        HASH_DEL(m_SampleMap, entry);
        Memory_Free(entry);
    }
}

static void M_SyncActiveSoundHandle(M_ACTIVE_SOUND *const sound)
{
    Audio_Sample_SetPan(sound->handle, M_ConvertPanToDecibel(sound->pan));
    Audio_Sample_SetPitch(sound->handle, M_ConvertPitch(sound->pitch));
    Audio_Sample_SetVolume(
        sound->handle, M_ConvertVolumeToDecibel(sound->volume));
}

static void M_UpdateActiveSoundParams(M_ACTIVE_SOUND *const sound)
{
    const int32_t distance = M_GetDistance(sound->sample, sound->pos_ptr);
    if (distance == INT32_MAX) {
        sound->volume = 0;
        return;
    }
    int32_t volume = M_GetVolume(sound->sample, distance, false);
    if (volume < 0) {
        sound->volume = 0;
        return;
    }

    sound->volume = volume;
    sound->pan = M_GetPan(sound->sample, sound->pos_ptr);
}

bool Sound_Init(void)
{
    m_MasterVolume = g_Config.audio.sound_volume;
    m_DecibelLUT[0] = -10000;

    for (int32_t i = 1; i < M_DECIBEL_LUT_SIZE; i++) {
        if (g_TRVersion < 3) {
            // Legacy scale
            m_DecibelLUT[i] =
                (log2(1.0 / M_DECIBEL_LUT_SIZE) - log2(1.0 / i)) * 1000;
        } else {
            // Hundredths of a dB in the range [-10000..0].
            // Later we apply a linear gain of `10^(centi_dB/2000)`.
            const double gain = (double)i / (double)M_DECIBEL_LUT_SIZE;
            int32_t centi_db = (int32_t)lrint(2000.0 * log10(gain));
            CLAMP(centi_db, -10000, 0);
            m_DecibelLUT[i] = centi_db;
        }
    }

    if (!Audio_Init()) {
        LOG_ERROR("Failed to initialize libtrx sound system");
        return false;
    }

    m_Initialised = true;
    M_ClearAllActiveSounds();
    return true;
}

void Sound_Shutdown(void)
{
    m_Initialised = false;
    Audio_Shutdown();
    M_ClearSampleMaps();
}

bool Sound_IsInitialised(void)
{
    return m_Initialised;
}

void Sound_SetMasterVolume(const float volume)
{
    m_MasterVolume = volume;
}

uint8_t Sound_GetReverbType(void)
{
    return Audio_GetReverbType();
}

void Sound_SetReverbType(uint8_t reverb_type)
{
    Audio_SetReverbType(reverb_type);
}

void Sound_ResetSamples(void)
{
    if (!Sound_IsInitialised()) {
        return;
    }
    Audio_Sample_CloseAll();
    Audio_Sample_UnloadAll();
    M_ClearAllActiveSounds();
    M_ClearSampleMaps();
}

bool Sound_LoadSampleData(
    const int32_t sample_data_id, const char *const sample_data,
    const size_t size)
{
    if (!Sound_IsInitialised()) {
        return false;
    }
    return Audio_Sample_Load(sample_data_id, sample_data, size);
}

int32_t Sound_ReserveSampleData(int32_t index, const int32_t how_many)
{
    M_SAMPLE_DATA_ENTRY *entry;

    if (index != -1) {
        HASH_FIND_INT(m_SampleDataMap, &index, entry);
        if (entry != nullptr) {
            return index;
        }
    } else {
        index = 0;

        // Ensure entries are ordered by starting slot
        HASH_SORT(m_SampleDataMap, M_SampleDataEntry_Cmp);
        // Find first gap large enough for how_many slots
        M_SAMPLE_DATA_ENTRY *e;
        M_SAMPLE_DATA_ENTRY *prev = nullptr;
        for (e = m_SampleDataMap; e != nullptr; prev = e, e = e->hh.next) {
            if (prev == nullptr && e->number >= how_many) {
                index = 0;
                break;
            }
            if (prev != nullptr
                && e->number - (prev->number + prev->size) >= how_many) {
                index = prev->number + prev->size;
                break;
            }
        }
        if (e == nullptr && prev != nullptr) {
            index = prev->number + prev->size;
        }
    }

    entry = Memory_Alloc(sizeof(*entry));
    entry->number = index;
    entry->size = how_many;
    HASH_ADD_INT(m_SampleDataMap, number, entry);
    return index;
}

SAMPLE_INFO *Sound_GetSample(const SAMPLE_ID sample_id)
{
    M_SAMPLE_ENTRY *entry = nullptr;
    if (sample_id == SFX_INVALID) {
        return nullptr;
    }
    HASH_FIND_INT(m_SampleMap, &sample_id, entry);
    return entry != nullptr ? &entry->sample : nullptr;
}

SAMPLE_INFO *Sound_GetOrCreateSample(const SAMPLE_ID sample_id)
{
    SAMPLE_INFO *const sample = Sound_GetSample(sample_id);
    if (sample != nullptr) {
        return sample;
    }
    M_SAMPLE_ENTRY *const entry = Memory_Alloc(sizeof(*entry));
    entry->sample_id = sample_id;
    HASH_ADD_INT(m_SampleMap, sample_id, entry);
    return &entry->sample;
}

bool Sound_IsAvailable_Direct(const SAMPLE_ID sample_id)
{
    return Sound_GetSample(sample_id) != nullptr;
}

bool Sound_IsAvailable(const SAMPLE_TRX_ID sample_id)
{
    return Sound_IsAvailable_Direct(Sound_ToGameID(sample_id));
}

// Get the maximum direct SAMPLE_ID loaded for playback.
// Returns SFX_INVALID if no samples are available.
SAMPLE_ID Sound_GetMaxDirectSampleID(void)
{
    M_SAMPLE_ENTRY *entry, *tmp;
    SAMPLE_ID max_id = SFX_INVALID;
    HASH_ITER(hh, m_SampleMap, entry, tmp)
    {
        if (entry->sample_id > max_id) {
            max_id = entry->sample_id;
        }
    }
    return max_id;
}

void Sound_InitialiseSources(const int32_t num_sources)
{
    m_SourceCount = num_sources;
    m_Sources = num_sources == 0
        ? nullptr
        : GameBuf_Alloc(
              num_sources * sizeof(OBJECT_VECTOR), GBUF_SOUND_SOURCES);
}

int32_t Sound_GetSourceCount(void)
{
    return m_SourceCount;
}

OBJECT_VECTOR *Sound_GetSource(const int32_t source_idx)
{
    if (m_Sources == nullptr) {
        return nullptr;
    }
    return &m_Sources[source_idx];
}

void Sound_ResetSources(void)
{
    const bool flip_status = Room_GetFlipStatus();
    for (int32_t i = 0; i < m_SourceCount; i++) {
        OBJECT_VECTOR *const source = &m_Sources[i];
        if ((flip_status && (source->flags & SF_FLIP))
            || (!flip_status && (source->flags & SF_UNFLIP))) {
            Sound_Effect_Direct(source->data, &source->pos, SPM_NORMAL);
        }
    }
}

bool Sound_Effect_Direct(
    const SAMPLE_ID sample_id, const XYZ_32 *const pos, const uint32_t flags)
{
    if (!Sound_IsInitialised()) {
        return false;
    }

    if ((flags & SPM_ALWAYS) == 0) {
        const bool play_underwater = (flags & SPM_UNDERWATER) != 0;
        const ROOM *const room = Room_Get(g_Camera.pos.room_num);
        const bool room_submerged = room != nullptr && room->flags.underwater;
        if (play_underwater != room_submerged) {
            return false;
        }
    }

    const SAMPLE_INFO *const sample = Sound_GetSample(sample_id);
    if (sample == nullptr || sample->number < 0) {
        return false;
    }

    if (sample->randomness) {
        int32_t r = Random_GetDraw();
        if (g_TRVersion >= 3) {
            r &= 0xFF;
        }
        if (r > sample->randomness) {
            return false;
        }
    }

    const int32_t distance = M_GetDistance(sample, pos);
    if (distance == INT32_MAX) {
        return false;
    }

    const int32_t pan = M_GetPan(sample, pos);
    const int32_t volume = M_GetVolume(sample, distance, true);
    if (volume <= 0) {
        return false;
    }

    const int32_t pitch = M_GetPitch(sample, flags);
    const int32_t num_samples = sample->flags.num_samples;
    const int32_t track_id = num_samples == 1
        ? sample->number
        : sample->number + ((num_samples * Random_GetDraw()) / 0x8000);

    M_ACTIVE_SOUND *sound = nullptr;
    switch (sample->mode) {
    case SAMPLE_MODE_NORMAL:
        sound = M_SelectUnusedSound();
        break;

    case SAMPLE_MODE_WAIT:
        sound = g_TRVersion == 1 ? M_SelectUsedSoundWithPos(sample_id, pos)
                                 : M_SelectUsedSound(sample_id);
        if (sound != nullptr && Audio_Sample_IsPlaying(sound->handle)) {
            return true;
        }
        if (sound == nullptr) {
            sound = M_SelectUnusedSound();
        }
        break;

    case SAMPLE_MODE_RESTART:
        sound = M_SelectUsedSound(sample_id);
        if (sound == nullptr) {
            sound = M_SelectUnusedSound();
        }
        break;

    case SAMPLE_MODE_LOOPED:
        sound = M_SelectUsedSound(sample_id);
        if (sound != nullptr) {
            if (volume > sound->volume) {
                sound->volume = volume;
                sound->pan = pan;
                sound->pitch = pitch;
            }
            return true;
        }
        sound = M_SelectUnusedSound();
        break;
    }

    if (sound == nullptr) {
        return false;
    }

    M_CloseActiveSound(sound);
    const int32_t handle = Audio_Sample_Play(
        track_id, M_ConvertVolumeToDecibel(volume), M_ConvertPitch(pitch),
        M_ConvertPanToDecibel(pan), sample->mode == SAMPLE_MODE_LOOPED);
    if (handle == AUDIO_NO_SOUND) {
        return false;
    }
    sound->sample = sample;
    sound->sample_id = sample_id;
    sound->handle = handle;
    sound->volume = volume;
    sound->pitch = pitch;
    sound->pan = pan;
    if (pos != nullptr) {
        sound->initial_pos = *pos;
        if (flags & SPM_STATIC_POS) {
            sound->pos_ptr = &sound->initial_pos;
        } else {
            sound->pos_ptr = pos;
        }
    } else {
        sound->pos_ptr = nullptr;
    }
    M_ClearActiveSoundHandles(sound);
    return true;
}

bool Sound_Effect(
    const SAMPLE_TRX_ID sample_id, const XYZ_32 *const pos,
    const uint32_t flags)
{
    return Sound_Effect_Direct(Sound_ToGameID(sample_id), pos, flags);
}

void Sound_StopEffect_Direct(const SAMPLE_ID sample_id)
{
    if (!Sound_IsInitialised()) {
        return;
    }
    for (int32_t i = 0; i < M_MAX_ACTIVE_SOUNDS; i++) {
        M_ACTIVE_SOUND *const sound = &m_ActiveSounds[i];
        if (sound->sample_id == sample_id) {
            M_CloseActiveSound(sound);
        }
    }
}

void Sound_StopEffect(const SAMPLE_TRX_ID sample_id)
{
    Sound_StopEffect_Direct(Sound_ToGameID(sample_id));
}

void Sound_ResetAmbient(void)
{
    if (!Sound_IsInitialised()) {
        return;
    }
    Sound_ResetSources();
}

void Sound_UpdateEffects(void)
{
    if (!Sound_IsInitialised()) {
        return;
    }
    for (int32_t i = 0; i < M_MAX_ACTIVE_SOUNDS; i++) {
        M_ACTIVE_SOUND *const sound = &m_ActiveSounds[i];
        if (sound->sample == nullptr) {
            continue;
        }

        if (sound->sample->mode == SAMPLE_MODE_LOOPED) {
            if (sound->volume <= 0) {
                M_CloseActiveSound(sound);
            } else {
                M_SyncActiveSoundHandle(sound);
                sound->volume = 0;
            }
        } else if (!Audio_Sample_IsPlaying(sound->handle)) {
            M_ClearActiveSound(sound);
        } else if (g_TRVersion == 1 && sound->pos_ptr != nullptr) {
            M_UpdateActiveSoundParams(sound);
            if (sound->volume <= 0) {
                M_CloseActiveSound(sound);
            } else {
                M_SyncActiveSoundHandle(sound);
            }
        }
    }
}

void Sound_PauseAll(void)
{
    Audio_Sample_PauseAll();
}

void Sound_UnpauseAll(void)
{
    Audio_Sample_UnpauseAll();
}

void Sound_StopAll(void)
{
    if (!Sound_IsInitialised()) {
        return;
    }
    Audio_Sample_CloseAll();
    M_ClearAllActiveSounds();
}
