#include <trx/game/input/common.h>

#include <trx/core/enum_map.h>
#include <trx/core/strings.h>
#include <trx/game/clock.h>
#include <trx/game/game_strings/entries.h>
#include <trx/game/input/backends/controller.h>
#include <trx/game/input/backends/keyboard.h>
#include <trx/version.h>

#include <SDL2/SDL_keyboard.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef enum {
    HOLD_INACTIVE,
    HOLD_DELAY,
    HOLD_REPEATING,
} M_HOLD_STATE;

typedef struct {
    CLOCK_TIMER delay_timer;
    CLOCK_TIMER repeat_timer;
    double delay_time;
    double hold_time;
    M_HOLD_STATE state;
    INPUT_ROLE role;
} M_HOLD_CHECK;

INPUT_STATE g_Input = {};
INPUT_STATE g_InputDB = {};
INPUT_STATE g_OldInputDB = {};

static bool m_ListenMode = false;

static M_HOLD_CHECK m_HoldChecks[] = {
    { .role = INPUT_ROLE_MENU_UP, .delay_time = 0.4, .hold_time = 0.1 },
    { .role = INPUT_ROLE_MENU_DOWN, .delay_time = 0.4, .hold_time = 0.1 },
    { .role = INPUT_ROLE_MENU_LEFT, .delay_time = 0.4, .hold_time = 0.2 },
    { .role = INPUT_ROLE_MENU_RIGHT, .delay_time = 0.4, .hold_time = 0.2 },
    { .role = INPUT_ROLE_MENU_SKIP, .delay_time = 0.4, .hold_time = 0.1 },
    { .role = (INPUT_ROLE)-1 }, // sentinel
};

static bool m_IsRoleHardcoded[INPUT_ROLE_NUMBER_OF] = {
    // clang-format off
    [INPUT_ROLE_RESET_BINDINGS]           = true,
    [INPUT_ROLE_UNBIND_KEY]               = true,
    [INPUT_ROLE_MENU_CONFIRM]             = true,
    [INPUT_ROLE_MENU_BACK]                = true,
    [INPUT_ROLE_MENU_LEFT]                = true,
    [INPUT_ROLE_MENU_RIGHT]               = true,
    [INPUT_ROLE_MENU_UP]                  = true,
    [INPUT_ROLE_MENU_DOWN]                = true,
    // clang-format on
};

static bool m_IsRoleNonUnbindable[INPUT_ROLE_NUMBER_OF] = {
    // clang-format off
    [INPUT_ROLE_UP]          = true,
    [INPUT_ROLE_DOWN]        = true,
    [INPUT_ROLE_LEFT]        = true,
    [INPUT_ROLE_RIGHT]       = true,
    [INPUT_ROLE_DRAW_WEAPON] = true,
    [INPUT_ROLE_ACTION]      = true,
    [INPUT_ROLE_JUMP]        = true,
    [INPUT_ROLE_ROLL]        = true,
    [INPUT_ROLE_LOOK]        = true,
    [INPUT_ROLE_SLOW]        = true,
    [INPUT_ROLE_INVENTORY]   = true,
    // clang-format on
};

static const GAME_STRING_ID m_LayoutMap[INPUT_LAYOUT_NUMBER_OF] = {
    [INPUT_LAYOUT_DEFAULT] = GS_ID("general/settings/controls/layout/default"),
    [INPUT_LAYOUT_CUSTOM_1] =
        GS_ID("general/settings/controls/layout/custom_1"),
    [INPUT_LAYOUT_CUSTOM_2] =
        GS_ID("general/settings/controls/layout/custom_2"),
    [INPUT_LAYOUT_CUSTOM_3] =
        GS_ID("general/settings/controls/layout/custom_3"),
};

static INPUT_BACKEND_IMPL *M_GetBackend(const INPUT_BACKEND backend)
{
    switch (backend) {
    case INPUT_BACKEND_KEYBOARD:
        return &g_Input_Keyboard;
    case INPUT_BACKEND_CONTROLLER:
        return &g_Input_Controller;
    default:
        return nullptr;
    }
}

static bool M_IsPressed(const INPUT_STATE input, const INPUT_ROLE role)
{
    switch (role) {
#define X_INPUT_ROLE(role_name, state_name)                                    \
    case role_name:                                                            \
        return input.state_name;
#include <trx/game/input/roles.def>
#undef X_INPUT_ROLE
    case INPUT_ROLE_NUMBER_OF:
        break;
    }
    return false;
}

static INPUT_STATE M_SetPressed(
    INPUT_STATE input, const INPUT_ROLE role, const bool is_pressed)
{
    switch (role) {
#define X_INPUT_ROLE(role_name, state_name)                                    \
    case role_name:                                                            \
        input.state_name = is_pressed;                                         \
        break;
#include <trx/game/input/roles.def>
#undef X_INPUT_ROLE
    case INPUT_ROLE_NUMBER_OF:
        break;
    }
    return input;
}

void Input_Init(void)
{
    for (int32_t i = 0; m_HoldChecks[i].role != (INPUT_ROLE)-1; i++) {
        m_HoldChecks[i].delay_timer.type = CLOCK_TIMER_REAL;
        m_HoldChecks[i].repeat_timer.type = CLOCK_TIMER_REAL;
    }
    if (g_Input_Keyboard.init != nullptr) {
        g_Input_Keyboard.init();
    }
    if (g_Input_Controller.init != nullptr) {
        g_Input_Controller.init();
    }
}

void Input_Shutdown(void)
{
    if (g_Input_Keyboard.shutdown != nullptr) {
        g_Input_Keyboard.shutdown();
    }
    if (g_Input_Controller.shutdown != nullptr) {
        g_Input_Controller.shutdown();
    }
}

void Input_Discover(void)
{
    if (g_Input_Keyboard.discover != nullptr) {
        g_Input_Keyboard.discover();
    }
    if (g_Input_Controller.discover != nullptr) {
        g_Input_Controller.discover();
    }
}

bool Input_IsRoleRebindable(const INPUT_ROLE role)
{
    return !m_IsRoleHardcoded[role];
}

bool Input_IsRoleUnbindable(const INPUT_ROLE role)
{
    return !m_IsRoleNonUnbindable[role];
}

bool Input_IsPressed(const INPUT_ROLE role)
{
    return M_IsPressed(g_Input, role);
}

bool Input_IsPressedDB(const INPUT_ROLE role)
{
    return M_IsPressed(g_InputDB, role);
}

bool Input_IsPressedEx(
    const INPUT_BACKEND backend, const INPUT_LAYOUT layout,
    const INPUT_ROLE role)
{
    return M_GetBackend(backend)->is_pressed(layout, role);
}

bool Input_IsKeyConflicted(
    const INPUT_BACKEND backend, const INPUT_LAYOUT layout,
    const INPUT_ROLE role)
{
    return M_GetBackend(backend)->is_role_conflicted(layout, role);
}

bool Input_ReadAndAssignRole(
    const INPUT_BACKEND backend, const INPUT_LAYOUT layout,
    const INPUT_ROLE role)
{
    // Check for canceling from other devices
    for (INPUT_BACKEND other_backend = 0;
         other_backend < INPUT_BACKEND_NUMBER_OF; other_backend++) {
        if (other_backend == backend) {
            continue;
        }
        if (Input_IsPressedEx(other_backend, layout, INPUT_ROLE_MENU_BACK)
            || Input_IsPressedEx(other_backend, layout, INPUT_ROLE_INVENTORY)) {
            return true;
        }
    }

    return M_GetBackend(backend)->read_and_assign(layout, role);
}

void Input_UnassignRole(
    const INPUT_BACKEND backend, const INPUT_LAYOUT layout,
    const INPUT_ROLE role)
{
    M_GetBackend(backend)->unassign_role(layout, role);
}

const char *Input_GetKeyName(
    const INPUT_BACKEND backend, const INPUT_LAYOUT layout,
    const INPUT_ROLE role)
{
    return M_GetBackend(backend)->get_name(layout, role);
}

void Input_ResetLayout(const INPUT_BACKEND backend, const INPUT_LAYOUT layout)
{
    M_GetBackend(backend)->reset_layout(layout);
}

void Input_EnterListenMode(void)
{
    m_ListenMode = true;
}

void Input_ExitListenMode(void)
{
    m_ListenMode = false;
    Input_Update();
    InputState_Copy(&g_OldInputDB, g_Input);
    InputState_Copy(&g_InputDB, g_Input);
}

bool Input_IsInListenMode(void)
{
    return m_ListenMode;
}

void Input_ProcessEvent(const SDL_Event *event)
{
    if (g_Input_Keyboard.process_event != nullptr) {
        g_Input_Keyboard.process_event(event);
    }
    if (g_Input_Controller.process_event != nullptr) {
        g_Input_Controller.process_event(event);
    }
}

bool Input_AssignFromJSONObject(
    const INPUT_BACKEND backend, const INPUT_LAYOUT layout,
    JSON_OBJECT *const bind_obj)
{
    INPUT_ROLE role = (INPUT_ROLE)-1;

    // TR1X <=4.5, TR2X <=0.5
    const int32_t role_idx = JSON_ObjectGetInt(bind_obj, "role", -1);
    // clang-format off
    switch (role_idx) {
    case 0: role = INPUT_ROLE_UP; break;
    case 1: role = INPUT_ROLE_DOWN; break;
    case 2: role = INPUT_ROLE_LEFT; break;
    case 3: role = INPUT_ROLE_RIGHT; break;
    case 4: role = INPUT_ROLE_STEP_LEFT; break;
    case 5: role = INPUT_ROLE_STEP_RIGHT; break;
    case 6: role = INPUT_ROLE_SLOW; break;
    case 7: role = INPUT_ROLE_JUMP; break;
    case 8: role = INPUT_ROLE_ACTION; break;
    case 9: role = INPUT_ROLE_DRAW_WEAPON; break;
    case 10: role = INPUT_ROLE_LOOK; break;
    case 11: role = INPUT_ROLE_ROLL; break;
    case 12: role = INPUT_ROLE_INVENTORY; break;
    case 13: role = INPUT_ROLE_FLY_CHEAT; break;
    case 14: role = INPUT_ROLE_ITEM_CHEAT; break;
    case 15: role = INPUT_ROLE_LEVEL_SKIP_CHEAT; break;
    case 16: role = INPUT_ROLE_TURBO_CHEAT; break;
    case 17: role = INPUT_ROLE_PAUSE; break;
    case 18: role = INPUT_ROLE_CAMERA_FORWARD; break;
    case 19: role = INPUT_ROLE_CAMERA_BACK; break;
    case 20: role = INPUT_ROLE_CAMERA_LEFT; break;
    case 21: role = INPUT_ROLE_CAMERA_RIGHT; break;
    case 22: role = INPUT_ROLE_CAMERA_RESET; break;
    case 23: role = INPUT_ROLE_EQUIP_PISTOLS; break;
    case 24: role = INPUT_ROLE_EQUIP_SHOTGUN; break;
    case 25: role = INPUT_ROLE_EQUIP_MAGNUMS; break;
    case 26: role = INPUT_ROLE_EQUIP_UZIS; break;
    case 27: role = INPUT_ROLE_USE_SMALL_MEDI; break;
    case 28: role = INPUT_ROLE_USE_BIG_MEDI; break;
    case 29: role = INPUT_ROLE_SAVE; break;
    case 30: role = INPUT_ROLE_LOAD; break;
    case 31: role = INPUT_ROLE_FPS; break;
    case 32: role = INPUT_ROLE_TOGGLE_BILINEAR_FILTER; break;
    case 33: role = INPUT_ROLE_ENTER_CONSOLE; break;
    case 34: role = INPUT_ROLE_CHANGE_TARGET; break;
    case 35: role = INPUT_ROLE_TOGGLE_UI; break;
    case 36: role = INPUT_ROLE_CAMERA_UP; break;
    case 37: role = INPUT_ROLE_CAMERA_DOWN; break;
    case 38: role = INPUT_ROLE_TOGGLE_PHOTO_MODE; break;
    case 39: role = INPUT_ROLE_UNBIND_KEY; break;
    case 40: role = INPUT_ROLE_RESET_BINDINGS; break;
    case 42: role = INPUT_ROLE_TOGGLE_TRAPEZOID_FILTER; break;
    case 43: role = INPUT_ROLE_MENU_CONFIRM; break;
    case 44: role = INPUT_ROLE_MENU_BACK; break;
    case 45: role = INPUT_ROLE_MENU_LEFT; break;
    case 46: role = INPUT_ROLE_MENU_UP; break;
    case 47: role = INPUT_ROLE_MENU_DOWN; break;
    case 48: role = INPUT_ROLE_MENU_RIGHT; break;
    case 49: role = INPUT_ROLE_SCREENSHOT; break;
    case 50: role = INPUT_ROLE_TOGGLE_FULLSCREEN; break;
    }
    // clang-format on

    // TR1X >= 4.6, TR2X >= 0.6
    if (role == (INPUT_ROLE)-1) {
        role = ENUM_MAP_GET(
            INPUT_ROLE, JSON_ObjectGetString(bind_obj, "role", ""),
            (int32_t)(INPUT_ROLE)-1);
    }

    if (role == (INPUT_ROLE)-1) {
        return false;
    }

    return M_GetBackend(backend)->assign_from_json_object(
        layout, role, bind_obj);
}

bool Input_AssignToJSONObject(
    const INPUT_BACKEND backend, const INPUT_LAYOUT layout,
    JSON_OBJECT *const bind_obj, const INPUT_ROLE role)
{
    JSON_ObjectAppendString(
        bind_obj, "role", ENUM_MAP_TO_STRING(INPUT_ROLE, role));
    return M_GetBackend(backend)->assign_to_json_object(layout, role, bind_obj);
}

const char *const *Input_GetLayoutNamePtr(const INPUT_LAYOUT layout)
{
    return GameString_GetPtr(m_LayoutMap[layout]);
}

INPUT_STATE Input_GetDebounced(const INPUT_STATE input)
{
    INPUT_STATE result;
    for (int32_t i = 0; i < INPUT_STATE_ANY_WORDS; i++) {
        result.any[i] = input.any[i] & ~g_OldInputDB.any[i];
    }

    // Allow holding certain keys
    for (int32_t i = 0; m_HoldChecks[i].role != (INPUT_ROLE)-1; i++) {
        M_HOLD_CHECK *const hold_check = &m_HoldChecks[i];
        if (!M_IsPressed(input, hold_check->role)) {
            hold_check->state = HOLD_INACTIVE;
        } else if (hold_check->state == HOLD_INACTIVE) {
            hold_check->state = HOLD_DELAY;
            ClockTimer_Sync(&hold_check->delay_timer);
        } else if (
            hold_check->state == HOLD_DELAY
            && ClockTimer_CheckElapsedAndTake(
                &hold_check->delay_timer, hold_check->delay_time)) {
            hold_check->state = HOLD_REPEATING;
        } else if (
            hold_check->state == HOLD_REPEATING
            && ClockTimer_CheckElapsedAndTake(
                &hold_check->repeat_timer, hold_check->hold_time)) {
            result = M_SetPressed(result, hold_check->role, true);
        }
    }

    g_OldInputDB = input;
    return result;
}

const char *Input_GetRoleName(const INPUT_ROLE role)
{
    return EnumMap_GetLabel(ENUM_MAP_NAME(INPUT_ROLE), role);
}

const char *Input_KeyDescFromSDL(SDL_Scancode scancode, SDL_Keymod mod)
{
    // clang-format off
    const char *mods = "";
    if (mod & KMOD_CTRL)  { mods = String_FormatStatic("%sctrl+",  mods); }
    if (mod & KMOD_SHIFT) { mods = String_FormatStatic("%sshift+", mods); }
    if (mod & KMOD_ALT)   { mods = String_FormatStatic("%salt+",   mods); }
    if (mod & KMOD_GUI)   { mods = String_FormatStatic("%sgui+",   mods); }
    // clang-format on

    const char *const name = SDL_GetScancodeName(scancode);
    if (name == nullptr || name[0] == '\0') {
        return nullptr;
    }

    char *const full = (char *)String_FormatStatic("%s%s", mods, name);
    for (size_t i = 0; i < strlen(full); i++) {
        full[i] = (char)tolower((unsigned char)full[i]);
    }
    return full;
}

bool Input_ParseKeyDesc(
    const char *const desc, SDL_Scancode *const scancode, SDL_Keymod *const mod)
{
    if (desc == nullptr || scancode == nullptr || mod == nullptr) {
        return false;
    }

    SDL_Keymod m = KMOD_NONE;
    const char *keystr = desc;
    const char *last = strrchr(desc, '+');

    if (last != nullptr) {
        for (const char *tok = desc; tok < last; tok = strchr(tok, '+') + 1) {
            const size_t len =
                strchr(tok, '+') ? strchr(tok, '+') - tok : last - tok;
            if (strncmp(tok, "ctrl", len) == 0) {
                m |= KMOD_CTRL;
            } else if (strncmp(tok, "shift", len) == 0) {
                m |= KMOD_SHIFT;
            } else if (strncmp(tok, "alt", len) == 0) {
                m |= KMOD_ALT;
            } else if (strncmp(tok, "gui", len) == 0) {
                m |= KMOD_GUI;
            }
        }
        keystr = last + 1;
    }

    *scancode = SDL_GetScancodeFromName(keystr);
    *mod = m;
    return *scancode != SDL_SCANCODE_UNKNOWN;
}

void InputState_Clear(INPUT_STATE *const state)
{
    for (int32_t i = 0; i < INPUT_STATE_ANY_WORDS; i++) {
        state->any[i] = 0;
    }
}

void InputState_Copy(INPUT_STATE *const dst, const INPUT_STATE src)
{
    for (int32_t i = 0; i < INPUT_STATE_ANY_WORDS; i++) {
        dst->any[i] = src.any[i];
    }
}

bool InputState_IsAnyPressed(const INPUT_STATE state)
{
    for (int32_t i = 0; i < INPUT_STATE_ANY_WORDS; i++) {
        if (state.any[i] != 0) {
            return true;
        }
    }
    return false;
}
