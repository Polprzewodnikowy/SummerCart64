#include <trx/game/ui/dialogs/setting_helpers/handlers.h>

#include <trx/config.h>
#include <trx/core/strings.h>
#include <trx/game/game_flow/common.h>
#include <trx/game/gun.h>
#include <trx/game/music.h>
#include <trx/game/objects/common.h>
#include <trx/game/sound.h>
#include <trx/game/ui/dialogs/settings_editor.h>
#include <trx/game/ui/settings.h>
#include <trx/version.h>

bool UI_Settings_EnablePS1Crystals_IsAvailable(
    const UI_SETTINGS_OPTION *const option)
{
    return g_Config.gameplay.enable_save_crystals;
}

bool UI_Settings_EnableFadeEffects_IsAvailable(
    const UI_SETTINGS_OPTION *const option)
{
    return g_Config.visuals.enable_fade_effects;
}

bool UI_Settings_FogColor_IsAvailable(const UI_SETTINGS_OPTION *const option)
{
    return !g_Config.visuals.fog_transparency;
}

bool UI_Settings_EnableBreeze_IsAvailable(
    const UI_SETTINGS_OPTION *const option)
{
    return g_Config.visuals.enable_braid || g_TRVersion == 3;
}

bool UI_Settings_ResponsiveJumping_IsAvailable(
    const UI_SETTINGS_OPTION *const option)
{
    return g_Config.gameplay.enable_tr2_jumping;
}

bool UI_Settings_Crawl_IsAvailable(const UI_SETTINGS_OPTION *const option)
{
    return g_Config.gameplay.enable_crawling;
}

bool UI_Settings_Sprint_IsAvailable(const UI_SETTINGS_OPTION *const option)
{
    return g_Config.gameplay.enable_sprint;
}

bool UI_Settings_Bar_IsAvailable(const UI_SETTINGS_OPTION *const option)
{
    return g_Config.ui.show_bars;
}

bool UI_Settings_Healthbar_IsAvailable(const UI_SETTINGS_OPTION *const option)
{
    return UI_Settings_Bar_IsAvailable(option);
}

bool UI_Settings_Airbar_IsAvailable(const UI_SETTINGS_OPTION *const option)
{
    return UI_Settings_Bar_IsAvailable(option);
}

bool UI_Settings_Sprintbar_IsAvailable(const UI_SETTINGS_OPTION *const option)
{
    return UI_Settings_Sprint_IsAvailable(option)
        && UI_Settings_Bar_IsAvailable(option);
}

bool UI_Settings_Exposurebar_IsAvailable(const UI_SETTINGS_OPTION *const option)
{
    return UI_Settings_Bar_IsAvailable(option);
}

bool UI_Settings_EnemyHealthbar_IsAvailable(
    const UI_SETTINGS_OPTION *const option)
{
    return UI_Settings_Bar_IsAvailable(option)
        && g_Config.ui.enemy_health_bar.show_mode != BAR_SHOW_MODE_NEVER;
}

bool UI_Settings_AllyHealthbar_IsAvailable(
    const UI_SETTINGS_OPTION *const option)
{
    return UI_Settings_Bar_IsAvailable(option)
        && g_Config.ui.enemy_health_bar.show_mode == BAR_SHOW_MODE_ALWAYS
        && g_Config.gameplay.enable_ally_targeting;
}

bool UI_Settings_HealthbarColor_IsAvailable(
    const UI_SETTINGS_OPTION *const option)
{
    return UI_Settings_Healthbar_IsAvailable(option);
}

bool UI_Settings_AirbarColor_IsAvailable(const UI_SETTINGS_OPTION *const option)
{
    return UI_Settings_Airbar_IsAvailable(option);
}

bool UI_Settings_SprintbarColor_IsAvailable(
    const UI_SETTINGS_OPTION *const option)
{
    return UI_Settings_Sprintbar_IsAvailable(option);
}

bool UI_Settings_ExposurebarColor_IsAvailable(
    const UI_SETTINGS_OPTION *const option)
{
    return UI_Settings_Exposurebar_IsAvailable(option);
}

bool UI_Settings_EnemyHealthbarColor_IsAvailable(
    const UI_SETTINGS_OPTION *const option)
{
    return UI_Settings_EnemyHealthbar_IsAvailable(option);
}

bool UI_Settings_AllyHealthbarColor_IsAvailable(
    const UI_SETTINGS_OPTION *const option)
{
    return UI_Settings_AllyHealthbar_IsAvailable(option);
}

bool UI_Settings_BarColorPC_IsVisible(const UI_SETTINGS_OPTION *const option)
{
    return !UI_Settings_IsCurrentBarLookPS1();
}

bool UI_Settings_BarColorPS1_IsVisible(const UI_SETTINGS_OPTION *const option)
{
    return UI_Settings_IsCurrentBarLookPS1();
}

bool UI_Settings_IdlePose_IsAvailable(const UI_SETTINGS_OPTION *const option)
{
    return g_Config.gameplay.idle_pose_timeout > 0;
}

const char *UI_Settings_ColorEditor_FormatValue(
    const UI_SETTINGS_OPTION *const option)
{
    const RGB_888 *const color = option->target;
    return String_FormatStatic("#%02X%02X%02X", color->r, color->g, color->b);
}

bool UI_Settings_ColorEditor_CanChangeValue(
    const UI_SETTINGS_OPTION *const option, const int32_t dir)
{
    return false;
}

bool UI_Settings_FixItemRots_IsAvailable(const UI_SETTINGS_OPTION *const option)
{
    return g_Config.visuals.enable_3d_pickups;
}

bool UI_Settings_FixStepGlitch_IsAvailable(
    const UI_SETTINGS_OPTION *const option)
{
    return g_Config.gameplay.enable_smooth_wall_deflect;
}

bool UI_Settings_FixWadeWallHit_IsAvailable(
    const UI_SETTINGS_OPTION *const option)
{
    return g_Config.gameplay.enable_wading;
}

bool UI_Settings_PauseMusicInInventory_IsAvailable(
    const UI_SETTINGS_OPTION *const option)
{
    return g_Config.audio.enable_music_in_inventory;
}

bool UI_Settings_BackgroundStyle_IsEnumValueAvailable(
    const UI_SETTINGS_OPTION *const option, const int32_t value)
{
    if (value == BK_PATTERN_STATIC || value == BK_PATTERN_WAVE) {
        return Object_Get(O_INV_BACKGROUND)->loaded;
    }
    return true;
}

bool UI_Settings_ShadowType_IsEnumValueAvailable(
    const UI_SETTINGS_OPTION *const option, const int32_t value)
{
    if (value == SHADOW_TYPE_SPRITE) {
        return Object_Get(O_SHADOW)->loaded;
    }
    return true;
}

bool UI_Settings_Volume_RequestChange(
    const UI_SETTINGS_OPTION *const option, const int32_t dir)
{
    UI_SettingsEditor_RequestChange(option, dir);
    if (option->target == &g_Config.audio.music_volume) {
        Music_SetVolume(g_Config.audio.music_volume);
    } else if (option->target == &g_Config.audio.sound_volume) {
        Sound_SetMasterVolume(g_Config.audio.sound_volume);
    }
    Sound_Effect(SFX_MENU_PASSPORT, nullptr, SPM_ALWAYS);
    return true;
}

bool UI_Settings_Flare_IsAvailable(const UI_SETTINGS_OPTION *const option)
{
    return g_Weapons[LGT_FLARE].is_available;
}

bool UI_Settings_Grenade_IsAvailable(const UI_SETTINGS_OPTION *const option)
{
    return g_Weapons[LGT_GRENADE].is_available;
}

bool UI_Settings_Harpoon_IsAvailable(const UI_SETTINGS_OPTION *const option)
{
    return g_Weapons[LGT_HARPOON].is_available;
}

bool UI_Settings_M16_IsAvailable(const UI_SETTINGS_OPTION *const option)
{
    return g_Weapons[LGT_M16].is_available || g_Weapons[LGT_MP5].is_available;
}

bool UI_Settings_ProjectileAreaDamage_IsAvailable(
    const UI_SETTINGS_OPTION *const option)
{
    return g_Weapons[LGT_ROCKET].is_available
        || g_Weapons[LGT_GRENADE].is_available;
}
