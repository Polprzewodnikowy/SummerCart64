#include <trx/game/ui/hud/overlay.h>

#include <trx/config.h>
#include <trx/core/memory.h>
#include <trx/core/strings.h>
#include <trx/game/camera.h>
#include <trx/game/const.h>
#include <trx/game/game.h>
#include <trx/game/game_strings/entries.h>
#include <trx/game/lara.h>
#include <trx/game/objects/names.h>
#include <trx/game/ui.h>
#include <trx/game/ui/scaler.h>
#include <trx/version.h>

typedef struct UI_OVERLAY_STATE {
    struct {
        bool state;
        int32_t frame;
    } blink;
    UI_FPS_COUNTER_STATE *fps;
    bool force_show_healthbar;
    bool show_arrows[6];
    bool show_version;
    UI_OVERLAY_TEXT top_text;
    UI_OVERLAY_TEXT bottom_text;
    UI_FLASH_STATE flash_state;
} UI_OVERLAY_STATE;

static struct {
    bool resize;
    const char *label;
} m_ArrowInfo[] = {
    [UI_OVERLAY_ARROW_TR] = { true, "\\{arrow up}" },
    [UI_OVERLAY_ARROW_TL] = { true, "\\{arrow up}" },
    [UI_OVERLAY_ARROW_BL] = { true, "\\{arrow down}" },
    [UI_OVERLAY_ARROW_BR] = { true, "\\{arrow down}" },
    [UI_OVERLAY_ARROW_BCL] = { false, "\\{button left}" },
    [UI_OVERLAY_ARROW_BCR] = { false, "\\{button right}" },
};

static bool M_LaraHealthBar(
    const UI_OVERLAY_STATE *const s, const UI_ELEMENT_LOCATION location)
{
    if (location != g_Config.ui.lara_health_bar.location) {
        return false;
    }
    if (!Lara_IsControllable()
        || (!Game_IsPlaying() && !s->force_show_healthbar)) {
        return false;
    }
    if (!g_Config.ui.enable_game_ui) {
        return false;
    }
    return UI_LaraHealthBar(s->blink.state, s->force_show_healthbar);
}

static bool M_LaraAirBar(
    const UI_OVERLAY_STATE *const s, const UI_ELEMENT_LOCATION location)
{
    if (location != g_Config.ui.lara_air_bar.location) {
        return false;
    }
    if (!Lara_IsControllable() || !Game_IsPlaying()) {
        return false;
    }
    if (!g_Config.ui.enable_game_ui) {
        return false;
    }
    return UI_LaraAirBar(s->blink.state);
}

static bool M_LaraSprintBar(
    const UI_OVERLAY_STATE *const s, const UI_ELEMENT_LOCATION location)
{
    if (location != g_Config.ui.lara_sprint_bar.location) {
        return false;
    }
    if (!Lara_IsControllable() || !Game_IsPlaying()) {
        return false;
    }
    if (!g_Config.ui.enable_game_ui) {
        return false;
    }
    return UI_LaraSprintBar();
}

static bool M_LaraExposureBar(
    const UI_OVERLAY_STATE *const s, const UI_ELEMENT_LOCATION location)
{
    if (location != g_Config.ui.lara_exposure_bar.location) {
        return false;
    }
    if (!Lara_IsControllable() || !Game_IsPlaying()) {
        return false;
    }
    if (!g_Config.ui.enable_game_ui) {
        return false;
    }
    return UI_LaraExposureBar(s->blink.state);
}

static const char *M_ResolveOverlayTextRaw(const UI_OVERLAY_TEXT *const t)
{
    if (t == nullptr) {
        return nullptr;
    }

    switch (t->kind) {
    case UI_OVERLAY_TEXT_NONE:
        return nullptr;
    case UI_OVERLAY_TEXT_LITERAL:
        return t->literal;
    case UI_OVERLAY_TEXT_GS_KEY:
        return GameString_Get(t->gs_key);
    case UI_OVERLAY_TEXT_OBJECT_NAME:
        return Object_GetName(t->object_id);
    }
    return nullptr;
}

static const char *M_ResolveOverlayText(const UI_OVERLAY_TEXT *const t)
{
    const char *const raw = M_ResolveOverlayTextRaw(t);
    if (raw == nullptr) {
        return nullptr;
    }

    if (t->fmt_gs_key == nullptr) {
        return raw;
    }

    return String_FormatStatic(GameString_Get(t->fmt_gs_key), raw);
}

static bool M_EnemyHealthBar(const UI_ELEMENT_LOCATION location)
{
    if (location != g_Config.ui.enemy_health_bar.location) {
        return false;
    }
    if (!Game_IsPlaying()) {
        return false;
    }
    if (!g_Config.ui.enable_game_ui) {
        return false;
    }
    return UI_EnemyHealthBar();
}

static bool M_AmmoLabel(const UI_ELEMENT_LOCATION location)
{
    if (location != g_Config.ui.ammo_counter.location) {
        return false;
    }
    if (!Game_IsPlaying()) {
        return false;
    }
    if (!g_Config.ui.enable_game_ui) {
        return false;
    }
    return UI_AmmoLabel();
}

static void M_Arrow(
    const UI_OVERLAY_STATE *const s, const UI_OVERLAY_ARROW arrow)
{
    if (s->show_arrows[arrow]) {
        // make sure the arrow has exactly the same size as the bar
        if (m_ArrowInfo[arrow].resize) {
            UI_BeginResize(
                -1.0,
                UI_BAR_HEIGHT * UI_Scaler_GetScale(UI_SCALER_TARGET_BAR)
                    / UI_Scaler_GetScale(UI_SCALER_TARGET_TEXT));
        }
        UI_Label(m_ArrowInfo[arrow].label);
        if (m_ArrowInfo[arrow].resize) {
            UI_EndResize();
        }
    }
}

static void M_DebugPosTopLeft(void)
{
    const ITEM *const lara = Lara_GetItem();
    const LARA_INFO *const lara_info = Lara_GetLaraInfo();
    if (lara == nullptr) {
        return;
    }

    const OBJECT_ID obj_id = Lara_GetAnimationObject();
    const ITEM *const vehicle = Lara_Vehicle_GetItem();
    UI_BeginStack(UI_STACK_HORIZONTAL);
    UI_BeginStack(UI_STACK_VERTICAL);
    if (g_Config.debug.enable_debug_pos) {
        UI_Label(GS("general/overlay/debug_position"));
        UI_Label(GS("general/overlay/debug_rotation"));
        UI_Label(GS("general/overlay/debug_speed"));
    }
    if (g_Config.debug.enable_debug_anim) {
        UI_Label(GS("general/overlay/debug_animation"));
        UI_Label(GS("general/overlay/debug_animation_state"));
    }
    if (g_Config.debug.enable_debug_camera) {
        UI_Label(GS("general/overlay/debug_camera_pos"));
        UI_Label(GS("general/overlay/debug_camera_target"));
    }
    UI_EndStack();
    UI_BeginStack(UI_STACK_VERTICAL);
    if (g_Config.debug.enable_debug_pos) {
        UI_Label(String_FormatStatic(
            "\\{small}%d, %d, %d / %d", lara->pos.x / WALL_L,
            lara->pos.y / WALL_L, lara->pos.z / WALL_L, lara->room_num));
        UI_Label(String_FormatStatic(
            "\\{small}%d°, %d°, %d°", (int32_t)lara->rot.x * 360 / DEG_360,
            (int32_t)lara->rot.y * 360 / DEG_360,
            (int32_t)lara->rot.z * 360 / DEG_360));
        UI_Label(String_FormatStatic(
            "\\{small}%d, %d",
            vehicle != nullptr ? vehicle->speed : lara->speed,
            vehicle != nullptr ? vehicle->fall_speed : lara->fall_speed));
    }
    if (g_Config.debug.enable_debug_anim) {
        UI_Label(String_FormatStatic(
            "\\{small}%d, %d", Item_GetRelativeObjAnim(lara, obj_id),
            Item_GetRelativeFrame(lara)));
        UI_Label(String_FormatStatic(
            "\\{small}%d, %d (%d)", lara->current_anim_state,
            lara->goal_anim_state, obj_id));
    }
    if (g_Config.debug.enable_debug_camera) {
        UI_Label(String_FormatStatic(
            "\\{small}%d, %d, %d / %d", g_Camera.pos.x / WALL_L,
            g_Camera.pos.y / WALL_L, g_Camera.pos.z / WALL_L,
            g_Camera.pos.room_num));
        UI_Label(String_FormatStatic(
            "\\{small}%d, %d, %d / %d", g_Camera.target.x / WALL_L,
            g_Camera.target.y / WALL_L, g_Camera.target.z / WALL_L,
            g_Camera.target.room_num));
    }
    UI_EndStack();
    UI_EndStack();
}

static void M_DebugPosTopRight(void)
{
    const ITEM *const lara = Lara_GetItem();
    if (lara == nullptr) {
        return;
    }

    UI_BeginStackEx((UI_STACK_SETTINGS) {
        .orientation = UI_STACK_VERTICAL,
        .align = { .h = UI_STACK_H_ALIGN_RIGHT },
    });
    if (g_Config.debug.enable_debug_pos) {
        UI_Label(String_FormatStatic(
            "\\{small}%d, %d, %d", lara->pos.x, lara->pos.y, lara->pos.z));
        UI_Label(String_FormatStatic(
            "\\{small}%d, %d, %d", lara->rot.x, lara->rot.y, lara->rot.z));
    }
    if (g_Config.debug.enable_debug_camera) {
        UI_Label(String_FormatStatic(
            "\\{small}%d, %d, %d", g_Camera.pos.x, g_Camera.pos.y,
            g_Camera.pos.z));
        UI_Label(String_FormatStatic(
            "\\{small}%d, %d, %d", g_Camera.target.x, g_Camera.target.y,
            g_Camera.target.z));
    }
    if (g_Config.debug.enable_debug_status
        && g_Config.debug.enable_invulnerability) {
        UI_LabelEx(
            GS("general/overlay/debug_immune"),
            (UI_LABEL_SETTINGS) { .scale = 0.8 });
    }
    UI_EndStack();
}

static bool M_CommonRegion(
    const UI_OVERLAY_STATE *const s, const UI_ELEMENT_LOCATION location)
{
    bool shown = false;
    UI_BeginStackEx((UI_STACK_SETTINGS) {
        .orientation = UI_STACK_VERTICAL,
        .align = { .h = UI_STACK_H_ALIGN_CENTER },
        .spacing = { .v = 10 },
    });
    shown |= M_LaraHealthBar(s, location);
    shown |= M_LaraAirBar(s, location);
    shown |= M_LaraSprintBar(s, location);
    shown |= M_LaraExposureBar(s, location);
    shown |= M_EnemyHealthBar(location);
    UI_EndStack();
    shown |= M_AmmoLabel(location);
    return shown;
}

static void M_TopLeftRegion(const UI_OVERLAY_STATE *const s)
{
    UI_BeginOverlayRegion(0.0f, 0.0f);
    if (!M_CommonRegion(s, UI_ELEMENT_LOCATION_TOP_LEFT)) {
        M_Arrow(s, UI_OVERLAY_ARROW_TL);
    }
    if (g_Config.ui.enable_game_ui) {
        M_DebugPosTopLeft();
        if (g_Config.ui.enable_fps_counter) {
            UI_FPSCounter(s->fps);
        }
    }
    UI_EndOverlayRegion();
}

static void M_TopCenterRegion(const UI_OVERLAY_STATE *const s)
{
    UI_BeginOverlayRegion(0.5f, 0.0f);
    M_CommonRegion(s, UI_ELEMENT_LOCATION_TOP_CENTER);
    {
        const char *const txt = M_ResolveOverlayText(&s->top_text);
        if (txt != nullptr) {
            if (s->top_text.flash_enabled) {
                UI_BeginFlash(&s->flash_state);
            }
            UI_Label(txt);
            if (s->top_text.flash_enabled) {
                UI_EndFlash();
            }
        }
    }
    UI_EndOverlayRegion();
}

static void M_TopRightRegion(const UI_OVERLAY_STATE *const s)
{
    UI_BeginOverlayRegion(1.0f, 0.0f);
    if (!M_CommonRegion(s, UI_ELEMENT_LOCATION_TOP_RIGHT)) {
        M_Arrow(s, UI_OVERLAY_ARROW_TR);
    }
    if (g_Config.ui.enable_game_ui) {
        M_DebugPosTopRight();
    }
    UI_EndOverlayRegion();
}

static void M_BottomLeftRegion(const UI_OVERLAY_STATE *const s)
{
    UI_BeginOverlayRegion(0.0f, 1.0f);
    if (!M_CommonRegion(s, UI_ELEMENT_LOCATION_BOTTOM_LEFT)) {
        M_Arrow(s, UI_OVERLAY_ARROW_BL);
    }
    UI_EndOverlayRegion();
}

static void M_BottomCenterRegion(const UI_OVERLAY_STATE *const s)
{
    UI_BeginOverlayRegion(0.5f, 1.0f);
    {
        const char *const txt = M_ResolveOverlayText(&s->bottom_text);
        if (txt != nullptr) {
            if (s->bottom_text.flash_enabled) {
                UI_BeginFlash(&s->flash_state);
            }
            UI_BeginRowArrows(
                s->show_arrows[UI_OVERLAY_ARROW_BCL],
                s->show_arrows[UI_OVERLAY_ARROW_BCR], UI_ROW_ARROWS_WIDE);
            UI_Label(txt);
            UI_EndRowArrows();
            if (s->bottom_text.flash_enabled) {
                UI_EndFlash();
            }
        }
    }
    M_CommonRegion(s, UI_ELEMENT_LOCATION_BOTTOM_CENTER);
    UI_EndOverlayRegion();
}

static void M_BottomRightRegion(const UI_OVERLAY_STATE *const s)
{
    UI_BeginOverlayRegion(1.0f, 1.0f);
    if (!M_CommonRegion(s, UI_ELEMENT_LOCATION_BOTTOM_RIGHT)) {
        M_Arrow(s, UI_OVERLAY_ARROW_BR);
    }
    if (s->show_version) {
        UI_LabelEx(g_TRXVersion, (UI_LABEL_SETTINGS) { .scale = 0.5f });
    }
    UI_EndOverlayRegion();
}

UI_OVERLAY_STATE *UI_Overlay_Init(void)
{
    UI_OVERLAY_STATE *const s = Memory_Alloc(sizeof(UI_OVERLAY_STATE));
    s->fps = UI_FPSCounter_Init();
    UI_Flash_Init(&s->flash_state, 20);
    return s;
}

void UI_Overlay_Free(UI_OVERLAY_STATE *const s)
{
    if (s == nullptr) {
        return;
    }
    if (s->fps != nullptr) {
        UI_FPSCounter_Free(s->fps);
        s->fps = nullptr;
    }
    UI_Flash_Free(&s->flash_state);
    Memory_Free(s);
}

void UI_Overlay_Control(UI_OVERLAY_STATE *const s)
{
    s->force_show_healthbar = false;
    UI_LaraHealthBar_Control();
    s->blink.frame++;
    if (s->blink.frame >= 10) {
        s->blink.state = !s->blink.state;
        s->blink.frame = 0;
    }
    UI_Flash_Control(&s->flash_state);
}

void UI_Overlay_ForceHealthBar(UI_OVERLAY_STATE *const s, const bool show)
{
    s->force_show_healthbar = show;
}

void UI_Overlay(UI_OVERLAY_STATE *const s)
{
    M_TopLeftRegion(s);
    M_TopCenterRegion(s);
    M_TopRightRegion(s);

    M_BottomLeftRegion(s);
    M_BottomCenterRegion(s);
    M_BottomRightRegion(s);
}

void UI_BeginOverlayRegion(const float x, const float y)
{
    // clang-format off
    const UI_STACK_H_ALIGN h_align =
        x > 0.55f ? UI_STACK_H_ALIGN_RIGHT :
        x < 0.45f ? UI_STACK_H_ALIGN_LEFT :
        UI_STACK_H_ALIGN_CENTER;
    // clang-format on
    UI_BeginModal(x, y);
    UI_BeginPad(20.0f, 14.0f);
    UI_BeginStackEx((UI_STACK_SETTINGS) {
        .orientation = UI_STACK_VERTICAL,
        .align = { .h = h_align },
        .spacing = { .v = 3 },
    });
}

void UI_EndOverlayRegion(void)
{
    UI_EndStack();
    UI_EndPad();
    UI_EndModal();
}

void UI_Overlay_ShowArrow(
    UI_OVERLAY_STATE *const s, const UI_OVERLAY_ARROW arrow, const bool show)
{
    s->show_arrows[arrow] = show;
}

void UI_Overlay_ShowVersion(UI_OVERLAY_STATE *const s, const bool show)
{
    s->show_version = show;
}

void UI_Overlay_SetTopText(
    UI_OVERLAY_STATE *const s, const UI_OVERLAY_TEXT text)
{
    s->top_text = text;
}

void UI_Overlay_SetBottomText(
    UI_OVERLAY_STATE *const s, const UI_OVERLAY_TEXT text)
{
    s->bottom_text = text;
}
