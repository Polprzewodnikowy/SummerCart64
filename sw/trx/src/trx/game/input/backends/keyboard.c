#include <trx/game/input/backends/keyboard.h>

#include <trx/game/input/backends/internal.h>
#include <trx/version.h>

#include <SDL2/SDL_keyboard.h>

// Key state table updated via SDL events.
#define KEY_DOWN(a) (m_KeyboardState[(a)])

typedef struct {
    INPUT_ROLE role;
    SDL_Scancode scancode;
} BUILTIN_KEYBOARD_LAYOUT;

static bool m_KeyboardState[SDL_NUM_SCANCODES] = {};
static bool m_Conflicts[INPUT_LAYOUT_NUMBER_OF][INPUT_ROLE_NUMBER_OF] = {};

static BUILTIN_KEYBOARD_LAYOUT m_BuiltinLayout[] = {
// clang-format off
#define INPUT_KEYBOARD_ASSIGN(role, key) { role, key },
#include <trx/game/input/backends/keyboard.def>
    { -1, SDL_SCANCODE_UNKNOWN },
    // clang-format on
};

static SDL_Scancode m_Layout[INPUT_LAYOUT_NUMBER_OF][INPUT_ROLE_NUMBER_OF];

// Update internal controller button/axis state from SDL events.
// @param event     Event to process.
static void M_ProcessEvent(const SDL_Event *const event)
{
    switch (event->type) {
    case SDL_KEYDOWN:
        if (!event->key.repeat) {
            m_KeyboardState[event->key.keysym.scancode] = true;
        }
        break;
    case SDL_KEYUP:
        m_KeyboardState[event->key.keysym.scancode] = false;
        break;
    default:
        break;
    }
}

static const char *M_GetScancodeName(SDL_Scancode scancode)
{
    // clang-format off
    switch (scancode) {
        case SDL_SCANCODE_LCTRL:              return "\\{keyboard l_ctrl}";
        case SDL_SCANCODE_RCTRL:              return "\\{keyboard r_ctrl}";
        case SDL_SCANCODE_RSHIFT:             return "\\{keyboard r_shift}";
        case SDL_SCANCODE_LSHIFT:             return "\\{keyboard l_shift}";
        case SDL_SCANCODE_RALT:               return "\\{keyboard l_alt}";
        case SDL_SCANCODE_LALT:               return "\\{keyboard r_alt}";
        case SDL_SCANCODE_LGUI:               return "\\{keyboard l_win}";
        case SDL_SCANCODE_RGUI:               return "\\{keyboard r_win}";

        case SDL_SCANCODE_LEFT:               return "\\{keyboard left}";
        case SDL_SCANCODE_UP:                 return "\\{keyboard up}";
        case SDL_SCANCODE_RIGHT:              return "\\{keyboard right}";
        case SDL_SCANCODE_DOWN:               return "\\{keyboard down}";

        case SDL_SCANCODE_RETURN:             return "\\{keyboard return}";
        case SDL_SCANCODE_ESCAPE:             return "\\{keyboard escape}";
        case SDL_SCANCODE_BACKSPACE:          return "\\{keyboard backspace}";
        case SDL_SCANCODE_TAB:                return "\\{keyboard tab}";
        case SDL_SCANCODE_SPACE:              return "\\{keyboard space}";
        case SDL_SCANCODE_CAPSLOCK:           return "\\{keyboard caps_lock}";
        case SDL_SCANCODE_PRINTSCREEN:        return "\\{keyboard print_screen}";
        case SDL_SCANCODE_SCROLLLOCK:         return "\\{keyboard scroll_lock}";
        case SDL_SCANCODE_PAUSE:              return "\\{keyboard pause}";
        case SDL_SCANCODE_INSERT:             return "\\{keyboard insert}";
        case SDL_SCANCODE_HOME:               return "\\{keyboard home}";
        case SDL_SCANCODE_PAGEUP:             return "\\{keyboard page_up}";
        case SDL_SCANCODE_DELETE:             return "\\{keyboard delete}";
        case SDL_SCANCODE_END:                return "\\{keyboard end}";
        case SDL_SCANCODE_PAGEDOWN:           return "\\{keyboard page_down}";

        case SDL_SCANCODE_A:                  return "\\{keyboard a}";
        case SDL_SCANCODE_B:                  return "\\{keyboard b}";
        case SDL_SCANCODE_C:                  return "\\{keyboard c}";
        case SDL_SCANCODE_D:                  return "\\{keyboard d}";
        case SDL_SCANCODE_E:                  return "\\{keyboard e}";
        case SDL_SCANCODE_F:                  return "\\{keyboard f}";
        case SDL_SCANCODE_G:                  return "\\{keyboard g}";
        case SDL_SCANCODE_H:                  return "\\{keyboard h}";
        case SDL_SCANCODE_I:                  return "\\{keyboard i}";
        case SDL_SCANCODE_J:                  return "\\{keyboard j}";
        case SDL_SCANCODE_K:                  return "\\{keyboard k}";
        case SDL_SCANCODE_L:                  return "\\{keyboard l}";
        case SDL_SCANCODE_M:                  return "\\{keyboard m}";
        case SDL_SCANCODE_N:                  return "\\{keyboard n}";
        case SDL_SCANCODE_O:                  return "\\{keyboard o}";
        case SDL_SCANCODE_P:                  return "\\{keyboard p}";
        case SDL_SCANCODE_Q:                  return "\\{keyboard q}";
        case SDL_SCANCODE_R:                  return "\\{keyboard r}";
        case SDL_SCANCODE_S:                  return "\\{keyboard s}";
        case SDL_SCANCODE_T:                  return "\\{keyboard t}";
        case SDL_SCANCODE_U:                  return "\\{keyboard u}";
        case SDL_SCANCODE_V:                  return "\\{keyboard v}";
        case SDL_SCANCODE_W:                  return "\\{keyboard w}";
        case SDL_SCANCODE_X:                  return "\\{keyboard x}";
        case SDL_SCANCODE_Y:                  return "\\{keyboard y}";
        case SDL_SCANCODE_Z:                  return "\\{keyboard z}";

        case SDL_SCANCODE_0:                  return "\\{keyboard 0}";
        case SDL_SCANCODE_1:                  return "\\{keyboard 1}";
        case SDL_SCANCODE_2:                  return "\\{keyboard 2}";
        case SDL_SCANCODE_3:                  return "\\{keyboard 3}";
        case SDL_SCANCODE_4:                  return "\\{keyboard 4}";
        case SDL_SCANCODE_5:                  return "\\{keyboard 5}";
        case SDL_SCANCODE_6:                  return "\\{keyboard 6}";
        case SDL_SCANCODE_7:                  return "\\{keyboard 7}";
        case SDL_SCANCODE_8:                  return "\\{keyboard 8}";
        case SDL_SCANCODE_9:                  return "\\{keyboard 9}";

        case SDL_SCANCODE_MINUS:              return "\\{keyboard minus}";
        case SDL_SCANCODE_EQUALS:             return "\\{keyboard equals}";
        case SDL_SCANCODE_LEFTBRACKET:        return "\\{keyboard left_square_bracket}";
        case SDL_SCANCODE_RIGHTBRACKET:       return "\\{keyboard right_square_bracket}";
        case SDL_SCANCODE_BACKSLASH:          return "\\{keyboard backslash}";
        case SDL_SCANCODE_NONUSHASH:          return "\\{keyboard hash}";
        case SDL_SCANCODE_SEMICOLON:          return "\\{keyboard semicolon}";
        case SDL_SCANCODE_APOSTROPHE:         return "\\{keyboard apostrophe}";
        case SDL_SCANCODE_GRAVE:              return "\\{keyboard backtick}";
        case SDL_SCANCODE_COMMA:              return "\\{keyboard comma}";
        case SDL_SCANCODE_PERIOD:             return "\\{keyboard period}";
        case SDL_SCANCODE_SLASH:              return "\\{keyboard slash}";
        case SDL_SCANCODE_NONUSBACKSLASH:     return "\\{keyboard backslash}";

        case SDL_SCANCODE_F1:                 return "\\{keyboard f1}";
        case SDL_SCANCODE_F2:                 return "\\{keyboard f2}";
        case SDL_SCANCODE_F3:                 return "\\{keyboard f3}";
        case SDL_SCANCODE_F4:                 return "\\{keyboard f4}";
        case SDL_SCANCODE_F5:                 return "\\{keyboard f5}";
        case SDL_SCANCODE_F6:                 return "\\{keyboard f6}";
        case SDL_SCANCODE_F7:                 return "\\{keyboard f7}";
        case SDL_SCANCODE_F8:                 return "\\{keyboard f8}";
        case SDL_SCANCODE_F9:                 return "\\{keyboard f9}";
        case SDL_SCANCODE_F10:                return "\\{keyboard f10}";
        case SDL_SCANCODE_F11:                return "\\{keyboard f11}";
        case SDL_SCANCODE_F12:                return "\\{keyboard f12}";
        case SDL_SCANCODE_F13:                return "\\{keyboard f13}";
        case SDL_SCANCODE_F14:                return "\\{keyboard f14}";
        case SDL_SCANCODE_F15:                return "\\{keyboard f15}";
        case SDL_SCANCODE_F16:                return "\\{keyboard f16}";
        case SDL_SCANCODE_F17:                return "\\{keyboard f17}";
        case SDL_SCANCODE_F18:                return "\\{keyboard f18}";
        case SDL_SCANCODE_F19:                return "\\{keyboard f19}";
        case SDL_SCANCODE_F20:                return "\\{keyboard f20}";
        case SDL_SCANCODE_F21:                return "\\{keyboard f21}";
        case SDL_SCANCODE_F22:                return "\\{keyboard f22}";
        case SDL_SCANCODE_F23:                return "\\{keyboard f23}";
        case SDL_SCANCODE_F24:                return "\\{keyboard f24}";

        case SDL_SCANCODE_NUMLOCKCLEAR:       return "\\{keyboard num_lock}";
        case SDL_SCANCODE_KP_0:               return "\\{keyboard num_0}";
        case SDL_SCANCODE_KP_1:               return "\\{keyboard num_1}";
        case SDL_SCANCODE_KP_2:               return "\\{keyboard num_2}";
        case SDL_SCANCODE_KP_3:               return "\\{keyboard num_3}";
        case SDL_SCANCODE_KP_4:               return "\\{keyboard num_4}";
        case SDL_SCANCODE_KP_5:               return "\\{keyboard num_5}";
        case SDL_SCANCODE_KP_6:               return "\\{keyboard num_6}";
        case SDL_SCANCODE_KP_7:               return "\\{keyboard num_7}";
        case SDL_SCANCODE_KP_8:               return "\\{keyboard num_8}";
        case SDL_SCANCODE_KP_9:               return "\\{keyboard num_9}";
        case SDL_SCANCODE_KP_PERIOD:          return "\\{keyboard num_period}";
        case SDL_SCANCODE_KP_DIVIDE:          return "\\{keyboard num_divide}";
        case SDL_SCANCODE_KP_MULTIPLY:        return "\\{keyboard num_multiply}";
        case SDL_SCANCODE_KP_MINUS:           return "\\{keyboard num_minus}";
        case SDL_SCANCODE_KP_PLUS:            return "\\{keyboard num_plus}";
        case SDL_SCANCODE_KP_EQUALS:          return "\\{keyboard num_equals}";
        case SDL_SCANCODE_KP_EQUALSAS400:     return "\\{keyboard num_equals}";
        case SDL_SCANCODE_KP_COMMA:           return "\\{keyboard num_comma}";
        case SDL_SCANCODE_KP_ENTER:           return "\\{keyboard num_enter}";

        // extra keys
        case SDL_SCANCODE_APPLICATION:        return "MENU";
        case SDL_SCANCODE_POWER:              return "POWER";
        case SDL_SCANCODE_EXECUTE:            return "EXEC";
        case SDL_SCANCODE_HELP:               return "HELP";
        case SDL_SCANCODE_MENU:               return "MENU";
        case SDL_SCANCODE_SELECT:             return "SEL";
        case SDL_SCANCODE_STOP:               return "STOP";
        case SDL_SCANCODE_AGAIN:              return "AGAIN";
        case SDL_SCANCODE_UNDO:               return "UNDO";
        case SDL_SCANCODE_CUT:                return "CUT";
        case SDL_SCANCODE_COPY:               return "COPY";
        case SDL_SCANCODE_PASTE:              return "PASTE";
        case SDL_SCANCODE_FIND:               return "FIND";
        case SDL_SCANCODE_MUTE:               return "MUTE";
        case SDL_SCANCODE_VOLUMEUP:           return "VOLUP";
        case SDL_SCANCODE_VOLUMEDOWN:         return "VOLDN";
        case SDL_SCANCODE_ALTERASE:           return "ALTER";
        case SDL_SCANCODE_SYSREQ:             return "SYSRQ";
        case SDL_SCANCODE_CANCEL:             return "CNCEL";
        case SDL_SCANCODE_CLEAR:              return "CLEAR";
        case SDL_SCANCODE_PRIOR:              return "PRIOR";
        case SDL_SCANCODE_RETURN2:            return "RETURN";
        case SDL_SCANCODE_SEPARATOR:          return "SEP";
        case SDL_SCANCODE_OUT:                return "OUT";
        case SDL_SCANCODE_OPER:               return "OPER";
        case SDL_SCANCODE_CLEARAGAIN:         return "CLEAR";
        case SDL_SCANCODE_CRSEL:              return "CRSEL";
        case SDL_SCANCODE_EXSEL:              return "EXSEL";
        case SDL_SCANCODE_KP_00:              return "PAD00";
        case SDL_SCANCODE_KP_000:             return "PAD000";
        case SDL_SCANCODE_THOUSANDSSEPARATOR: return "TSEP";
        case SDL_SCANCODE_DECIMALSEPARATOR:   return "DSEP";
        case SDL_SCANCODE_CURRENCYUNIT:       return "CURU";
        case SDL_SCANCODE_CURRENCYSUBUNIT:    return "CURSU";
        case SDL_SCANCODE_KP_LEFTPAREN:       return "PAD(";
        case SDL_SCANCODE_KP_RIGHTPAREN:      return "PAD)";
        case SDL_SCANCODE_KP_LEFTBRACE:       return "PAD{";
        case SDL_SCANCODE_KP_RIGHTBRACE:      return "PAD}";
        case SDL_SCANCODE_KP_TAB:             return "PADT";
        case SDL_SCANCODE_KP_BACKSPACE:       return "PADBK";
        case SDL_SCANCODE_KP_A:               return "PADA";
        case SDL_SCANCODE_KP_B:               return "PADB";
        case SDL_SCANCODE_KP_C:               return "PADC";
        case SDL_SCANCODE_KP_D:               return "PADD";
        case SDL_SCANCODE_KP_E:               return "PADE";
        case SDL_SCANCODE_KP_F:               return "PADF";
        case SDL_SCANCODE_KP_XOR:             return "PADXR";
        case SDL_SCANCODE_KP_POWER:           return "PAD^";
        case SDL_SCANCODE_KP_PERCENT:         return "PAD%";
        case SDL_SCANCODE_KP_LESS:            return "PAD<";
        case SDL_SCANCODE_KP_GREATER:         return "PAD>";
        case SDL_SCANCODE_KP_AMPERSAND:       return "PAD&";
        case SDL_SCANCODE_KP_DBLAMPERSAND:    return "PAD&&";
        case SDL_SCANCODE_KP_VERTICALBAR:     return "PAD|";
        case SDL_SCANCODE_KP_DBLVERTICALBAR:  return "PAD||";
        case SDL_SCANCODE_KP_COLON:           return "PAD:";
        case SDL_SCANCODE_KP_HASH:            return "PAD#";
        case SDL_SCANCODE_KP_SPACE:           return "PADSP";
        case SDL_SCANCODE_KP_AT:              return "PAD@";
        case SDL_SCANCODE_KP_EXCLAM:          return "PAD!";
        case SDL_SCANCODE_KP_MEMSTORE:        return "PADMS";
        case SDL_SCANCODE_KP_MEMRECALL:       return "PADMR";
        case SDL_SCANCODE_KP_MEMCLEAR:        return "PADMC";
        case SDL_SCANCODE_KP_MEMADD:          return "PADMA";
        case SDL_SCANCODE_KP_MEMSUBTRACT:     return "PADM-";
        case SDL_SCANCODE_KP_MEMMULTIPLY:     return "PADM*";
        case SDL_SCANCODE_KP_MEMDIVIDE:       return "PADM/";
        case SDL_SCANCODE_KP_PLUSMINUS:       return "PAD+-";
        case SDL_SCANCODE_KP_CLEAR:           return "PADCL";
        case SDL_SCANCODE_KP_CLEARENTRY:      return "PADCL";
        case SDL_SCANCODE_KP_BINARY:          return "PAD02";
        case SDL_SCANCODE_KP_OCTAL:           return "PAD08";
        case SDL_SCANCODE_KP_DECIMAL:         return "PAD10";
        case SDL_SCANCODE_KP_HEXADECIMAL:     return "PAD16";
        case SDL_SCANCODE_MODE:               return "MODE";
        case SDL_SCANCODE_AUDIONEXT:          return "NEXT";
        case SDL_SCANCODE_AUDIOPREV:          return "PREV";
        case SDL_SCANCODE_AUDIOSTOP:          return "STOP";
        case SDL_SCANCODE_AUDIOPLAY:          return "PLAY";
        case SDL_SCANCODE_AUDIOMUTE:          return "MUTE";
        case SDL_SCANCODE_MEDIASELECT:        return "MEDIA";
        case SDL_SCANCODE_WWW:                return "WWW";
        case SDL_SCANCODE_MAIL:               return "MAIL";
        case SDL_SCANCODE_CALCULATOR:         return "CALC";
        case SDL_SCANCODE_COMPUTER:           return "COMP";
        case SDL_SCANCODE_AC_SEARCH:          return "SRCH";
        case SDL_SCANCODE_AC_HOME:            return "HOME";
        case SDL_SCANCODE_AC_BACK:            return "BACK";
        case SDL_SCANCODE_AC_FORWARD:         return "FRWD";
        case SDL_SCANCODE_AC_STOP:            return "STOP";
        case SDL_SCANCODE_AC_REFRESH:         return "RFRSH";
        case SDL_SCANCODE_AC_BOOKMARKS:       return "BKMK";
        case SDL_SCANCODE_BRIGHTNESSDOWN:     return "BNDN";
        case SDL_SCANCODE_BRIGHTNESSUP:       return "BNUP";
        case SDL_SCANCODE_DISPLAYSWITCH:      return "DPSW";
        case SDL_SCANCODE_KBDILLUMTOGGLE:     return "KBDIT";
        case SDL_SCANCODE_KBDILLUMDOWN:       return "KBDID";
        case SDL_SCANCODE_KBDILLUMUP:         return "KBDIU";
        case SDL_SCANCODE_EJECT:              return "EJECT";
        case SDL_SCANCODE_SLEEP:              return "SLEEP";
        case SDL_SCANCODE_APP1:               return "APP1";
        case SDL_SCANCODE_APP2:               return "APP2";
        case SDL_SCANCODE_AUDIOREWIND:        return "RWND";
        case SDL_SCANCODE_AUDIOFASTFORWARD:   return "FF";
        case SDL_SCANCODE_UNKNOWN:            return nullptr;

        default:                              return "\\{keyboard unknown}";
    }
    // clang-format on
}

static bool M_Key(const INPUT_LAYOUT layout, const INPUT_ROLE role)
{
    SDL_Scancode scancode = m_Layout[layout][role];
    if (scancode == SDL_SCANCODE_RETURN && KEY_DOWN(SDL_SCANCODE_LALT)) {
        return false;
    }
#ifdef _WIN32
    if (scancode == SDL_SCANCODE_F4
        && (KEY_DOWN(SDL_SCANCODE_LALT) || KEY_DOWN(SDL_SCANCODE_RALT))) {
        return false;
    }
#endif
    if (KEY_DOWN(scancode)) {
        return true;
    }
    if (scancode == SDL_SCANCODE_LCTRL) {
        return KEY_DOWN(SDL_SCANCODE_RCTRL);
    }
    if (scancode == SDL_SCANCODE_RCTRL) {
        return KEY_DOWN(SDL_SCANCODE_LCTRL);
    }
    if (scancode == SDL_SCANCODE_LSHIFT) {
        return KEY_DOWN(SDL_SCANCODE_RSHIFT);
    }
    if (scancode == SDL_SCANCODE_RSHIFT) {
        return KEY_DOWN(SDL_SCANCODE_LSHIFT);
    }
    if (scancode == SDL_SCANCODE_LALT) {
        return KEY_DOWN(SDL_SCANCODE_RALT);
    }
    if (scancode == SDL_SCANCODE_RALT) {
        return KEY_DOWN(SDL_SCANCODE_LALT);
    }
    return false;
}

static SDL_Scancode M_GetAssignedScancode(INPUT_LAYOUT layout, INPUT_ROLE role)
{
    return m_Layout[layout][role];
}

static bool M_CheckConflict(
    const INPUT_LAYOUT layout, const INPUT_ROLE role1, const INPUT_ROLE role2)
{
    const SDL_Scancode scancode1 = M_GetAssignedScancode(layout, role1);
    const SDL_Scancode scancode2 = M_GetAssignedScancode(layout, role2);
    return scancode1 == scancode2;
}

static void M_AssignConflict(
    const INPUT_LAYOUT layout, const INPUT_ROLE role, bool conflict)
{
    m_Conflicts[layout][role] = conflict;
}

static void M_CheckConflicts(const INPUT_LAYOUT layout)
{
    Input_ConflictHelper(layout, M_CheckConflict, M_AssignConflict);
}

static void M_AssignScancode(
    const INPUT_LAYOUT layout, const INPUT_ROLE role,
    const SDL_Scancode scancode)
{
    m_Layout[layout][role] = scancode;
    M_CheckConflicts(layout);
}

static void M_ResetLayout(const INPUT_LAYOUT layout)
{
    for (INPUT_ROLE role = 0; role < INPUT_ROLE_NUMBER_OF; role++) {
        const SDL_Scancode scancode =
            M_GetAssignedScancode(INPUT_LAYOUT_DEFAULT, role);
        m_Layout[layout][role] = scancode;
    }
    M_CheckConflicts(layout);
}

static BUILTIN_KEYBOARD_LAYOUT *M_GetBuiltInLayout(const INPUT_ROLE role)
{
    for (int32_t i = 0; m_BuiltinLayout[i].role != (INPUT_ROLE)-1; i++) {
        BUILTIN_KEYBOARD_LAYOUT *const builtin = &m_BuiltinLayout[i];
        if (builtin->role == role) {
            return builtin;
        }
    }
    return nullptr;
}

static void M_HandleBuiltInDefaults(void)
{
#define L_BIND(role, code)                                                     \
    do {                                                                       \
        M_GetBuiltInLayout(role)->scancode = code;                             \
    } while (0)

    if (g_TRVersion == 2) {
        L_BIND(INPUT_ROLE_EQUIP_MAGNUMS, SDL_SCANCODE_UNKNOWN);
        L_BIND(INPUT_ROLE_EQUIP_AUTOS, SDL_SCANCODE_3);
    } else if (g_TRVersion == 3) {
        L_BIND(INPUT_ROLE_USE_SMALL_MEDI, SDL_SCANCODE_0);
        L_BIND(INPUT_ROLE_USE_BIG_MEDI, SDL_SCANCODE_9);
        L_BIND(INPUT_ROLE_EQUIP_MAGNUMS, SDL_SCANCODE_UNKNOWN);
        L_BIND(INPUT_ROLE_EQUIP_DESERT_EAGLE, SDL_SCANCODE_3);
        L_BIND(INPUT_ROLE_EQUIP_M16, SDL_SCANCODE_UNKNOWN);
        L_BIND(INPUT_ROLE_EQUIP_MP5, SDL_SCANCODE_6);
        L_BIND(INPUT_ROLE_EQUIP_ROCKET_LAUNCHER, SDL_SCANCODE_7);
        L_BIND(INPUT_ROLE_EQUIP_GRENADE_LAUNCHER, SDL_SCANCODE_8);
    }

#undef L_BIND
}

static void M_Init(void)
{
    // first, reset the roles to null
    for (INPUT_ROLE role = 0; role < INPUT_ROLE_NUMBER_OF; role++) {
        m_Layout[INPUT_LAYOUT_DEFAULT][role] = SDL_SCANCODE_UNKNOWN;
    }
    // allow specific engines to re-assign default bindings
    M_HandleBuiltInDefaults();
    // then load actually defined default bindings
    for (int32_t i = 0; m_BuiltinLayout[i].role != (INPUT_ROLE)-1; i++) {
        const BUILTIN_KEYBOARD_LAYOUT *const builtin = &m_BuiltinLayout[i];
        m_Layout[INPUT_LAYOUT_DEFAULT][builtin->role] = builtin->scancode;
    }
    M_CheckConflicts(INPUT_LAYOUT_DEFAULT);

    for (int32_t layout = INPUT_LAYOUT_CUSTOM_1;
         layout < INPUT_LAYOUT_NUMBER_OF; layout++) {
        M_ResetLayout(layout);
    }
}

static bool M_CustomUpdate(INPUT_STATE *const result, const INPUT_LAYOUT layout)
{
    // we only do this for keyboard input
    result->menu_confirm |= result->action;
    result->toggle_fullscreen =
        KEY_DOWN(SDL_SCANCODE_RETURN) && KEY_DOWN(SDL_SCANCODE_LALT);
    result->menu_skip = result->menu_confirm || result->menu_back;
    return true;
}

static bool M_IsPressed(const INPUT_LAYOUT layout, const INPUT_ROLE role)
{
    return M_Key(layout, role);
}

static bool M_IsRoleConflicted(const INPUT_LAYOUT layout, const INPUT_ROLE role)
{
    return m_Conflicts[layout][role];
}

static const char *M_GetName(const INPUT_LAYOUT layout, const INPUT_ROLE role)
{
    return M_GetScancodeName(m_Layout[layout][role]);
}

static void M_UnassignRole(const INPUT_LAYOUT layout, const INPUT_ROLE role)
{
    M_AssignScancode(layout, role, SDL_SCANCODE_UNKNOWN);
}

static bool M_AssignFromJSONObject(
    const INPUT_LAYOUT layout, const INPUT_ROLE role,
    JSON_OBJECT *const bind_obj)
{
    const SDL_Scancode default_scancode = M_GetAssignedScancode(layout, role);
    const SDL_Scancode user_scancode =
        JSON_ObjectGetInt(bind_obj, "scancode", default_scancode);
    M_AssignScancode(layout, role, user_scancode);
    return true;
}

static bool M_AssignToJSONObject(
    const INPUT_LAYOUT layout, const INPUT_ROLE role,
    JSON_OBJECT *const bind_obj)
{
    const SDL_Scancode default_scancode =
        M_GetAssignedScancode(INPUT_LAYOUT_DEFAULT, role);
    const SDL_Scancode user_scancode = M_GetAssignedScancode(layout, role);

    if (user_scancode == default_scancode) {
        return false;
    }

    JSON_ObjectAppendInt(bind_obj, "scancode", user_scancode);
    return true;
}

static bool M_ReadAndAssign(const INPUT_LAYOUT layout, const INPUT_ROLE role)
{
    for (SDL_Scancode scancode = 0; scancode < SDL_NUM_SCANCODES; scancode++) {
        if (KEY_DOWN(scancode)) {
            M_AssignScancode(layout, role, scancode);
            return true;
        }
    }
    return false;
}

INPUT_BACKEND_IMPL g_Input_Keyboard = {
    .init = M_Init,
    .shutdown = nullptr,
    .discover = nullptr,
    .process_event = M_ProcessEvent,
    .custom_update = M_CustomUpdate,
    .is_pressed = M_IsPressed,
    .is_role_conflicted = M_IsRoleConflicted,
    .get_name = M_GetName,
    .unassign_role = M_UnassignRole,
    .assign_from_json_object = M_AssignFromJSONObject,
    .assign_to_json_object = M_AssignToJSONObject,
    .reset_layout = M_ResetLayout,
    .read_and_assign = M_ReadAndAssign,
};
