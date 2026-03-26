#include <trx/game/gym.h>

#include <trx/config.h>
#include <trx/core/utils.h>
#include <trx/debug.h>
#include <trx/game/const.h>
#include <trx/game/game.h>
#include <trx/game/game_flow.h>
#include <trx/game/items.h>
#include <trx/game/lara.h>
#include <trx/game/music.h>
#include <trx/game/objects.h>
#include <trx/game/pathing/lot.h>
#include <trx/game/savegame.h>
#include <trx/game/stats.h>
#include <trx/version.h>

#define M_NO_TIME (-1)
#define M_MAX_ASSAULT_TIME_FRAMES (60 * 60 * LOGIC_FPS - 3) // 59:59

typedef struct {
    int32_t is_inventory_open_enabled;
    int16_t completion_timer;
    GYM_TRACK_TYPE active_track_type;

    struct {
        bool timer_display;
        bool timer_active;
        int32_t penalty_display_timer;
        int32_t penalty_frames;
        int32_t target_penalty_frames;
        int32_t timer_auto_hide_timer;
        bool pad_touched_this_frame;
        bool pad_lock;
    } assault_course;

    struct {
        bool timer_display;
        bool timer_active;
        int32_t lap_time;
        int32_t lap_time_display_timer;
    } quad_course;
} M_PRIV;

static M_PRIV m_Priv = {
    .is_inventory_open_enabled = -1,
};

static int32_t M_CountAssaultTargets(void)
{
    int32_t remaining = 0;
    for (int16_t item_num = 0; item_num < Item_GetLevelCount(); item_num++) {
        ITEM *const item = Item_Get(item_num);
        if (item->object_id != O_ASSAULT_TARGET) {
            continue;
        }

        if ((item->flags & IF_KILLED) == 0
            && item->timer > GYM_ASSAULT_TARGET_TIME) {
            remaining++;
        }
    }
    LOG_INFO("remaining=%d", remaining);
    return remaining;
}

static void M_ResetAssaultTargets(void)
{
    M_PRIV *const p = &m_Priv;

    const OBJECT *const obj = Object_Get(O_ASSAULT_TARGET);
    if (!obj->loaded) {
        return;
    }

    for (int16_t item_num = 0; item_num < Item_GetLevelCount(); item_num++) {
        ITEM *const item = Item_Get(item_num);
        if (item->object_id != O_ASSAULT_TARGET) {
            continue;
        }

        if ((item->flags & IF_KILLED) != 0) {
            // Rockets can kill targets via Creature_Die()+Item_Kill(), which
            // removes them from room draw + item lists. Use Item_Initialise()
            // to re-link them back.

            // Clear the flags so that Item_Initialise doesn't mark the item
            // as active preemptively.
            item->flags = 0;

            Item_Initialise(item_num);
        } else if (
            item->status != IS_INACTIVE && obj->initialise_func != nullptr) {
            obj->initialise_func(item_num);
        }
    }
}

static int32_t M_GetBestTime(void)
{
    const GYM_TRACK_STATS *const assault = &g_Config.profile.assault_stats;
    return assault->total_attempts > 0 ? (int32_t)assault->entries[0].time
                                       : M_NO_TIME;
}

static bool M_StoreCourseTime(GYM_TRACK_STATS *const stats, const uint32_t time)
{
    int32_t insert_idx = -1;
    const int32_t limit = MAX_ASSAULT_TIMES;
    for (int32_t i = 0; i < limit; i++) {
        if (stats->entries[i].time == 0 || time < stats->entries[i].time) {
            insert_idx = i;
            break;
        }
    }
    if (insert_idx == -1) {
        return false;
    }

    for (int32_t i = limit - 1; i > insert_idx; i--) {
        stats->entries[i] = stats->entries[i - 1];
    }

    stats->total_attempts++;
    stats->entries[insert_idx].time = time;
    stats->entries[insert_idx].attempt_num = stats->total_attempts;
    Config_Update();
    return true;
}

static bool M_IsOnQuadBike(void)
{
    const ITEM *const vehicle = Lara_Vehicle_GetItem();
    return vehicle != nullptr && vehicle->object_id == O_QUAD_BIKE;
}

void Gym_SetInventoryOpenEnabled(const bool enabled)
{
    M_PRIV *const p = &m_Priv;
    p->is_inventory_open_enabled = enabled;
}

bool Gym_IsInventoryOpenEnabled(void)
{
    M_PRIV *const p = &m_Priv;
    if (p->is_inventory_open_enabled == -1) {
        p->is_inventory_open_enabled = g_TRVersion >= 2;
    }
    return p->is_inventory_open_enabled;
}

void Gym_Control(void)
{
    if (g_TRVersion < 3) {
        return;
    }

    M_PRIV *const p = &m_Priv;

    if (p->assault_course.pad_lock
        && !p->assault_course.pad_touched_this_frame) {
        p->assault_course.pad_lock = false;
    }
    p->assault_course.pad_touched_this_frame = false;

    if (p->assault_course.penalty_display_timer > 0) {
        p->assault_course.penalty_display_timer--;
    }
    if (!p->assault_course.timer_active && p->assault_course.timer_display
        && p->assault_course.timer_auto_hide_timer > 0) {
        p->assault_course.timer_auto_hide_timer--;
        if (p->assault_course.timer_auto_hide_timer == 0) {
            p->assault_course.timer_display = false;
            p->active_track_type = GYM_TRACK_NONE;
        }
    }

    if (p->quad_course.lap_time_display_timer > 0) {
        p->quad_course.lap_time_display_timer--;
    }
}

static void M_Assault_Finish(void)
{
    M_PRIV *const p = &m_Priv;
    if (!p->assault_course.timer_active) {
        return;
    }

    RESUME_INFO *const resume = Savegame_GetCurrentInfo(Game_GetCurrentLevel());

    uint32_t final_time = resume->stats.timer;
    if (g_TRVersion >= 3) {
        const int32_t targets_remaining = M_CountAssaultTargets();
        p->assault_course.penalty_display_timer = 10 * LOGIC_FPS;
        p->assault_course.target_penalty_frames =
            10 * LOGIC_FPS * targets_remaining;
        CLAMPG(
            p->assault_course.target_penalty_frames, M_MAX_ASSAULT_TIME_FRAMES);

        final_time += (uint32_t)p->assault_course.penalty_frames
            + (uint32_t)p->assault_course.target_penalty_frames;
        CLAMPG(final_time, M_MAX_ASSAULT_TIME_FRAMES);
        resume->stats.timer = final_time;
    }

    if (g_TRVersion >= 3) {
        if (final_time < (uint32_t)(180 * LOGIC_FPS)) {
            Music_Play(MX_TR3_GYM_HINT_FAST_TIME, MPM_ONCE);
        }
        p->assault_course.timer_auto_hide_timer = 15 * LOGIC_FPS;
    } else {
        const int32_t current_best_time = M_GetBestTime();
        if (current_best_time <= 0) {
            if (final_time < (uint32_t)(100 * LOGIC_FPS)) {
                // "Gosh! That was my best time yet!"
                Music_Play(MX_TR2_GYM_HINT_15, MPM_ONCE);
            } else {
                // "Congratulations! You did it! But perhaps I could've been
                // faster."
                Music_Play(MX_TR2_GYM_HINT_17, MPM_ONCE);
            }
        } else if (final_time < (uint32_t)current_best_time) {
            // "Gosh! That was my best time yet!"
            Music_Play(MX_TR2_GYM_HINT_15, MPM_ONCE);
        } else if (final_time < (uint32_t)current_best_time + 5 * LOGIC_FPS) {
            // "Almost. Perhaps another try and I might beat it."
            Music_Play(MX_TR2_GYM_HINT_16, MPM_ONCE);
        } else {
            // "Great. But nowhere near my best time."
            Music_Play(MX_TR2_GYM_HINT_14, MPM_ONCE);
        }
    }

    p->assault_course.timer_active = false;

    M_StoreCourseTime(&g_Config.profile.assault_stats, final_time);
}

static void M_Racetrack_Finish(void)
{
    if (!M_IsOnQuadBike()) {
        return;
    }

    M_PRIV *const p = &m_Priv;
    if (!p->quad_course.timer_active) {
        return;
    }

    RESUME_INFO *const resume = Savegame_GetCurrentInfo(Game_GetCurrentLevel());
    uint32_t final_time = resume->stats.timer;
    CLAMPG(final_time, M_MAX_ASSAULT_TIME_FRAMES);
    resume->stats.timer = final_time;

    p->quad_course.lap_time = (int32_t)final_time;
    p->quad_course.lap_time_display_timer = 10 * LOGIC_FPS;
    M_StoreCourseTime(&g_Config.profile.racetrack_stats, final_time);
    p->quad_course.timer_active = false;
}

GYM_TRACK_TYPE Gym_TrackManager_GetActiveTrackType(void)
{
    M_PRIV *const p = &m_Priv;
    return p->active_track_type;
}

bool Gym_TrackManager_HasStats(const GYM_TRACK_TYPE track)
{
    switch (track) {
    case GYM_TRACK_ASSAULT:
        return g_TRVersion >= 2;
    case GYM_TRACK_QUAD:
        return g_TRVersion >= 3;
    default:
        return false;
    }
}

const GYM_TRACK_STATS *Gym_TrackManager_GetStats(const GYM_TRACK_TYPE track)
{
    switch (track) {
    case GYM_TRACK_ASSAULT:
        return &g_Config.profile.assault_stats;
    case GYM_TRACK_QUAD:
        return &g_Config.profile.racetrack_stats;
    default:
        return nullptr;
    }
}

bool Gym_TrackManager_IsTimerDisplay(const GYM_TRACK_TYPE track)
{
    M_PRIV *const p = &m_Priv;
    switch (track) {
    case GYM_TRACK_ASSAULT:
        return p->assault_course.timer_display;
    case GYM_TRACK_QUAD:
        return p->quad_course.timer_display;
    default:
        return false;
    }
}

bool Gym_TrackManager_IsTimerActive(const GYM_TRACK_TYPE track)
{
    M_PRIV *const p = &m_Priv;
    switch (track) {
    case GYM_TRACK_ASSAULT:
        return p->assault_course.timer_active;
    case GYM_TRACK_QUAD:
        return p->quad_course.timer_active;
    default:
        return false;
    }
}

void Gym_TrackManager_Reset(const GYM_TRACK_TYPE track)
{
    M_PRIV *const p = &m_Priv;
    p->active_track_type = GYM_TRACK_NONE;

    switch (track) {
    case GYM_TRACK_ASSAULT:
        p->assault_course.timer_active = false;
        p->assault_course.timer_display = false;
        p->assault_course.penalty_frames = 0;
        p->assault_course.target_penalty_frames = 0;
        p->assault_course.penalty_display_timer = 0;
        p->assault_course.timer_auto_hide_timer = 0;
        p->assault_course.pad_touched_this_frame = false;
        p->assault_course.pad_lock = false;
        break;

    case GYM_TRACK_QUAD:
        p->quad_course.timer_active = false;
        p->quad_course.timer_display = false;
        p->quad_course.lap_time = 0;
        p->quad_course.lap_time_display_timer = 0;
        break;

    default:
        break;
    }
}

void Gym_TrackManager_Start(const GYM_TRACK_TYPE track)
{
    M_PRIV *const p = &m_Priv;
    p->active_track_type = track;

    RESUME_INFO *const resume = Savegame_GetCurrentInfo(Game_GetCurrentLevel());
    resume->stats.timer = 0;

    switch (track) {
    case GYM_TRACK_ASSAULT: {
        p->assault_course.timer_active = true;
        p->assault_course.timer_display = true;
        p->assault_course.penalty_frames = 0;
        p->assault_course.target_penalty_frames = 0;
        p->assault_course.penalty_display_timer = 0;
        p->assault_course.timer_auto_hide_timer = 0;
        p->assault_course.pad_touched_this_frame = false;
        p->assault_course.pad_lock = false;
        M_ResetAssaultTargets();
        break;
    }

    case GYM_TRACK_QUAD: {
        if (!M_IsOnQuadBike()) {
            return;
        }

        p->quad_course.timer_active = true;
        p->quad_course.timer_display = true;
        break;
    default:
        break;
    }
    }
}

void Gym_TrackManager_Stop(const GYM_TRACK_TYPE track)
{
    M_PRIV *const p = &m_Priv;
    p->active_track_type = GYM_TRACK_NONE;

    switch (track) {
    case GYM_TRACK_ASSAULT:
        p->assault_course.timer_active = false;
        p->assault_course.timer_display = true;
        p->assault_course.timer_auto_hide_timer = 0;
        break;

    case GYM_TRACK_QUAD:
        if (!M_IsOnQuadBike()) {
            return;
        }
        p->quad_course.timer_active = false;
        p->quad_course.timer_display = true;
        break;

    default:
        break;
    }
}

void Gym_TrackManager_Finish(const GYM_TRACK_TYPE track)
{
    M_PRIV *const p = &m_Priv;

    switch (track) {
    case GYM_TRACK_ASSAULT:
        M_Assault_Finish();
        break;

    case GYM_TRACK_QUAD:
        M_Racetrack_Finish();
        break;

    default:
        break;
    }
}

void Gym_TrackManager_AddPenaltySeconds(
    const GYM_TRACK_TYPE track, const int32_t seconds)
{
    ASSERT(track == GYM_TRACK_ASSAULT);
    M_PRIV *const p = &m_Priv;
    if (seconds <= 0 || !p->assault_course.timer_active) {
        return;
    }

    p->assault_course.penalty_display_timer = 4 * LOGIC_FPS;
    p->assault_course.penalty_frames += seconds * LOGIC_FPS;
    CLAMPG(p->assault_course.penalty_frames, M_MAX_ASSAULT_TIME_FRAMES);
}

int32_t Gym_TrackManager_GetPenaltyDisplayTimer(const GYM_TRACK_TYPE track)
{
    if (track != GYM_TRACK_ASSAULT) {
        return 0;
    }
    M_PRIV *const p = &m_Priv;
    return p->assault_course.penalty_display_timer;
}

int32_t Gym_TrackManager_GetPenaltyFrames(const GYM_TRACK_TYPE track)
{
    if (track != GYM_TRACK_ASSAULT) {
        return 0;
    }
    M_PRIV *const p = &m_Priv;
    return p->assault_course.penalty_frames;
}

int32_t Gym_TrackManager_GetTargetPenaltyFrames(const GYM_TRACK_TYPE track)
{
    if (track != GYM_TRACK_ASSAULT) {
        return 0;
    }
    M_PRIV *const p = &m_Priv;
    return p->assault_course.target_penalty_frames;
}

bool Gym_TrackManager_OnPadContact(
    const GYM_TRACK_TYPE track, const bool on_ground)
{
    if (g_TRVersion < 3) {
        return true;
    }
    if (track != GYM_TRACK_ASSAULT) {
        return true;
    }
    if (!Game_IsInGym()) {
        return true;
    }

    M_PRIV *const p = &m_Priv;
    p->assault_course.pad_touched_this_frame = true;
    if (!on_ground) {
        return true;
    }
    if (p->assault_course.pad_lock) {
        return false;
    }

    p->assault_course.pad_lock = true;
    return true;
}

int32_t Gym_TrackManager_GetLapTimeDisplayTimer(const GYM_TRACK_TYPE track)
{
    if (track != GYM_TRACK_QUAD) {
        return 0;
    }
    M_PRIV *const p = &m_Priv;
    return p->quad_course.lap_time_display_timer;
}

int32_t Gym_TrackManager_GetLapTime(const GYM_TRACK_TYPE track)
{
    if (track != GYM_TRACK_QUAD) {
        return 0;
    }
    M_PRIV *const p = &m_Priv;
    return p->quad_course.lap_time;
}

bool Gym_CanPlayMusicTrack(MUSIC_ID *const track_id)
{
    const uint16_t flags = Music_GetTrackFlags(*track_id);
    const ITEM *const lara = Lara_GetItem();
    switch (Music_FromGameID(*track_id)) {
    case MX_TR1_GYM_HINT_03:
        if ((flags & IF_ONE_SHOT) != 0
            && lara->current_anim_state == LS(LS_JUMP_UP)) {
            *track_id = Music_ToGameID(MX_TR1_GYM_HINT_04);
        }
        break;

    case MX_TR1_GYM_HINT_12:
        if (lara->current_anim_state != LS(LS_HANG)) {
            return false;
        }
        break;

    case MX_TR1_GYM_HINT_16:
        if (lara->current_anim_state != LS(LS_HANG)) {
            return false;
        }
        break;

    case MX_TR1_GYM_HINT_17:
        if ((flags & IF_ONE_SHOT) != 0
            && lara->current_anim_state == LS(LS_HANG)) {
            *track_id = Music_ToGameID(MX_TR1_GYM_HINT_18);
        }
        break;

    case MX_TR1_GYM_HINT_24:
        if (lara->current_anim_state != LS(LS_SURF_TREAD)) {
            return false;
        }
        break;

    case MX_TR1_GYM_HINT_25:
        if ((flags & IF_ONE_SHOT) != 0) {
            M_PRIV *const p = &m_Priv;
            p->completion_timer++;
            if (p->completion_timer == LOGIC_FPS * 4) {
                Game_SetIsLevelComplete(true);
                p->completion_timer = 0;
            }
        } else if (lara->current_anim_state != LS(LS_WATER_OUT)) {
            return false;
        }
        break;

    default:
        return true;
    }
    return true;
}
