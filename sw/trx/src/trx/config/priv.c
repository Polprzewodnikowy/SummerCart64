#include <trx/config/priv.h>

#include <trx/config/common.h>
#include <trx/config/file.h>
#include <trx/config/vars.h>
#include <trx/core/log.h>
#include <trx/core/memory.h>
#include <trx/core/strings.h>
#include <trx/core/utils.h>
#include <trx/debug.h>
#include <trx/game/clock.h>
#include <trx/game/input.h>
#include <trx/game/lara/const.h>
#include <trx/version.h>

#include <stdio.h>
#include <string.h>

#define M_CONFIG_VERSION_CURRENT 1

static void M_LoadKeyboardLayout(
    JSON_OBJECT *const parent_obj, const INPUT_LAYOUT layout)
{
    char layout_name[20];
    sprintf(layout_name, "layout_%d", layout);
    JSON_ARRAY *const arr = JSON_ObjectGetArray(parent_obj, layout_name);
    if (arr == nullptr) {
        return;
    }

    for (size_t i = 0; i < arr->length; i++) {
        JSON_OBJECT *const bind_obj = JSON_ArrayGetObject(arr, i);
        if (bind_obj == nullptr) {
            // this can happen on TR1X <= 3.1.1, which is no longer supported
            LOG_WARNING("unsupported keyboard layout config");
            continue;
        }

        Input_AssignFromJSONObject(INPUT_BACKEND_KEYBOARD, layout, bind_obj);
    }
}

static void M_LoadControllerLayout(
    JSON_OBJECT *const parent_obj, const INPUT_LAYOUT layout)
{
    char layout_name[20];
    sprintf(layout_name, "cntlr_layout_%d", layout);
    JSON_ARRAY *const arr = JSON_ObjectGetArray(parent_obj, layout_name);
    if (arr == nullptr) {
        return;
    }

    for (size_t i = 0; i < arr->length; i++) {
        JSON_OBJECT *const bind_obj = JSON_ArrayGetObject(arr, i);
        if (bind_obj == nullptr) {
            // this can happen on TR1X <= 3.1.1, which is no longer supported
            LOG_WARNING("unsupported controller layout config");
            continue;
        }

        Input_AssignFromJSONObject(INPUT_BACKEND_CONTROLLER, layout, bind_obj);
    }
}

static void M_LoadLegacyInputConfig(JSON_OBJECT *const root_obj)
{
    for (INPUT_LAYOUT layout = INPUT_LAYOUT_CUSTOM_1;
         layout < INPUT_LAYOUT_NUMBER_OF; layout++) {
        M_LoadKeyboardLayout(root_obj, layout);
        M_LoadControllerLayout(root_obj, layout);
    }
}

static void M_LoadInputLayout(
    JSON_OBJECT *const parent_obj, const INPUT_BACKEND backend,
    const INPUT_LAYOUT layout)
{
    char layout_name[20];
    sprintf(layout_name, "layout_%d", layout);
    JSON_ARRAY *const arr = JSON_ObjectGetArray(parent_obj, layout_name);
    if (arr == nullptr) {
        return;
    }

    for (size_t i = 0; i < arr->length; i++) {
        JSON_OBJECT *const bind_obj = JSON_ArrayGetObject(arr, i);
        ASSERT(bind_obj != nullptr);
        Input_AssignFromJSONObject(backend, layout, bind_obj);
    }
}

static void M_LoadInputConfig(JSON_OBJECT *const root_obj)
{
    JSON_OBJECT *const input_obj = JSON_ObjectGetObject(root_obj, "input");
    if (input_obj == nullptr) {
        return;
    }

    JSON_OBJECT *const keyboard_obj =
        JSON_ObjectGetObject(input_obj, "keyboard");
    JSON_OBJECT *const controller_obj =
        JSON_ObjectGetObject(input_obj, "controller");
    for (INPUT_LAYOUT layout = INPUT_LAYOUT_CUSTOM_1;
         layout < INPUT_LAYOUT_NUMBER_OF; layout++) {
        if (keyboard_obj != nullptr) {
            M_LoadInputLayout(keyboard_obj, INPUT_BACKEND_KEYBOARD, layout);
        }
        if (controller_obj != nullptr) {
            M_LoadInputLayout(controller_obj, INPUT_BACKEND_CONTROLLER, layout);
        }
    }
}

static void M_DumpInputLayout(
    JSON_OBJECT *const parent_obj, const INPUT_BACKEND backend,
    const INPUT_LAYOUT layout)
{
    JSON_ARRAY *const arr = JSON_ArrayNew();

    bool has_elements = false;
    for (INPUT_ROLE role = 0; role < INPUT_ROLE_NUMBER_OF; role++) {
        JSON_OBJECT *const bind_obj = JSON_ObjectNew();
        if (Input_AssignToJSONObject(backend, layout, bind_obj, role)) {
            has_elements = true;
            JSON_ArrayAppendObject(arr, bind_obj);
        } else {
            JSON_ObjectFree(bind_obj);
        }
    }

    if (has_elements) {
        char layout_name[20];
        sprintf(layout_name, "layout_%d", layout);
        JSON_ObjectAppendArray(parent_obj, layout_name, arr);
    } else {
        JSON_ArrayFree(arr);
    }
}

static void M_DumpInputConfig(JSON_OBJECT *const root_obj)
{
    JSON_OBJECT *const input_obj = JSON_ObjectNew();
    JSON_OBJECT *const keyboard_obj = JSON_ObjectNew();
    JSON_OBJECT *const controller_obj = JSON_ObjectNew();
    JSON_ObjectAppendObject(root_obj, "input", input_obj);
    JSON_ObjectAppendObject(input_obj, "keyboard", keyboard_obj);
    JSON_ObjectAppendObject(input_obj, "controller", controller_obj);
    for (INPUT_LAYOUT layout = INPUT_LAYOUT_CUSTOM_1;
         layout < INPUT_LAYOUT_NUMBER_OF; layout++) {
        M_DumpInputLayout(keyboard_obj, INPUT_BACKEND_KEYBOARD, layout);
        M_DumpInputLayout(controller_obj, INPUT_BACKEND_CONTROLLER, layout);
    }
}

static void M_MigrateBarColorName(char **const value_ptr)
{
    const char *value = *value_ptr;
    const char *new_value = nullptr;
    if (value == nullptr) {
        return;
    }
    if (String_Equivalent(value, "gold")) {
        new_value = "brown";
    } else if (String_Equivalent(value, "green")) {
        new_value = "teal";
    } else if (String_Equivalent(value, "gold2")) {
        new_value = "yellow";
    } else if (String_Equivalent(value, "blue2")) {
        new_value = "cyan";
    } else if (String_Equivalent(value, "green2")) {
        new_value = "green";
    } else if (String_Equivalent(value, "gold-green")) {
        new_value = "yellow-green";
    }
    if (new_value != nullptr) {
        Memory_FreePointer(value_ptr);
        *value_ptr = Memory_DupStr(new_value);
    }
}

static void M_LoadLegacyOptions(JSON_OBJECT *const parent_obj)
{
#define L_READ_BOOL(target, key)                                               \
    target = JSON_ObjectGetBool(parent_obj, key, target)
#define L_READ_INT(target, key)                                                \
    target = JSON_ObjectGetInt(parent_obj, key, target)

    // TR1X 2.16..4.5.1 load_current_music
    {
        const JSON_VALUE *const value =
            JSON_ObjectGetValue(parent_obj, "load_current_music");
        if (JSON_ValueIsTrue(value)) {
            g_Config.audio.music_load_condition =
                MUSIC_LOAD_CONDITION_NON_AMBIENT;
        } else if (JSON_ValueIsFalse(value)) {
            g_Config.audio.music_load_condition = MUSIC_LOAD_CONDITION_NEVER;
        }
    }

    // Legacy bool loading screens option.
    {
        const JSON_VALUE *const value =
            JSON_ObjectGetValue(parent_obj, "enable_loading_screens");
        if (JSON_ValueIsTrue(value)) {
            g_Config.gameplay.loading_screens = LOADING_SCREENS_ALWAYS;
        } else if (JSON_ValueIsFalse(value)) {
            g_Config.gameplay.loading_screens = LOADING_SCREENS_DISABLED;
        }
    }

    // TR1X ..4.7
    L_READ_BOOL(g_Config.window.is_fullscreen, "enable_fullscreen");
    L_READ_BOOL(g_Config.window.is_maximized, "enable_maximized");
    L_READ_BOOL(g_Config.gameplay.enable_walk_to_items, "walk_to_items");
    L_READ_BOOL(
        g_Config.gameplay.enable_inverted_look, "enabled_inverted_look");
    L_READ_INT(g_Config.window.x, "window_x");
    L_READ_INT(g_Config.window.y, "window_y");
    L_READ_INT(g_Config.window.width, "window_width");
    L_READ_INT(g_Config.window.height, "window_height");
    L_READ_INT(g_Config.input.keyboard_layout, "layout");
    L_READ_INT(g_Config.input.controller_layout, "cntlr_layout");

    // TR1X ..4.9
    L_READ_BOOL(g_Config.gameplay.enable_cutscenes, "enable_cine");
    L_READ_BOOL(g_Config.gameplay.enable_legal, "enable_eidos_logo");

    // TR1X ..4.11, TR2X ..1.1: 0…10 scale volumes to 0.0f…1.0f
    {
        const JSON_VALUE *const value =
            JSON_ObjectGetValue(parent_obj, "sound_volume");
        const JSON_NUMBER *const num =
            value != nullptr ? JSON_ValueGetNumber(value) : nullptr;
        if (num != nullptr && strchr(num->number, '.') == nullptr) {
            g_Config.audio.sound_volume = JSON_ValueGetInt(value, 0) / 10.0f;
        }
    }
    {
        const JSON_VALUE *const value =
            JSON_ObjectGetValue(parent_obj, "music_volume");
        const JSON_NUMBER *const num =
            value != nullptr ? JSON_ValueGetNumber(value) : nullptr;
        if (value != nullptr && value->type == JSON_TYPE_NUMBER
            && strchr(num->number, '.') == nullptr) {
            g_Config.audio.music_volume = JSON_ValueGetInt(value, 0) / 10.0f;
        }
    }

    // TR1X ..4.11: convert wall bug fix
    {
        const JSON_VALUE *const value =
            JSON_ObjectGetValue(parent_obj, "fix_wall_jump_glitch");
        if (JSON_ValueIsTrue(value)) {
            g_Config.gameplay.wall_glitch_mode = WALL_GLITCH_FIXED;
        }
    }

    // TR1X ..4.13: convert enhanced look to enum type
    {
        const JSON_VALUE *const value =
            JSON_ObjectGetValue(parent_obj, "enable_enhanced_look");
        if (JSON_ValueIsTrue(value)) {
            g_Config.gameplay.look_mode = LOOK_MODE_UNRESTRICTED;
        } else if (JSON_ValueIsFalse(value)) {
            g_Config.gameplay.look_mode = LOOK_MODE_RESTRICTED;
        }
    }

    // TR1X ..4.15, TR2X 1.5
    if (JSON_ObjectGetValue(parent_obj, "ambient_volume") == nullptr) {
        g_Config.audio.ambient_volume = g_Config.audio.music_volume;
    }
    if (JSON_ObjectGetValue(parent_obj, "cutscene_volume") == nullptr) {
        g_Config.audio.cutscene_volume = g_Config.audio.music_volume;
    }
    if (JSON_ObjectGetValue(parent_obj, "fmv_volume") == nullptr) {
        g_Config.audio.fmv_volume = g_Config.audio.music_volume;
    }

    // TR2X 1.6
    L_READ_BOOL(g_Config.audio.enable_ps1_sfx, "enable_barefoot_sfx");
    // TR1X ..4.16
    L_READ_BOOL(g_Config.audio.enable_ps1_sfx, "enable_ps_uzi_sfx");
    L_READ_BOOL(
        g_Config.audio.fix_chainblock_secret_sound, "fix_tihocan_secret_sound");
    L_READ_BOOL(g_Config.input.enable_buffering_inventory, "enable_buffering");
    L_READ_BOOL(g_Config.input.enable_buffering_func_keys, "enable_buffering");

    // TR1X ..4.16, TR2X ..1.6
    {
        const JSON_VALUE *const value =
            JSON_ObjectGetValue(parent_obj, "revert_to_pistols");
        if (JSON_ValueIsTrue(value)) {
            g_Config.gameplay.remember_gun_status = false;
        } else if (JSON_ValueIsFalse(value)) {
            g_Config.gameplay.remember_gun_status = true;
        }
    }

    // TR2X ..1.5.1
    {
        const JSON_VALUE *const value =
            JSON_ObjectGetValue(parent_obj, "fix_pickup_drift_glitch");
        if (JSON_ValueIsFalse(value)) {
            g_Config.gameplay.fix_lara_pickup_embed = false;
        }
    }

    // TRX ..1.2: split brightness into game and UI fields.
    {
        const JSON_VALUE *const old_value =
            JSON_ObjectGetValue(parent_obj, "brightness");
        if (old_value != nullptr) {
            const float old_brightness = JSON_ValueGetDouble(
                old_value, g_Config.visuals.game_brightness);
            if (JSON_ObjectGetValue(parent_obj, "game_brightness") == nullptr) {
                g_Config.visuals.game_brightness = old_brightness;
            }
            if (JSON_ObjectGetValue(parent_obj, "ui_brightness") == nullptr) {
                g_Config.visuals.ui_brightness = old_brightness;
            }
        }
    }

    // TRX legacy: "round shadows" boolean to enum shadow type.
    if (JSON_ObjectGetValue(parent_obj, "shadow_type") == nullptr) {
        const JSON_VALUE *const value =
            JSON_ObjectGetValue(parent_obj, "enable_round_shadow");
        if (JSON_ValueIsTrue(value)) {
            g_Config.visuals.shadow_type = SHADOW_TYPE_CIRCLE;
        } else if (JSON_ValueIsFalse(value)) {
            g_Config.visuals.shadow_type = SHADOW_TYPE_OCTAGON;
        }
    }

    // Pre-1.2 UI bar look and color migration; one-shot via config version.
    if (g_Config.config_version >= 0 && g_Config.config_version < 1) {
        {
            const char *const value =
                JSON_ObjectGetString(parent_obj, "bar_look", "");
            const char *new_value = nullptr;
            if (String_Equivalent(value, "tr1")) {
                new_value = "tr1_pc";
            } else if (String_Equivalent(value, "tr2")) {
                new_value = "tr2_pc";
            } else if (String_Equivalent(value, "tr3")) {
                new_value = "tr3_pc";
            } else if (String_Equivalent(value, "ps1")) {
                new_value = "tr2_ps1";
            }
            if (new_value != nullptr) {
                Memory_FreePointer(&g_Config.ui.bar_look);
                g_Config.ui.bar_look = Memory_DupStr(new_value);
            }
        }

        M_MigrateBarColorName(&g_Config.ui.lara_health_bar.color);
        M_MigrateBarColorName(&g_Config.ui.lara_health_bar.color_ps1);
        M_MigrateBarColorName(&g_Config.ui.lara_health_bar.poison_color);
        M_MigrateBarColorName(&g_Config.ui.lara_health_bar.poison_color_ps1);
        M_MigrateBarColorName(&g_Config.ui.lara_air_bar.color);
        M_MigrateBarColorName(&g_Config.ui.lara_air_bar.color_ps1);
        M_MigrateBarColorName(&g_Config.ui.lara_sprint_bar.color);
        M_MigrateBarColorName(&g_Config.ui.lara_sprint_bar.color_ps1);
        M_MigrateBarColorName(&g_Config.ui.lara_exposure_bar.color);
        M_MigrateBarColorName(&g_Config.ui.lara_exposure_bar.color_ps1);
        M_MigrateBarColorName(&g_Config.ui.enemy_health_bar.color);
        M_MigrateBarColorName(&g_Config.ui.enemy_health_bar.color_ps1);
        M_MigrateBarColorName(&g_Config.ui.enemy_health_bar.color_allies);
        M_MigrateBarColorName(&g_Config.ui.enemy_health_bar.color_allies_ps1);
    }

    // Pre 1.3 Fixed glide camera option was only visible to TR2 and defaulted
    // to false in TR1. Now a regular toggle for all camera modes.
    if (g_TRVersion == 2) {
        L_READ_BOOL(g_Config.visuals.enable_glide_cameras, "fix_glide_cameras");
    }

    // TRX ..1.4: sunglasses on/off changed to mode.
    if (JSON_ObjectGetValue(parent_obj, "sunglasses_mode") == nullptr) {
        const JSON_VALUE *const value =
            JSON_ObjectGetValue(parent_obj, "enable_sunglasses");
        g_Config.visuals.sunglasses_mode = JSON_ValueIsTrue(value)
            ? SUNGLASSES_MODE_OPAQUE
            : SUNGLASSES_MODE_OFF;
    }

    if (g_Config.config_version >= 0
        && g_Config.config_version < M_CONFIG_VERSION_CURRENT) {
        g_Config.config_version = M_CONFIG_VERSION_CURRENT;
    }

#undef L_READ_BOOL
#undef L_READ_INT
}

void Config_LoadFromJSON(JSON_OBJECT *root_obj)
{
    ConfigFile_LoadOptions(root_obj, Config_GetOptionMap());
    if (Gym_TrackManager_HasStats(GYM_TRACK_ASSAULT)) {
        ConfigFile_LoadGymTrackStats(
            root_obj, "assault_stats", &g_Config.profile.assault_stats);
    }
    if (Gym_TrackManager_HasStats(GYM_TRACK_QUAD)) {
        ConfigFile_LoadGymTrackStats(
            root_obj, "racetrack_stats", &g_Config.profile.racetrack_stats);
    }
    M_LoadLegacyInputConfig(root_obj);
    M_LoadInputConfig(root_obj);
    M_LoadLegacyOptions(root_obj);
}

void Config_DumpToJSON(JSON_OBJECT *root_obj)
{
    ConfigFile_DumpOptions(root_obj, Config_GetOptionMap());
    if (Gym_TrackManager_HasStats(GYM_TRACK_ASSAULT)) {
        ConfigFile_DumpGymTrackStats(
            root_obj, "assault_stats", &g_Config.profile.assault_stats);
    }
    if (Gym_TrackManager_HasStats(GYM_TRACK_QUAD)) {
        ConfigFile_DumpGymTrackStats(
            root_obj, "racetrack_stats", &g_Config.profile.racetrack_stats);
    }
    M_DumpInputConfig(root_obj);
}

void Config_Sanitize(void)
{
    if (g_Config.rendering.aspect_mode != ASPECT_MODE_ANY
        && g_Config.rendering.aspect_mode != ASPECT_MODE_16_9
        && g_Config.rendering.aspect_mode != ASPECT_MODE_16_10) {
        g_Config.rendering.aspect_mode = ASPECT_MODE_4_3;
    }
    CLAMP(g_Config.audio.master_volume, 0.0f, 1.0f);
    CLAMP(g_Config.audio.sound_volume, 0.0f, 1.0f);
    CLAMP(g_Config.audio.music_volume, 0.0f, 1.0f);
    CLAMP(g_Config.audio.inventory_music_volume, 0.0f, 1.0f);
    CLAMP(g_Config.audio.underwater_music_volume, 0.0f, 1.0f);
    CLAMP(g_Config.audio.ambient_volume, 0.0f, 1.0f);
    CLAMP(g_Config.audio.inventory_ambient_volume, 0.0f, 1.0f);
    CLAMP(g_Config.audio.underwater_ambient_volume, 0.0f, 1.0f);
    CLAMP(g_Config.audio.cutscene_volume, 0.0f, 1.0f);
    CLAMP(g_Config.audio.fmv_volume, 0.0f, 1.0f);
    CLAMP(g_Config.input.keyboard_layout, 0, INPUT_LAYOUT_NUMBER_OF - 1);
    CLAMP(g_Config.input.controller_layout, 0, INPUT_LAYOUT_NUMBER_OF - 1);
    CLAMP(
        g_Config.gameplay.turbo_speed, CLOCK_TURBO_SPEED_MIN,
        CLOCK_TURBO_SPEED_MAX);
    CLAMP(g_Config.gameplay.start_lara_hitpoints, 1, LARA_MAX_HITPOINTS);
    CLAMP(g_Config.gameplay.camera_speed, 1, 10);
    CLAMP(g_Config.gameplay.idle_pose_timeout, 0, 1200);
    CLAMP(g_Config.rendering.wireframe_width, 1.0, 100.0);
    CLAMP(g_Config.rendering.upscaling_factor, 1, 8);
    CLAMP(g_Config.rendering.borders, 0.0, 0.45);
    CLAMP(g_Config.ui.bar_scale, 0.5, 2.0);
    CLAMP(g_Config.ui.text_scale, 0.5, 2.0);
    CLAMP(g_Config.ui.pickup_scale, 0.5, 2.0);
    CLAMP(g_Config.visuals.fog_start, 1, 100);
    CLAMP(g_Config.visuals.fog_end, 1, 100);
    CLAMP(g_Config.visuals.fov, 30, 150);
    CLAMPL(g_Config.gameplay.maximum_save_slots, 0);
    CLAMPL(g_Config.gameplay.maximum_quick_save_slots, 0);
    CLAMP(g_Config.visuals.shadow_type, 0, SHADOW_TYPE_NUMBER_OF - 1);
    CLAMP(g_Config.gameplay.loading_screens, 0, LOADING_SCREENS_NEW_GAMES);

    if (g_Config.rendering.fps != 30 && g_Config.rendering.fps != 60) {
        g_Config.rendering.fps = 30;
    }

    CLAMP(
        g_Config.visuals.game_brightness, CONFIG_MIN_BRIGHTNESS,
        CONFIG_MAX_BRIGHTNESS);
    CLAMP(
        g_Config.visuals.ui_brightness, CONFIG_MIN_BRIGHTNESS,
        CONFIG_MAX_BRIGHTNESS);
    CLAMP(g_Config.visuals.gamma, CONFIG_MIN_GAMMA, CONFIG_MAX_GAMMA);
    CLAMPL(g_Config.rendering.anisotropy_filter, 1.0);
}
