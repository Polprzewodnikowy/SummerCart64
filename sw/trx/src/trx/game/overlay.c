#include <trx/game/overlay.h>

#include <trx/config.h>
#include <trx/core/strings.h>
#include <trx/game/const.h>
#include <trx/game/game.h>
#include <trx/game/game_flow.h>
#include <trx/game/gym.h>
#include <trx/game/interpolation.h>
#include <trx/game/inventory.h>
#include <trx/game/inventory_ring.h>
#include <trx/game/music.h>
#include <trx/game/objects.h>
#include <trx/game/output.h>
#include <trx/game/output/sources/ui.h>
#include <trx/game/savegame.h>
#include <trx/game/ui.h>
#include <trx/game/ui/scaler.h>
#include <trx/version.h>

#define M_MAX_PICKUP_DURATION_DISPLAY (LOGIC_FPS * 2)
#define M_MAX_PICKUP_DURATION_EASE_IN (LOGIC_FPS / 2)
#define M_MAX_PICKUP_DURATION_EASE_OUT LOGIC_FPS

typedef enum {
    DPP_EASE_IN,
    DPP_DISPLAY,
    DPP_EASE_OUT,
    DPP_DEAD,
} DISPLAY_PICKUP_PHASE;

typedef struct {
    DISPLAY_PICKUP_PHASE phase;
    OBJECT_ID object_id;
    OUTPUT_UI_PICKUP display;
    int16_t start_rot;
    int32_t elapsed;
    int32_t total_elapsed;
} DISPLAY_PICKUP;

static UI_OVERLAY_STATE *m_UI = nullptr;
static DISPLAY_PICKUP m_Pickups[OUTPUT_UI_MAX_PICKUPS] = {};
static bool m_PickupsActive;

static const RGBA_F m_WhiteTextColor[4] = {
    { 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f },
};

static const RGBA_F m_NeutralTextColor[4] = {
    { 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f },
    { 0.25f, 0.25f, 0.25f, 1.0f },
    { 0.25f, 0.25f, 0.25f, 1.0f },
};

static const RGBA_F m_GreyTextColor[4] = {
    { 0.5f, 0.5f, 0.5f, 1.0f },
    { 0.5f, 0.5f, 0.5f, 1.0f },
    { 0.1f, 0.1f, 0.1f, 1.0f },
    { 0.1f, 0.1f, 0.1f, 1.0f },
};

static const RGBA_F m_GreenTextColor[4] = {
    { 0.35f, 0.75f, 0.2f, 1.0f },
    { 0.35f, 0.75f, 0.2f, 1.0f },
    { 0.1f, 0.25f, 0.0f, 1.0f },
    { 0.1f, 0.25f, 0.0f, 1.0f },
};

static const RGBA_F m_RedTextColor[4] = {
    { 0.9f, 0.2f, 0.0f, 1.0f },
    { 0.9f, 0.2f, 0.0f, 1.0f },
    { 0.3f, 0.0f, 0.0f, 1.0f },
    { 0.3f, 0.0f, 0.0f, 1.0f },
};

static const RGBA_F m_PinkTextColor[4] = {
    { 1.0f, 0.0f, 1.0f, 1.0f },
    { 1.0f, 0.0f, 1.0f, 1.0f },
    { 0.25f, 0.0f, 0.25f, 1.0f },
    { 0.25f, 0.0f, 0.25f, 1.0f },
};

static const char *M_FormatAssaultTimeText(
    const int32_t frames, const bool placeholder)
{
    if (placeholder && frames <= 0) {
        return "--:--.-";
    }
    const int32_t total_sec = frames / LOGIC_FPS;
    const int32_t frame = frames % LOGIC_FPS;
    return String_FormatStatic(
        "%d:%02d.%d", total_sec / 60, total_sec % 60, frame * 10 / LOGIC_FPS);
}

static int32_t M_DrawAssaultTimerText(
    const OBJECT *const digits_obj, const char *const text, int32_t x,
    const int32_t y, const RGBA_F color[4])
{
    const int32_t scale_h =
        UI_Scaler_Calc(PHD_ONE, UI_SCALER_TARGET_ASSAULT_DIGITS);
    const int32_t scale_v =
        UI_Scaler_Calc(PHD_ONE, UI_SCALER_TARGET_ASSAULT_DIGITS);
    const int32_t d0 = UI_Scaler_Calc(-6, UI_SCALER_TARGET_ASSAULT_DIGITS);
    const int32_t d1 = UI_Scaler_Calc(14, UI_SCALER_TARGET_ASSAULT_DIGITS);
    const int32_t d2 = UI_Scaler_Calc(20, UI_SCALER_TARGET_ASSAULT_DIGITS);

    for (const char *c = text; *c != '\0'; c++) {
        if (*c == '-') {
            x += d2;
            continue;
        }

        int32_t mesh_num = 0;
        int32_t offset = 0;
        int32_t width = 0;
        if (*c == ':') {
            mesh_num = 10;
            offset = d0;
            width = d1;
        } else if (*c == '.') {
            mesh_num = 11;
            offset = d0;
            width = d1;
        } else {
            mesh_num = *c - '0';
            offset = 0;
            width = d2;
        }

        x += offset;
        UI_ScheduleDrawScreenSprite(
            x, y, 0, scale_h, scale_v, digits_obj->mesh_idx + mesh_num, color);
        x += width;
    }

    return x;
}

static int32_t M_MeasureAssaultTimerText(const char *const text)
{
    const int32_t d0 = UI_Scaler_Calc(-6, UI_SCALER_TARGET_ASSAULT_DIGITS);
    const int32_t d1 = UI_Scaler_Calc(14, UI_SCALER_TARGET_ASSAULT_DIGITS);
    const int32_t d2 = UI_Scaler_Calc(20, UI_SCALER_TARGET_ASSAULT_DIGITS);

    int32_t w = 0;
    for (const char *c = text; *c != '\0'; c++) {
        if (*c == ':' || *c == '.') {
            w += d0 + d1;
        } else if (*c == '-') {
            w += d2;
        } else {
            w += d2;
        }
    }
    return w;
}

static bool M_IsSprite(const DISPLAY_PICKUP *const pickup)
{
    return !g_Config.visuals.enable_3d_pickups
        || pickup->display.object == nullptr;
}

static float M_Ease(float current, const float start, const float goal)
{
    if (start == goal) {
        return start;
    } else if (start > goal) {
        return 1.0f - M_Ease(current, goal, start);
    } else {
        CLAMP(current, start, goal);
        const float ratio = (current - start) / (goal - start);
        if (ratio < 0.5f) {
            return 2.0f * SQUARE(ratio);
        }
        const float new_ratio = ratio - 1.0f;
        return 1.0f - 2.0f * SQUARE(new_ratio);
    }
}

static void M_DrawTrackTimer(const GYM_TRACK_TYPE track_type)
{
    if (!Gym_TrackManager_IsTimerDisplay(track_type)) {
        return;
    }

    const OBJECT *const digits_obj = Object_Get(O_ASSAULT_DIGITS);
    if (!digits_obj->loaded) {
        return;
    }

    const RESUME_INFO *const resume =
        Savegame_GetCurrentInfo(Game_GetCurrentLevel());
    const char *const buffer =
        M_FormatAssaultTimeText(resume->stats.timer, false);

    const int32_t y = UI_Scaler_Calc(36, UI_SCALER_TARGET_ASSAULT_DIGITS);
    int32_t x = Viewport_GetCenterX(VIEWPORT_UI)
        - UI_Scaler_Calc(50, UI_SCALER_TARGET_ASSAULT_DIGITS);

    M_DrawAssaultTimerText(
        digits_obj, buffer, x, y,
        g_TRVersion < 3 ? m_WhiteTextColor : m_NeutralTextColor);
}

static void M_DrawAssaultPenalties(
    const GYM_TRACK_TYPE track_type, const bool is_target_penalty)
{
    if (!Gym_TrackManager_IsTimerDisplay(track_type)
        || Gym_TrackManager_GetPenaltyDisplayTimer(track_type) <= 0) {
        return;
    }

    const OBJECT *const digits_obj = Object_Get(O_ASSAULT_DIGITS);
    if (!digits_obj->loaded) {
        return;
    }

    const int32_t timer = is_target_penalty
        ? Gym_TrackManager_GetTargetPenaltyFrames(track_type)
        : Gym_TrackManager_GetPenaltyFrames(track_type);
    if (timer <= 0) {
        return;
    }

    const int32_t total_sec = timer / LOGIC_FPS;
    const char *const fmt = is_target_penalty ? "T %d:%02d s" : "%d:%02d s";
    const char *const buffer =
        String_FormatStatic(fmt, total_sec / 60, total_sec % 60);

    const int32_t scale_h =
        UI_Scaler_Calc(PHD_ONE, UI_SCALER_TARGET_ASSAULT_DIGITS);
    const int32_t scale_v =
        UI_Scaler_Calc(PHD_ONE, UI_SCALER_TARGET_ASSAULT_DIGITS);
    const int32_t p = UI_Scaler_Calc(1, UI_SCALER_TARGET_ASSAULT_DIGITS);
    const int32_t d0 = UI_Scaler_Calc(-6, UI_SCALER_TARGET_ASSAULT_DIGITS);
    const int32_t d1 = UI_Scaler_Calc(14, UI_SCALER_TARGET_ASSAULT_DIGITS);
    const int32_t d2 = UI_Scaler_Calc(20, UI_SCALER_TARGET_ASSAULT_DIGITS);
    const int32_t d3 = UI_Scaler_Calc(8, UI_SCALER_TARGET_ASSAULT_DIGITS);

    int32_t x =
        Viewport_GetCenterX(VIEWPORT_UI)
        - UI_Scaler_Calc(
            is_target_penalty ? 193 : 175, UI_SCALER_TARGET_ASSAULT_DIGITS);
    int32_t y = UI_Scaler_Calc(36, UI_SCALER_TARGET_ASSAULT_DIGITS);
    if (is_target_penalty
        && Gym_TrackManager_GetPenaltyFrames(track_type) != 0) {
        y = UI_Scaler_Calc(64, UI_SCALER_TARGET_ASSAULT_DIGITS);
    }

    for (const char *c = buffer; *c != '\0'; c++) {
        if (*c == ' ') {
            x += d3;
        } else if (*c == 'T') {
            x += d0;
            UI_ScheduleDrawScreenSprite(
                x, y + p, 0, scale_h, scale_v, digits_obj->mesh_idx + 12,
                g_TRVersion < 3 ? m_WhiteTextColor : m_NeutralTextColor);
            x += d1 + (p * 2);
        } else if (*c == 's') {
            x += d0;
            UI_ScheduleDrawScreenSprite(
                x - (p * 4), y, 0, scale_h, scale_v, digits_obj->mesh_idx + 13,
                m_PinkTextColor);
            x += d1;
        } else if (*c == ':') {
            x += d0;
            UI_ScheduleDrawScreenSprite(
                x, y, 0, scale_h, scale_v, digits_obj->mesh_idx + 10,
                m_PinkTextColor);
            x += d1;
        } else if (*c == '.') {
            x += d0;
            UI_ScheduleDrawScreenSprite(
                x, y, 0, scale_h, scale_v, digits_obj->mesh_idx + 11,
                m_PinkTextColor);
            x += d1;
        } else {
            UI_ScheduleDrawScreenSprite(
                x, y, 0, scale_h, scale_v, digits_obj->mesh_idx + (*c - '0'),
                m_PinkTextColor);
            x += d2;
        }
    }
}

static int32_t M_GetBestTrackTime(const GYM_TRACK_TYPE track_type)
{
    const GYM_TRACK_STATS *const stats = Gym_TrackManager_GetStats(track_type);
    return stats->total_attempts > 0 ? (int32_t)stats->entries[0].time : 0;
}

static void M_DrawRacetrackLapTimes(const GYM_TRACK_TYPE track_type)
{
    const int32_t last_lap_frames = Gym_TrackManager_GetLapTime(track_type);
    if (last_lap_frames <= 0) {
        return;
    }

    const OBJECT *const digits_obj = Object_Get(O_ASSAULT_DIGITS);
    if (!digits_obj->loaded) {
        return;
    }

    const int32_t best_lap_frames = M_GetBestTrackTime(track_type);
    const bool is_best_lap =
        best_lap_frames > 0 && last_lap_frames == best_lap_frames;

    const char *const last_text =
        M_FormatAssaultTimeText(last_lap_frames, true);
    const char *const best_text = best_lap_frames > 0
        ? M_FormatAssaultTimeText(best_lap_frames, true)
        : "";

    const int32_t w_last = M_MeasureAssaultTimerText(last_text);
    const int32_t w_best = M_MeasureAssaultTimerText(best_text);
    const int32_t gap = UI_Scaler_Calc(20, UI_SCALER_TARGET_ASSAULT_DIGITS);

    const int32_t cx = Viewport_GetCenterX(VIEWPORT_UI);
    const int32_t y = UI_Scaler_Calc(36, UI_SCALER_TARGET_ASSAULT_DIGITS);

    int32_t x = cx - w_last / 2;
    x = M_DrawAssaultTimerText(
        digits_obj, last_text, x, y,
        is_best_lap               ? m_GreenTextColor
            : best_lap_frames > 0 ? m_RedTextColor
                                  : m_NeutralTextColor);
    x += gap;
    M_DrawAssaultTimerText(
        digits_obj, best_text, x, y,
        is_best_lap ? m_GreenTextColor : m_GreyTextColor);
}

static void M_DrawPickup2D(const DISPLAY_PICKUP *const pickup)
{
    const VIEWPORT_RECT pickup_rect =
        OutputSource_UI_GetPickupRect(&pickup->display);
    const int16_t sprite_num = Object_Get(pickup->object_id)->mesh_idx;
    const SPRITE_TEXTURE *const sprite = Output_GetSpriteTexture(sprite_num);
    const float sprite_w = ABS(sprite->x1 - sprite->x0);
    const float sprite_h = ABS(sprite->y1 - sprite->y0);
    const float scale = MIN(pickup_rect.h / sprite_h, pickup_rect.w / sprite_w);
    const float scaled_sprite_w = sprite_w * scale;
    const float scaled_sprite_h = sprite_h * scale;
    const float x = pickup_rect.x + (pickup_rect.w - scaled_sprite_w) / 2;
    const float y = pickup_rect.y + (pickup_rect.h - scaled_sprite_h) / 2;
    OutputSource_UI_StageSprite((OUTPUT_UI_SPRITE) {
        .sprite_idx = sprite_num,
        .x0 = x,
        .y0 = y,
        .x1 = x + (sprite->x1 - sprite->x0) * scale,
        .y1 = y + (sprite->y1 - sprite->y0) * scale,
        .z = Output_GetNearZ_UI(),
        .shade = SHADE_NEUTRAL,
        .color = {
            m_WhiteTextColor[0],
            m_WhiteTextColor[1],
            m_WhiteTextColor[2],
            m_WhiteTextColor[3],
        },
    });
}

static void M_DrawPickup3D(const DISPLAY_PICKUP *const pickup)
{
    OutputSource_UI_StagePickup(pickup->display);
}

static void M_DrawPickups(void)
{
    for (int32_t i = 0; i < OUTPUT_UI_MAX_PICKUPS; i++) {
        DISPLAY_PICKUP *const pickup = &m_Pickups[i];

        int32_t duration = 0;
        float slide_start = 0.0f;
        float slide_goal = 0.0f;
        switch (pickup->phase) {
        case DPP_DEAD:
            continue;
        case DPP_EASE_IN:
            duration = M_MAX_PICKUP_DURATION_EASE_IN;
            slide_start = 0.0f;
            slide_goal = 1.0f;
            break;
        case DPP_DISPLAY:
            duration = M_MAX_PICKUP_DURATION_DISPLAY;
            slide_start = 1.0f;
            slide_goal = 1.0f;
            break;
        case DPP_EASE_OUT:
            duration = M_MAX_PICKUP_DURATION_EASE_OUT;
            slide_start = 1.0f;
            slide_goal = 0.0f;
            break;
        }

        if (M_IsSprite(pickup)) {
            pickup->display.ease = 1.0f;
        } else {
            const float rate = Interpolation_GetRate();
            pickup->display.rot_y = pickup->start_rot
                + (4 * DEG_1 * (pickup->total_elapsed + rate));
            pickup->display.ease = M_Ease(
                (pickup->elapsed + rate) / (float)duration, slide_start,
                slide_goal);
        }

        if (M_IsSprite(pickup)) {
            M_DrawPickup2D(pickup);
        } else {
            M_DrawPickup3D(pickup);
        }
    }
}

static void M_AnimatePickups(const int32_t frames)
{
    m_PickupsActive = false;
    for (int32_t i = 0; i < OUTPUT_UI_MAX_PICKUPS; i++) {
        DISPLAY_PICKUP *const pickup = &m_Pickups[i];
        pickup->elapsed += frames;
        pickup->total_elapsed += frames;
        switch (pickup->phase) {
        case DPP_EASE_IN:
            if (pickup->elapsed >= M_MAX_PICKUP_DURATION_EASE_IN) {
                pickup->elapsed = 0;
                pickup->phase = DPP_DISPLAY;
            }
            m_PickupsActive = true;
            break;
        case DPP_DISPLAY:
            if (pickup->elapsed >= M_MAX_PICKUP_DURATION_DISPLAY) {
                pickup->elapsed = 0;
                pickup->phase = DPP_EASE_OUT;
            }
            m_PickupsActive = true;
            break;
        case DPP_EASE_OUT:
            if (pickup->elapsed >= M_MAX_PICKUP_DURATION_EASE_OUT) {
                pickup->elapsed = 0;
                pickup->phase = DPP_DEAD;
            } else {
                m_PickupsActive = true;
            }
            break;
        case DPP_DEAD:
            continue;
        }
    }
}

void Overlay_Init(void)
{
    if (m_UI == nullptr) {
        m_UI = UI_Overlay_Init();
    }
}

void Overlay_Shutdown(void)
{
    if (m_UI != nullptr) {
        UI_Overlay_Free(m_UI);
        m_UI = nullptr;
    }
}

void Overlay_Reset(void)
{
    for (int32_t i = 0; i < OUTPUT_UI_MAX_PICKUPS; i++) {
        m_Pickups[i].phase = DPP_DEAD;
    }
}

void Overlay_Control(void)
{
    if (m_UI != nullptr) {
        UI_Overlay_Control(m_UI);
    }
}

void Overlay_Animate(int32_t frames)
{
    if (Game_IsPlaying()) {
        M_AnimatePickups(frames);
    }
}

void Overlay_Draw(void)
{
    if (m_UI != nullptr) {
        UI_Overlay(m_UI);
    }
}

void Overlay_DrawGameInfo(void)
{
    if (!Game_IsPlaying()) {
        return;
    }

    if (g_Config.ui.show_pickups_overlay && m_PickupsActive) {
        SceneCompositor_Flush();
        const int32_t old_fog_start = Output_GetFogStart();
        const int32_t old_fog_end = Output_GetFogEnd();
        Output_SetFogStart(20 * WALL_L);
        Output_SetFogEnd(100 * WALL_L);
        M_DrawPickups();
        SceneCompositor_Flush();
        Output_SetFogStart(old_fog_start);
        Output_SetFogEnd(old_fog_end);
    }

    if (Gym_TrackManager_GetLapTimeDisplayTimer(GYM_TRACK_QUAD) > 0) {
        M_DrawRacetrackLapTimes(GYM_TRACK_QUAD);
    } else {
        const GYM_TRACK_TYPE track_type = Gym_TrackManager_GetActiveTrackType();
        if (track_type == GYM_TRACK_NONE) {
            return;
        }
        M_DrawTrackTimer(track_type);
        M_DrawAssaultPenalties(track_type, false);
        M_DrawAssaultPenalties(track_type, true);
    }
}

void Overlay_ForceHealthBar(const bool show)
{
    UI_Overlay_ForceHealthBar(m_UI, show);
}

void Overlay_SetHealthBarTimer(const int16_t timer)
{
    UI_LaraHealthBar_SetTimer(timer);
}

void Overlay_ShowArrow(const UI_OVERLAY_ARROW arrow, const bool show)
{
    if (m_UI != nullptr) {
        UI_Overlay_ShowArrow(m_UI, arrow, show);
    }
}

void Overlay_ShowVersion(const bool show)
{
    if (m_UI != nullptr) {
        UI_Overlay_ShowVersion(m_UI, show);
    }
}

void Overlay_SetTopText(const OVERLAY_TEXT text)
{
    if (m_UI != nullptr) {
        UI_Overlay_SetTopText(m_UI, text);
    }
}

void Overlay_SetBottomText(const OVERLAY_TEXT text)
{
    if (m_UI != nullptr) {
        UI_Overlay_SetBottomText(m_UI, text);
    }
}

void Overlay_AddDisplayPickup(const OBJECT_ID obj_id)
{
    if (Object_IsType(obj_id, g_SecretObjects)) {
        const MUSIC_PLAY_MODE mode =
            g_Config.audio.fix_secrets_killing_music ? MPM_OVERLAY : MPM_ONCE;
        Music_Play(MX_SECRET, mode);
    }

    int32_t grid_x = -1;
    int32_t grid_y = -1;
    for (int32_t i = 0; i < OUTPUT_UI_MAX_PICKUPS; i++) {
        const int32_t x = i % OUTPUT_UI_MAX_PICKUP_COLUMNS;
        const int32_t y = i / OUTPUT_UI_MAX_PICKUP_COLUMNS;
        bool is_occupied = false;
        for (int32_t j = 0; j < OUTPUT_UI_MAX_PICKUPS; j++) {
            DISPLAY_PICKUP *const pickup = &m_Pickups[j];
            const bool is_dead_or_dying = pickup->phase == DPP_DEAD
                || (!M_IsSprite(pickup) && pickup->phase == DPP_EASE_OUT);
            if (pickup->display.grid_x == x && pickup->display.grid_y == y
                && !is_dead_or_dying) {
                is_occupied = true;
                break;
            }
        }
        if (!is_occupied) {
            grid_x = x;
            grid_y = y;
            break;
        }
    }

    for (int32_t i = 0; i < OUTPUT_UI_MAX_PICKUPS; i++) {
        DISPLAY_PICKUP *const pickup = &m_Pickups[i];
        if (pickup->phase != DPP_DEAD) {
            continue;
        }
        const OBJECT_ID inv_object_id = Inv_GetItemOption(obj_id);
        const INVENTORY_ITEM *const inv_item = InvRing_GetInvItem(obj_id);
        pickup->phase = DPP_EASE_IN;
        pickup->object_id = obj_id;
        pickup->display.object =
            inv_object_id != NO_OBJECT ? Object_Get(inv_object_id) : nullptr;
        pickup->display.grid_x = grid_x;
        pickup->display.grid_y = grid_y;
        pickup->start_rot = inv_item != nullptr ? inv_item->y_rot_sel : 0;
        pickup->elapsed = 0;
        pickup->total_elapsed = 0;
        return;
    }
}
