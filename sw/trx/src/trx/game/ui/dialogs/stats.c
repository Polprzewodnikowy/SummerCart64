#include <trx/config.h>
#include <trx/core/memory.h>
#include <trx/core/strings.h>
#include <trx/debug.h>
#include <trx/game/const.h>
#include <trx/game/creature.h>
#include <trx/game/game.h>
#include <trx/game/game_flow.h>
#include <trx/game/gym.h>
#include <trx/game/objects.h>
#include <trx/game/savegame.h>
#include <trx/game/stats.h>
#include <trx/game/ui.h>
#include <trx/version.h>

#include <stdio.h>
#include <string.h>

#define M_MIN_ASSAULT_COURSE_ROWS 7

typedef enum {
    M_ROW_GENERIC,
    M_ROW_LEVEL_COUNTER,
    M_ROW_TIMER,
    M_ROW_ICON_SECRETS,
    M_ROW_NUM_SECRETS,
    M_ROW_PICKUPS,
    M_ROW_DEATHS,
    M_ROW_KILLS,
    M_ROW_AMMO,
    M_ROW_AMMO_USED,
    M_ROW_AMMO_HITS,
    M_ROW_MEDIPACKS_USED,
    M_ROW_DISTANCE_TRAVELLED,
    M_ROW_ASSAULT_COURSE_TITLE,
    M_ROW_ASSAULT_COURSE_ROW,
    M_ROW_ASSAULT_NO_TIMES_SET,
    M_ROW_RACETRACK_TITLE,
    M_ROW_RACETRACK_ROW,
    M_ROW_SPACER,
} M_ROW_ROLE;

typedef struct {
    float window_margin;
    float window_y;
    float title_spacing;
    float min_width;
    float row_spacing;
    bool use_full_hours;
} M_LOOK;

typedef struct UI_STATS_DIALOG_STATE {
    UI_STATS_DIALOG_ARGS args;
    UI_SCROLLABLE scrollable;

    union {
        struct {
            const STATS_COMMON *stats;
            const LEVEL_MAX_STATS *max_stats;
            FINAL_STATS final_stats;
            LEVEL_MAX_STATS adjusted_max_stats;
        };
        const GYM_TRACK_STATS *assault_stats[GYM_TRACK_NUMBER_OF];
    };

    const M_LOOK *look;
    bool has_floordata_secrets;
} UI_STATS_DIALOG_STATE;

static const M_LOOK m_Looks[TR_VERSION_COUNT] = {
    [0] = {
        .window_margin = 0.0f,
        .window_y = 0.5f,
        .title_spacing = 4.0f,
        .min_width = 0.0f,
        .row_spacing = 30.0f,
        .use_full_hours = false,
    },
    [1] = {
        .window_margin = 40.0f,
        .window_y = 1.0f,
        .title_spacing = 3.0f,
        .min_width = 290.0f,
        .row_spacing = 25.0f,
        .use_full_hours = true,
    },
    [2] = {
        .window_margin = 40.0f,
        .window_y = 1.0f,
        .title_spacing = 3.0f,
        .min_width = 290.0f,
        .row_spacing = 25.0f,
        .use_full_hours = true,
    },
};

static const char *M_FormatRecordTime(const int32_t total_frames)
{
    const int32_t total_seconds = total_frames / LOGIC_FPS;
    const int32_t minutes = (total_seconds / 60) % 60;
    const int32_t seconds = total_seconds % 60;
    const int32_t centiseconds = total_frames % LOGIC_FPS / (LOGIC_FPS / 10);
    return String_FormatStatic(
        "%02d:%02d.%-2d", minutes, seconds, centiseconds);
}

static const char *M_FormatTime(
    const UI_STATS_DIALOG_STATE *const s, const int32_t total_frames)
{
    const int32_t total_seconds = total_frames / LOGIC_FPS;
    const int32_t hours = total_seconds / 3600;
    const int32_t minutes = (total_seconds / 60) % 60;
    const int32_t seconds = total_seconds % 60;
    if (s->look->use_full_hours) {
        return String_FormatStatic("%02d:%02d:%02d", hours, minutes, seconds);
    } else if (hours != 0) {
        return String_FormatStatic("%d:%02d:%02d", hours, minutes, seconds);
    } else {
        return String_FormatStatic("%d:%02d", minutes, seconds);
    }
}

static const char *M_FormatDistance(int32_t distance)
{
    distance /= 445;
    if (distance < 1000) {
        return String_FormatStatic("%dm", distance);
    } else {
        return String_FormatStatic(
            "%d.%02dkm", distance / 1000, (distance % 1000) / 10);
    }
}

static void M_AdjustMaxKills(
    UI_STATS_DIALOG_STATE *const s, const bool include_allies)
{
    if (s->max_stats == nullptr) {
        return;
    }
    s->adjusted_max_stats = *s->max_stats;
    s->adjusted_max_stats.max_kill_count =
        s->adjusted_max_stats.max_kill_non_ally_count;
    if (include_allies) {
        s->adjusted_max_stats.max_kill_count +=
            s->adjusted_max_stats.max_kill_ally_count;
    }
    s->max_stats = &s->adjusted_max_stats;
}

static bool M_HasHurtAlliesSoFar(const int32_t level_num)
{
    const GF_LEVEL_TABLE *const level_table = GF_GetLevelTable(GFLT_MAIN);
    if (level_table == nullptr || level_num <= 0) {
        return false;
    }
    for (int32_t i = 0; i <= level_num && i < level_table->count; i++) {
        const GF_LEVEL *const level = &level_table->levels[i];
        const RESUME_INFO *const resume = Savegame_GetCurrentInfo(level);
        if (resume != nullptr && resume->flags.available
            && resume->hurt_allies) {
            return true;
        }
    }
    return false;
}

static bool M_HasHurtAlliesEver(const bool include_bonus_levels)
{
    const GF_LEVEL_TABLE *const level_table = GF_GetLevelTable(GFLT_MAIN);
    if (level_table == nullptr) {
        return false;
    }
    for (int32_t i = 0; i < level_table->count; i++) {
        const GF_LEVEL *const level = &level_table->levels[i];
        if (!(level->type == GFL_NORMAL
              || (level->type == GFL_BONUS && include_bonus_levels))) {
            continue;
        }
        const RESUME_INFO *const resume = Savegame_GetCurrentInfo(level);
        if (resume != nullptr && resume->hurt_allies) {
            return true;
        }
    }
    return false;
}

static void M_FormatIconSecrets(
    char *const out, const LEVEL_STATS *const level_stats)
{
    char *ptr = out;
    int32_t num_secrets = 0;
    for (int32_t i = 0; i < STATS_MAX_SECRETS; i++) {
        if (!Stats_IsSecretValid(i)) {
            continue;
        }

        const bool has_secret = Stats_HasSecret(i);
        if (!has_secret && out == ptr) {
            // Do not reserve space pointlessly.
            // Good: [secret][ ][ ]
            // Bad:  [ ][ ][secret] – should be just [secret]
            continue;
        }
        const OBJECT_ID obj_id = Stats_GetSecretObject(i);
        if (obj_id != NO_OBJECT) {
            ptr += sprintf(
                ptr, has_secret ? "\\{secret %d}" : "\\{i}\\{secret %d}\\{/i}",
                obj_id + 1 - O_SECRET_1);
        }
        if (has_secret) {
            num_secrets++;
        }
    }
    *ptr++ = '\0';

    if (num_secrets == 0) {
        strcpy(out, GS("general/stats/none"));
    }
}

static void M_RowCentered(
    const UI_STATS_DIALOG_STATE *const s, const char *const text)
{
    UI_BeginStackEx((UI_STACK_SETTINGS) {
        .orientation = UI_STACK_VERTICAL,
        .align = { .h = UI_STACK_H_ALIGN_CENTER },
    });
    UI_Label(text);
    UI_EndStack();
}

static void M_Row(
    const UI_STATS_DIALOG_STATE *const s, const char *const key,
    const char *const value)
{
    if (s->args.style == UI_STATS_DIALOG_STYLE_BARE) {
        UI_BeginStack(UI_STACK_HORIZONTAL);
        UI_Label(key);
        UI_Label(" ");
        UI_Label(value);
        UI_EndStack();
    } else {
        UI_BeginStackEx((UI_STACK_SETTINGS) {
            .orientation = UI_STACK_HORIZONTAL,
            .spacing = { .h = s->look->row_spacing },
            .align = { .h = UI_STACK_H_ALIGN_DISTRIBUTE },
        });
        UI_Label(key);
        UI_Label(value);
        UI_EndStack();
    }
}

static void M_RowFromRole(
    const UI_STATS_DIALOG_STATE *const s, const M_ROW_ROLE role,
    const int32_t param)
{
    const char *const num_fmt =
        g_Config.ui.stat_detail_mode == STAT_DETAIL_MODE_MINIMAL
        ? GS("general/stats/basic_fmt")
        : GS("general/stats/detail_fmt");

    switch (role) {
    case M_ROW_LEVEL_COUNTER:
        M_Row(
            s, GS("general/stats/level"),
            String_FormatStatic(
                GS("general/stats/detail_fmt"),
                GF_GetLevelOrdinalNumber(
                    GFLT_MAIN, GF_GetLevel(GFLT_MAIN, s->args.level_num)),
                GF_GetLevelCount(GFLT_MAIN)));
        break;

    case M_ROW_TIMER:
        M_Row(
            s, GS("general/stats/time_taken"),
            M_FormatTime(s, s->stats->timer));
        break;

    case M_ROW_ICON_SECRETS: {
        if (s->has_floordata_secrets) {
            M_RowFromRole(s, M_ROW_NUM_SECRETS, 0);
            break;
        }
        char buf[256];
        M_FormatIconSecrets(buf, (LEVEL_STATS *)s->stats);
        M_Row(s, GS("general/stats/secrets"), buf);
        break;
    }

    case M_ROW_NUM_SECRETS:
        M_Row(
            s, GS("general/stats/secrets"),
            String_FormatStatic(
                GS("general/stats/detail_fmt"), s->stats->secret_count,
                s->max_stats->max_secret_count));
        break;

    case M_ROW_PICKUPS:
        M_Row(
            s, GS("general/stats/pickups"),
            String_FormatStatic(
                num_fmt, s->stats->pickup_count,
                s->max_stats->max_pickup_count));
        break;

    case M_ROW_KILLS:
        M_Row(
            s, GS("general/stats/kills"),
            String_FormatStatic(
                num_fmt, s->stats->kill_count, s->max_stats->max_kill_count));
        break;

    case M_ROW_DEATHS:
        M_Row(
            s, GS("general/stats/deaths"),
            String_FormatStatic(
                GS("general/stats/basic_fmt"), s->stats->death_count));
        break;

    case M_ROW_AMMO:
        M_Row(
            s, GS("general/stats/ammo"),
            String_FormatStatic(
                GS("general/misc/pagination_nav"), s->stats->ammo_hits,
                s->stats->ammo_used));
        break;

    case M_ROW_AMMO_USED:
        M_Row(
            s, GS("general/stats/ammo_used"),
            String_FormatStatic("%d", s->stats->ammo_used));
        break;

    case M_ROW_AMMO_HITS:
        M_Row(
            s, GS("general/stats/ammo_hits"),
            String_FormatStatic("%d", s->stats->ammo_hits));
        break;

    case M_ROW_MEDIPACKS_USED:
        M_Row(
            s, GS("general/stats/medipacks_used"),
            String_FormatStatic("%.1f", s->stats->medipacks_used));
        break;

    case M_ROW_DISTANCE_TRAVELLED:
        M_Row(
            s, GS("general/stats/distance_travelled"),
            M_FormatDistance(s->stats->distance_travelled));
        break;

    case M_ROW_ASSAULT_COURSE_TITLE:
        M_RowCentered(s, GS("general/stats/gym_assault_course"));
        break;

    case M_ROW_ASSAULT_COURSE_ROW:
    case M_ROW_RACETRACK_ROW: {
        const GYM_TRACK_TYPE track_type = role == M_ROW_ASSAULT_COURSE_ROW
            ? GYM_TRACK_ASSAULT
            : GYM_TRACK_QUAD;
        const GYM_TRACK_ENTRY *const entry =
            &s->assault_stats[track_type]->entries[param];
        const char *const attempt_str = String_FormatStatic(
            "%2d: %s %d", param + 1, GS("general/stats/assault_finish"),
            entry->attempt_num);
        const char *const time_str = String_FormatStatic(
            param == 0 ? GS("general/stats/assault_best_time_fmt")
                       : GS("general/stats/assault_other_times_fmt"),
            M_FormatRecordTime(entry->time));
        if (g_TRVersion == 3) {
            M_RowCentered(s, time_str);
        } else {
            M_Row(s, attempt_str, time_str);
        }
        break;
    }

    case M_ROW_ASSAULT_NO_TIMES_SET:
        M_RowCentered(s, GS("general/stats/assault_no_times_set"));
        break;

    case M_ROW_RACETRACK_TITLE:
        M_RowCentered(s, GS("general/stats/gym_racetrack_course"));
        break;

    case M_ROW_SPACER:
        M_RowCentered(s, " ");
        break;

    default:
        break;
    }
}

static void M_CommonRows(const UI_STATS_DIALOG_STATE *const s)
{
    if (g_TRVersion == 1) {
        if (g_Config.ui.stat_detail_mode == STAT_DETAIL_MODE_MINIMAL) {
            M_RowFromRole(s, M_ROW_KILLS, 0);
            M_RowFromRole(s, M_ROW_PICKUPS, 0);
            M_RowFromRole(s, M_ROW_NUM_SECRETS, 0);
            M_RowFromRole(s, M_ROW_TIMER, 0);
        } else {
            M_RowFromRole(s, M_ROW_TIMER, 0);
            M_RowFromRole(s, M_ROW_NUM_SECRETS, 0);
            M_RowFromRole(s, M_ROW_PICKUPS, 0);
            M_RowFromRole(s, M_ROW_KILLS, 0);
            if (g_Config.ui.stat_detail_mode == STAT_DETAIL_MODE_FULL) {
                M_RowFromRole(s, M_ROW_AMMO, 0);
                M_RowFromRole(s, M_ROW_MEDIPACKS_USED, 0);
                M_RowFromRole(s, M_ROW_DISTANCE_TRAVELLED, 0);
            }
        }
    } else {
        if (g_Config.ui.stat_detail_mode == STAT_DETAIL_MODE_FULL) {
            M_RowFromRole(s, M_ROW_PICKUPS, 0);
        }
        M_RowFromRole(s, M_ROW_KILLS, 0);
        if (g_Config.ui.stat_detail_mode == STAT_DETAIL_MODE_FULL) {
            M_RowFromRole(s, M_ROW_AMMO, 0);
        } else {
            M_RowFromRole(s, M_ROW_AMMO_USED, 0);
            M_RowFromRole(s, M_ROW_AMMO_HITS, 0);
        }
        M_RowFromRole(s, M_ROW_MEDIPACKS_USED, 0);
        M_RowFromRole(s, M_ROW_DISTANCE_TRAVELLED, 0);
    }

    if (g_Config.gameplay.enable_deaths_counter && s->stats->death_count >= 0) {
        // Always use sum of all levels for the deaths.
        // Deaths get stored in the resume info for the level they happen
        // on, so if the player dies in Vilcabamba and reloads Caves, they
        // should still see an incremented death counter.
        M_RowFromRole(s, M_ROW_DEATHS, 0);
    }
}

static void M_LevelStatsRows(const UI_STATS_DIALOG_STATE *const s)
{
    if (g_Config.ui.enable_stats_level_header) {
        M_RowFromRole(s, M_ROW_LEVEL_COUNTER, 0);
    }

    if (g_TRVersion == 1) {
        M_CommonRows(s);
    } else {
        M_RowFromRole(s, M_ROW_TIMER, 0);
        if (s->max_stats->max_secret_count != 0) {
            M_RowFromRole(s, M_ROW_ICON_SECRETS, 0);
        }
        M_CommonRows(s);
    }
}

static void M_FinalStatsRows(const UI_STATS_DIALOG_STATE *const s)
{
    if (g_TRVersion == 1) {
        M_CommonRows(s);
    } else {
        M_RowFromRole(s, M_ROW_TIMER, 0);
        if (s->max_stats->max_secret_count != 0) {
            M_RowFromRole(s, M_ROW_NUM_SECRETS, 0);
        }
        M_CommonRows(s);
    }
}

static const char *M_GetDialogTitle(const UI_STATS_DIALOG_STATE *const s)
{
    switch (s->args.mode) {
    case UI_STATS_DIALOG_MODE_LEVEL:
        return GF_GetLevel(GFLT_MAIN, s->args.level_num)->title;
    case UI_STATS_DIALOG_MODE_FINAL: {
        const GF_LEVEL_TYPE level_type =
            GF_GetLevel(GFLT_MAIN, s->args.level_num)->type;
        const char *const title = level_type == GFL_BONUS
            ? GS("general/stats/bonus_statistics")
            : GS("general/stats/final_statistics");
        return title;
    }
    case UI_STATS_DIALOG_MODE_ASSAULT_COURSE:
        return GS("general/stats/assault_title");
    }
    return nullptr;
}

static void M_AssaultCourseStatsRows(UI_STATS_DIALOG_STATE *const s)
{
    const int32_t record_limit = g_TRVersion >= 3 ? 3 : MAX_ASSAULT_TIMES;
    const bool has_race_track = Gym_TrackManager_HasStats(GYM_TRACK_QUAD);
    int32_t count = 0;

#define L_EMIT_ROW(...)                                                        \
    M_RowFromRole(__VA_ARGS__);                                                \
    count++;

    if (has_race_track) {
        L_EMIT_ROW(s, M_ROW_ASSAULT_COURSE_TITLE, 0);
    }

    if (s->assault_stats[GYM_TRACK_ASSAULT]->entries[0].time == 0) {
        L_EMIT_ROW(s, M_ROW_ASSAULT_NO_TIMES_SET, 0);
    } else {
        for (int32_t i = 0; i < record_limit; i++) {
            if (s->assault_stats[GYM_TRACK_ASSAULT]->entries[i].time == 0) {
                break;
            }
            L_EMIT_ROW(s, M_ROW_ASSAULT_COURSE_ROW, i);
        }
    }

    if (has_race_track) {
        L_EMIT_ROW(s, M_ROW_SPACER, 0);
        L_EMIT_ROW(s, M_ROW_RACETRACK_TITLE, 0);
        if (s->assault_stats[GYM_TRACK_QUAD]->entries[0].time == 0) {
            L_EMIT_ROW(s, M_ROW_ASSAULT_NO_TIMES_SET, 0);
        } else {
            for (int32_t i = 0; i < record_limit; i++) {
                if (s->assault_stats[GYM_TRACK_QUAD]->entries[i].time == 0) {
                    break;
                }
                L_EMIT_ROW(s, M_ROW_RACETRACK_ROW, i);
            }
        }
    }

#undef L_EMIT_ROW

    while (count < M_MIN_ASSAULT_COURSE_ROWS) {
        M_RowFromRole(s, M_ROW_SPACER, 0);
        count++;
    }
}

static int32_t M_GetAssaultCourseRowCount(const UI_STATS_DIALOG_STATE *const s)
{
    const int32_t record_limit = g_TRVersion >= 3 ? 3 : MAX_ASSAULT_TIMES;
    const bool has_race_track = Gym_TrackManager_HasStats(GYM_TRACK_QUAD);
    int32_t count = 0;

    if (has_race_track) {
        count++;
    }

    if (s->assault_stats[GYM_TRACK_ASSAULT]->entries[0].time == 0) {
        count++;
    } else {
        for (int32_t i = 0; i < record_limit; i++) {
            if (s->assault_stats[GYM_TRACK_ASSAULT]->entries[i].time == 0) {
                break;
            }
            count++;
        }
    }

    if (has_race_track) {
        count += 2;
        if (s->assault_stats[GYM_TRACK_QUAD]->entries[0].time == 0) {
            count++;
        } else {
            for (int32_t i = 0; i < record_limit; i++) {
                if (s->assault_stats[GYM_TRACK_QUAD]->entries[i].time == 0) {
                    break;
                }
                count++;
            }
        }
    }

    return MAX(count, M_MIN_ASSAULT_COURSE_ROWS);
}

static UI_WINDOW_SETTINGS M_GetWindowSettings(
    const UI_STATS_DIALOG_STATE *const s)
{
    const bool is_assault_mode =
        s->args.mode == UI_STATS_DIALOG_MODE_ASSAULT_COURSE;
    return (UI_WINDOW_SETTINGS) {
        .title = M_GetDialogTitle(s),
        .scrollable = is_assault_mode ? &s->scrollable : nullptr,
        .title_spacing = s->look->title_spacing,
    };
}

static void M_BeginDialog(const UI_STATS_DIALOG_STATE *const s)
{
    if (s->args.style == UI_STATS_DIALOG_STYLE_BARE) {
        UI_BeginStackEx((UI_STACK_SETTINGS) {
            .orientation = UI_STACK_VERTICAL,
            .spacing = { .v = 11.0f },
            .align = { .h = UI_STACK_H_ALIGN_CENTER },
        });
        const char *const title = M_GetDialogTitle(s);
        if (title != nullptr) {
            UI_Label(title);
        }
    } else {
        UI_BeginWindow(M_GetWindowSettings(s));
    }
    // ensure minimum dialog width
    UI_Spacer(s->look->min_width, 0.0f);
}

static void M_EndDialog(const UI_STATS_DIALOG_STATE *const s)
{
    if (s->args.style == UI_STATS_DIALOG_STYLE_BARE) {
        UI_EndStack();
    } else {
        UI_EndWindow();
    }
}

UI_STATS_DIALOG_STATE *UI_StatsDialog_Init(const UI_STATS_DIALOG_ARGS args)
{
    UI_STATS_DIALOG_STATE *const s = Memory_Alloc(sizeof(*s));

    s->has_floordata_secrets = false;
    s->scrollable.vis_items = M_MIN_ASSAULT_COURSE_ROWS;
    s->args = args;
    s->look = &m_Looks[g_TRVersion - 1];

    switch (args.mode) {
    case UI_STATS_DIALOG_MODE_LEVEL:
        const GF_LEVEL *const current_level =
            GF_GetLevel(GFLT_MAIN, s->args.level_num);
        const RESUME_INFO *const current_info =
            Savegame_GetCurrentInfo(current_level);
        s->stats = (const STATS_COMMON *)&current_info->stats;
        s->max_stats = Stats_GetLevelMaxStats(current_level);
        const bool include_allies = M_HasHurtAlliesSoFar(s->args.level_num);
        M_AdjustMaxKills(s, include_allies);

        const GF_LEVEL *const level = Game_GetCurrentLevel();
        for (int32_t i = 0; i < STATS_MAX_SECRETS; i++) {
            if (s->max_stats->secret_objects[i].taken
                && s->max_stats->secret_objects[i].assigned_object_id
                    == NO_OBJECT) {
                s->has_floordata_secrets = true;
            }
        }
        break;

    case UI_STATS_DIALOG_MODE_FINAL:
        const GF_LEVEL_TYPE level_type =
            GF_GetLevel(GFLT_MAIN, s->args.level_num)->type;
        const bool include_bonus_levels = level_type == GFL_BONUS;
        s->final_stats = Stats_ComputeFinalStats(include_bonus_levels);
        s->stats = &s->final_stats.stats;
        s->max_stats = &s->final_stats.max_stats;
        M_AdjustMaxKills(s, M_HasHurtAlliesEver(include_bonus_levels));
        break;

    case UI_STATS_DIALOG_MODE_ASSAULT_COURSE:
        for (int32_t track_type = 0; track_type < GYM_TRACK_NUMBER_OF;
             track_type++) {
            s->assault_stats[track_type] =
                Gym_TrackManager_GetStats(track_type);
        }
        s->scrollable.max_items = M_GetAssaultCourseRowCount(s);
        break;
    }

    return s;
}

void UI_StatsDialog_Free(UI_STATS_DIALOG_STATE *const s)
{
    Memory_Free(s);
}

int32_t UI_StatsDialog_Control(UI_STATS_DIALOG_STATE *const s)
{
    return UI_ScrollableStack_Control(&s->scrollable, UI_STACK_VERTICAL);
}

void UI_StatsDialog(UI_STATS_DIALOG_STATE *const s)
{
    UI_BeginModal(0.5f, s->look->window_y);
    UI_BeginPad(s->look->window_margin, s->look->window_margin);

    M_BeginDialog(s);
    switch (s->args.mode) {
    case UI_STATS_DIALOG_MODE_LEVEL:
        M_LevelStatsRows(s);
        break;

    case UI_STATS_DIALOG_MODE_FINAL:
        M_FinalStatsRows(s);
        break;

    case UI_STATS_DIALOG_MODE_ASSAULT_COURSE:
        // Ensure minimum size even if there are no items
        UI_Spacer(290.0f, 0.0f);
        UI_BeginScrollableStack(
            &s->scrollable,
            (UI_SCROLLABLE_STACK_SETTINGS) {
                .orientation = UI_STACK_VERTICAL,
                .spacing = 3.0f,
            });
        M_AssaultCourseStatsRows(s);
        UI_EndScrollableStack();
        break;
    }

    M_EndDialog(s);

    UI_EndPad();
    UI_EndModal();
}
