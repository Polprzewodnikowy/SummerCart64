#include <trx/game/ui/dialogs/setting_helpers/enums.h>

#include <trx/config.h>
#include <trx/game/lara/const.h>

const UI_SETTINGS_ENUM_ENTRY UI_Settings_StatDetailModeEnumEntries[] = {
    { STAT_DETAIL_MODE_MINIMAL },
    { STAT_DETAIL_MODE_DETAILED },
    { STAT_DETAIL_MODE_FULL },
    { -1 },
};

const UI_SETTINGS_ENUM_ENTRY UI_Settings_TargetModeEnumEntries[] = {
    { TARGET_LOCK_MODE_FULL },
    { TARGET_LOCK_MODE_SEMI },
    { TARGET_LOCK_MODE_NONE },
    { -1 },
};

const UI_SETTINGS_ENUM_ENTRY UI_Settings_LookModeEnumEntries[] = {
    { LOOK_MODE_RESTRICTED },
    { LOOK_MODE_ENHANCED },
    { LOOK_MODE_UNRESTRICTED },
    { -1 },
};

const UI_SETTINGS_ENUM_ENTRY UI_Settings_QuickGunsModeEnumEntries[] = {
    { QUICK_GUNS_MODE_DRAW_ONLY },
    { QUICK_GUNS_MODE_DRAW_AND_HOLSTER },
    { -1 },
};

const UI_SETTINGS_ENUM_ENTRY UI_Settings_JumpLockModeEnumEntries[] = {
    { JUMP_LOCK_LEGACY },
    { JUMP_LOCK_TUNED },
    { JUMP_LOCK_DISABLED },
    { -1 },
};

const UI_SETTINGS_ENUM_ENTRY UI_Settings_WallGlitchEnumEntries[] = {
    { WALL_GLITCH_FIXED },
    { WALL_GLITCH_TR1 },
    { WALL_GLITCH_TR2 },
    { -1 },
};

const UI_SETTINGS_ENUM_ENTRY UI_Settings_EnemyHealthBarShowModeEnumEntries[] = {
    { BAR_SHOW_MODE_NEVER },
    { BAR_SHOW_MODE_BOSS_ONLY },
    { BAR_SHOW_MODE_ALWAYS },
    { -1 },
};

const UI_SETTINGS_ENUM_ENTRY UI_Settings_UIElementLocationEnumEntries[] = {
    { UI_ELEMENT_LOCATION_TOP_LEFT },
    { UI_ELEMENT_LOCATION_TOP_CENTER },
    { UI_ELEMENT_LOCATION_TOP_RIGHT },
    { UI_ELEMENT_LOCATION_BOTTOM_LEFT },
    { UI_ELEMENT_LOCATION_BOTTOM_CENTER },
    { UI_ELEMENT_LOCATION_BOTTOM_RIGHT },
    { -1 },
};

const UI_SETTINGS_ENUM_ENTRY UI_Settings_BackgroundStyleEnumEntries[] = {
    { BK_NONE },
    { BK_TRANSPARENT_MEDIUM },
    { BK_TRANSPARENT_DARK },
    { BK_BLACK },
    { BK_PATTERN_STATIC },
    { BK_PATTERN_WAVE },
    { BK_MONOCHROME },
    { BK_MONOCHROME_COOL },
    { BK_MONOCHROME_WARM },
    { -1 },
};

const UI_SETTINGS_ENUM_ENTRY UI_Settings_CameraModeEnumEntries[] = {
    { CAMERA_MODE_TR1 },
    { CAMERA_MODE_TR2 },
    { CAMERA_MODE_TR3 },
    { -1 },
};

const UI_SETTINGS_ENUM_ENTRY UI_Settings_TextureFilterEnumEntries[] = {
    { TEXTURE_FILTER_POINT },
    { TEXTURE_FILTER_BILINEAR },
    { -1 },
};

const UI_SETTINGS_ENUM_ENTRY UI_Settings_MenuStyleEnumEntries[] = {
    { UI_STYLE_PS1 },
    { UI_STYLE_PC },
    { -1 },
};

const UI_SETTINGS_ENUM_ENTRY UI_Settings_LightingContrastEnumEntries[] = {
    { LIGHTING_CONTRAST_LOW },
    { LIGHTING_CONTRAST_MEDIUM },
    { LIGHTING_CONTRAST_HIGH },
    { -1 },
};

const UI_SETTINGS_ENUM_ENTRY UI_Settings_SpriteLockModeEnumEntries[] = {
    { BILLBOARD_LOCK_NONE },
    { BILLBOARD_LOCK_ROLL },
    { BILLBOARD_LOCK_ROLL_PITCH },
    { BILLBOARD_LOCK_PERSPECTIVE },
    { -1 },
};

const UI_SETTINGS_ENUM_ENTRY UI_Settings_AspectModeEnumEntries[] = {
    { ASPECT_MODE_4_3 },
    { ASPECT_MODE_16_9 },
    { ASPECT_MODE_16_10 },
    { ASPECT_MODE_ANY },
    { -1 },
};

const UI_SETTINGS_ENUM_ENTRY UI_Settings_ScreenshotFormatEnumEntries[] = {
    { SCREENSHOT_FORMAT_JPEG },
    { SCREENSHOT_FORMAT_PNG },
    { -1 },
};

const UI_SETTINGS_ENUM_ENTRY UI_Settings_AllyHostilityPolicyEnumEntries[] = {
    { ALLY_HOSTILITY_POLICY_INDIVIDUAL },
    { ALLY_HOSTILITY_POLICY_SHARED },
    { -1 },
};

const UI_SETTINGS_ENUM_ENTRY UI_Settings_ProjectileAreaDamageEnumEntries[] = {
    { PROJECTILE_AREA_DAMAGE_SINGLE_SWEEP },
    { PROJECTILE_AREA_DAMAGE_MULTI_SWEEP },
    { -1 },
};

const UI_SETTINGS_ENUM_ENTRY UI_Settings_LoadingScreensModeEnumEntries[] = {
    { LOADING_SCREENS_DISABLED },
    { LOADING_SCREENS_ALWAYS },
    { LOADING_SCREENS_NEW_GAMES },
    { -1 },
};

const UI_SETTINGS_ENUM_ENTRY UI_Settings_MusicLoadConditionEnumEntries[] = {
    { MUSIC_LOAD_CONDITION_NEVER },
    { MUSIC_LOAD_CONDITION_NON_AMBIENT },
    { MUSIC_LOAD_CONDITION_ALWAYS },
    { -1 },
};

const UI_SETTINGS_ENUM_ENTRY UI_Settings_ShadowTypeEnumEntries[] = {
    { SHADOW_TYPE_OCTAGON },
    { SHADOW_TYPE_CIRCLE },
    { SHADOW_TYPE_SPRITE },
    { -1 },
};

const UI_SETTINGS_ENUM_ENTRY UI_Settings_SunglassesModeEnumEntries[] = {
    { SUNGLASSES_MODE_OFF },
    { SUNGLASSES_MODE_OPAQUE },
    { SUNGLASSES_MODE_TRANSPARENT },
    { -1 },
};
