#include <trx/game/input/backends/controller.h>

#include <trx/core/log.h>
#include <trx/game/input/backends/internal.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_gamecontroller.h>

typedef enum {
    BT_BUTTON = 0,
    BT_AXIS = 1,
} BUTTON_TYPE;

typedef struct {
    BUTTON_TYPE type;
    union {
        SDL_GameControllerButton button;
        SDL_GameControllerAxis axis;
    } bind;
    int16_t axis_dir;
} CONTROLLER_MAP;

typedef struct {
    INPUT_ROLE role;
    CONTROLLER_MAP map;
} BUILTIN_CONTROLLER_LAYOUT;

static BUILTIN_CONTROLLER_LAYOUT m_BuiltinLayout[] = {
#define INPUT_CONTROLLER_ASSIGN_BUTTON(role, bind)                             \
    { role, { BT_BUTTON, { .button = bind }, 0 } },
#define INPUT_CONTROLLER_ASSIGN_AXIS(role, bind, axis_dir)                     \
    { role, { BT_AXIS, { .axis = bind }, axis_dir } },
#include <trx/game/input/backends/controller.def>
    // guard
    { -1, { 0, { 0 }, 0 } },
};

// clang-format off
#define M_ICON_X              "\\{controller button cross}"
#define M_ICON_CIRCLE         "\\{controller button circle}"
#define M_ICON_SQUARE         "\\{controller button square}"
#define M_ICON_TRIANGLE       "\\{controller button triangle}"
#define M_ICON_UP             "\\{controller dpad up}"
#define M_ICON_DOWN           "\\{controller dpad down}"
#define M_ICON_LEFT           "\\{controller dpad left}"
#define M_ICON_RIGHT          "\\{controller dpad right}"
#define M_ICON_L1             "\\{controller button l1}"
#define M_ICON_R1             "\\{controller button r1}"
#define M_ICON_L2             "\\{controller button l2}"
#define M_ICON_R2             "\\{controller button r2}"

#define M_NAME_L_STICK        "\\{controller lstick}"
#define M_NAME_L_ANALOG_UP    "\\{controller lstick up}"
#define M_NAME_L_ANALOG_RIGHT "\\{controller lstick right}"
#define M_NAME_L_ANALOG_DOWN  "\\{controller lstick down}"
#define M_NAME_L_ANALOG_LEFT  "\\{controller lstick left}"
#define M_NAME_R_STICK        "\\{controller rstick}"
#define M_NAME_R_ANALOG_UP    "\\{controller rstick up}"
#define M_NAME_R_ANALOG_RIGHT "\\{controller rstick right}"
#define M_NAME_R_ANALOG_DOWN  "\\{controller rstick down}"
#define M_NAME_R_ANALOG_LEFT  "\\{controller rstick left}"
#define M_NAME_L_TRIGGER      "\\{controller trigger left}"
#define M_NAME_R_TRIGGER      "\\{controller trigger right}"

#define M_NAME_ZL             "\\{controller button zl}"
#define M_NAME_ZR             "\\{controller button zr}"

#define M_NAME_XBOX           "\\{controller button xbox}"
#define M_NAME_PS             "\\{controller button ps}"
#define M_NAME_PS_SHARE       "\\{controller button share}"
#define M_NAME_PS_OPTIONS     "\\{controller button options}"

#define M_NAME_BACK           "\\{controller button back}"
#define M_NAME_HOME           "\\{controller button home}"
#define M_NAME_START          "\\{controller button home}"
#define M_NAME_SHARE          "\\{controller button share}"
#define M_NAME_CAPTURE        "\\{controller button capture}"
#define M_NAME_TOUCHPAD       "\\{controller button touchpad}"
#define M_NAME_MIC            "\\{controller button mic}"
#define M_NAME_PADDLE_1       "\\{controller button paddle 1}"
#define M_NAME_PADDLE_2       "\\{controller button paddle 2}"
#define M_NAME_PADDLE_3       "\\{controller button paddle 3}"
#define M_NAME_PADDLE_4       "\\{controller button paddle 4}"

#define M_NAME_L_BUMPER       "\\{controller bumper left}"
#define M_NAME_R_BUMPER       "\\{controller bumper right}"

#define M_NAME_A              "\\{controller button a}"
#define M_NAME_B              "\\{controller button b}"
#define M_NAME_X              "\\{controller button x}"
#define M_NAME_Y              "\\{controller button y}"
// clang-format on

static CONTROLLER_MAP m_Layout[INPUT_LAYOUT_NUMBER_OF][INPUT_ROLE_NUMBER_OF];

static SDL_GameController *m_Controller = nullptr;
static const char *m_ControllerName = nullptr;
static SDL_GameControllerType m_ControllerType = SDL_CONTROLLER_TYPE_UNKNOWN;

static bool m_Conflicts[INPUT_LAYOUT_NUMBER_OF][INPUT_ROLE_NUMBER_OF] = {};

// Internal controller state tables updated via SDL events
static bool m_ButtonState[SDL_CONTROLLER_BUTTON_MAX] = {};
static int16_t m_AxisState[SDL_CONTROLLER_AXIS_MAX] = {};

static const char *M_GetButtonName(const SDL_GameControllerButton button)
{
    // First switch: Handle platform-specific deviations from defaults
    switch (m_ControllerType) {
    case SDL_CONTROLLER_TYPE_PS3:
    case SDL_CONTROLLER_TYPE_PS4:
    case SDL_CONTROLLER_TYPE_PS5:
        // clang-format off
        switch (button) {
        case SDL_CONTROLLER_BUTTON_A:             return M_ICON_X;
        case SDL_CONTROLLER_BUTTON_B:             return M_ICON_CIRCLE;
        case SDL_CONTROLLER_BUTTON_X:             return M_ICON_SQUARE;
        case SDL_CONTROLLER_BUTTON_Y:             return M_ICON_TRIANGLE;
        case SDL_CONTROLLER_BUTTON_BACK:          return M_NAME_PS_OPTIONS;
        case SDL_CONTROLLER_BUTTON_START:         return M_NAME_PS_SHARE;
        case SDL_CONTROLLER_BUTTON_LEFTSTICK:     return M_NAME_L_STICK;
        case SDL_CONTROLLER_BUTTON_RIGHTSTICK:    return M_NAME_R_STICK;
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:  return M_ICON_L1;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return M_ICON_R1;
        case SDL_CONTROLLER_BUTTON_MISC1:         return M_NAME_MIC;
        case SDL_CONTROLLER_BUTTON_GUIDE:         return M_NAME_PS;
        default: break;
        }
        // clang-format on
        break;

    case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO:
    case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
        // clang-format off
        switch (button) {
        case SDL_CONTROLLER_BUTTON_A:             return M_NAME_B;
        case SDL_CONTROLLER_BUTTON_B:             return M_NAME_A;
        case SDL_CONTROLLER_BUTTON_X:             return M_NAME_Y;
        case SDL_CONTROLLER_BUTTON_Y:             return M_NAME_X;
        case SDL_CONTROLLER_BUTTON_START:         return M_NAME_START;
        case SDL_CONTROLLER_BUTTON_MISC1:         return M_NAME_CAPTURE;
        default: break;
        }
        // clang-format on
        break;

    case SDL_CONTROLLER_TYPE_XBOX360:
    case SDL_CONTROLLER_TYPE_XBOXONE:
        // clang-format off
        switch (button) {
        case SDL_CONTROLLER_BUTTON_GUIDE:         return M_NAME_XBOX;
        default: break;
        }
        // clang-format on
        break;

    default:
        break;
    }

    // Second switch: Provide default mappings for all keys
    switch (button) {
    case SDL_CONTROLLER_BUTTON_INVALID:
    case SDL_CONTROLLER_BUTTON_MAX:
        return nullptr;

        // clang-format off
    case SDL_CONTROLLER_BUTTON_A:             return M_NAME_A;
    case SDL_CONTROLLER_BUTTON_B:             return M_NAME_B;
    case SDL_CONTROLLER_BUTTON_X:             return M_NAME_X;
    case SDL_CONTROLLER_BUTTON_Y:             return M_NAME_Y;
    case SDL_CONTROLLER_BUTTON_BACK:          return M_NAME_BACK;
    case SDL_CONTROLLER_BUTTON_GUIDE:         return M_NAME_HOME;
    case SDL_CONTROLLER_BUTTON_START:         return M_NAME_START;
    case SDL_CONTROLLER_BUTTON_LEFTSTICK:     return M_NAME_L_STICK;
    case SDL_CONTROLLER_BUTTON_RIGHTSTICK:    return M_NAME_R_STICK;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:  return M_NAME_L_BUMPER;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return M_NAME_R_BUMPER;
    case SDL_CONTROLLER_BUTTON_DPAD_UP:       return M_ICON_UP;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:     return M_ICON_DOWN;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:     return M_ICON_LEFT;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:    return M_ICON_RIGHT;
    case SDL_CONTROLLER_BUTTON_MISC1:         return M_NAME_SHARE;
    case SDL_CONTROLLER_BUTTON_PADDLE1:       return M_NAME_PADDLE_1;
    case SDL_CONTROLLER_BUTTON_PADDLE2:       return M_NAME_PADDLE_2;
    case SDL_CONTROLLER_BUTTON_PADDLE3:       return M_NAME_PADDLE_3;
    case SDL_CONTROLLER_BUTTON_PADDLE4:       return M_NAME_PADDLE_4;
    case SDL_CONTROLLER_BUTTON_TOUCHPAD:      return M_NAME_TOUCHPAD;
        // clang-format on

    default:
        return "????";
    }
}

// Update internal controller button/axis state from SDL events.
// @param event     Event to process.
static void M_ProcessEvent(const SDL_Event *const event)
{
    switch (event->type) {
    case SDL_CONTROLLERBUTTONDOWN:
        m_ButtonState[event->cbutton.button] = true;
        break;
    case SDL_CONTROLLERBUTTONUP:
        m_ButtonState[event->cbutton.button] = false;
        break;
    case SDL_CONTROLLERAXISMOTION: {
        const Sint16 value = event->caxis.value;
        if (value < -SDL_JOYSTICK_AXIS_MAX / 2) {
            m_AxisState[event->caxis.axis] = -1;
        } else if (value > SDL_JOYSTICK_AXIS_MAX / 2) {
            m_AxisState[event->caxis.axis] = 1;
        } else {
            m_AxisState[event->caxis.axis] = 0;
        }
        break;
    }
    default:
        break;
    }
}

static bool M_JoyBtn(const SDL_GameControllerButton button)
{
    if (m_Controller == nullptr || button == SDL_CONTROLLER_BUTTON_INVALID) {
        return false;
    }
    return m_ButtonState[button];
}

static int16_t M_JoyAxis(const SDL_GameControllerAxis axis)
{
    if (m_Controller == nullptr || axis == SDL_CONTROLLER_AXIS_INVALID) {
        return false;
    }
    return m_AxisState[axis];
}

static bool M_GetBindState(const INPUT_LAYOUT layout, const INPUT_ROLE role)
{
    const CONTROLLER_MAP assigned = m_Layout[layout][role];
    if (assigned.type == BT_BUTTON) {
        return M_JoyBtn(assigned.bind.button);
    } else {
        return M_JoyAxis(assigned.bind.axis) == assigned.axis_dir;
    }
}

static int16_t M_GetUniqueBind(const INPUT_LAYOUT layout, const INPUT_ROLE role)
{
    const CONTROLLER_MAP assigned = m_Layout[layout][role];
    if (assigned.type == BT_AXIS) {
        // Add SDL_CONTROLLER_BUTTON_MAX as an axis offset because button and
        // axis enum values overlap. Also offset depending on axis direction.
        if (assigned.axis_dir == -1) {
            return assigned.bind.axis + SDL_CONTROLLER_BUTTON_MAX;
        } else {
            return assigned.bind.axis + SDL_CONTROLLER_BUTTON_MAX + 10;
        }
    }
    return assigned.bind.button;
}

static bool M_CheckConflict(
    const INPUT_LAYOUT layout, const INPUT_ROLE role1, const INPUT_ROLE role2)
{
    const int16_t bind1 = M_GetUniqueBind(layout, role1);
    const int16_t bind2 = M_GetUniqueBind(layout, role2);
    return bind1 == bind2;
}

static void M_AssignConflict(
    const INPUT_LAYOUT layout, const INPUT_ROLE role, const bool conflict)
{
    m_Conflicts[layout][role] = conflict;
}

static void M_CheckConflicts(const INPUT_LAYOUT layout)
{
    Input_ConflictHelper(layout, M_CheckConflict, M_AssignConflict);
}

static int16_t M_GetAssignedButtonType(
    const INPUT_LAYOUT layout, const INPUT_ROLE role)
{
    return m_Layout[layout][role].type;
}

static int16_t M_GetAssignedBind(
    const INPUT_LAYOUT layout, const INPUT_ROLE role)
{
    const CONTROLLER_MAP assigned = m_Layout[layout][role];
    if (assigned.type == BT_BUTTON) {
        return assigned.bind.button;
    } else {
        return assigned.bind.axis;
    }
}

static int16_t M_GetAssignedAxisDir(
    const INPUT_LAYOUT layout, const INPUT_ROLE role)
{
    return m_Layout[layout][role].axis_dir;
}

static void M_AssignButton(
    const INPUT_LAYOUT layout, const INPUT_ROLE role, const int16_t button)
{
    m_Layout[layout][role].type = BT_BUTTON;
    m_Layout[layout][role].bind.button = button;
    m_Layout[layout][role].axis_dir = 0;
    M_CheckConflicts(layout);
}

static void M_AssignAxis(
    const INPUT_LAYOUT layout, const INPUT_ROLE role, const int16_t axis,
    const int16_t axis_dir)
{
    m_Layout[layout][role].type = BT_AXIS;
    m_Layout[layout][role].bind.axis = axis;
    m_Layout[layout][role].axis_dir = axis_dir;
    M_CheckConflicts(layout);
}

static SDL_GameController *M_FindController(void)
{
    if (m_Controller != nullptr) {
        return m_Controller;
    }

    int32_t controllers = SDL_NumJoysticks();
    LOG_INFO("%d controllers", controllers);
    for (int32_t i = 0; i < controllers; i++) {
        m_ControllerName = SDL_GameControllerNameForIndex(i);
        m_ControllerType = SDL_GameControllerTypeForIndex(i);
        bool is_game_controller = SDL_IsGameController(i);
        LOG_DEBUG(
            "controller %d: %s %d (%d)", i, m_ControllerName, m_ControllerType,
            is_game_controller);
        if (is_game_controller) {
            SDL_GameController *const result = SDL_GameControllerOpen(i);
            if (result == nullptr) {
                LOG_ERROR("Could not open controller: %s", SDL_GetError());
            }
            return result;
        }
    }

    return nullptr;
}

static void M_ResetLayout(const INPUT_LAYOUT layout)
{
    for (INPUT_ROLE role = 0; role < INPUT_ROLE_NUMBER_OF; role++) {
        const CONTROLLER_MAP default_btn = m_Layout[INPUT_LAYOUT_DEFAULT][role];
        m_Layout[layout][role] = default_btn;
    }
    M_CheckConflicts(layout);
}

static void M_Discover(void)
{
    if (m_Controller != nullptr) {
        SDL_GameControllerClose(m_Controller);
        m_Controller = nullptr;
    }
    m_Controller = M_FindController();
}

static void M_Init(void)
{
    // first, reset the roles to null
    for (INPUT_ROLE role = 0; role < INPUT_ROLE_NUMBER_OF; role++) {
        m_Layout[INPUT_LAYOUT_DEFAULT][role] = (CONTROLLER_MAP) {
            BT_BUTTON, { SDL_CONTROLLER_BUTTON_INVALID }, 0
        };
    }
    // then load actually defined default bindings
    for (int32_t i = 0; m_BuiltinLayout[i].role != (INPUT_ROLE)-1; i++) {
        const BUILTIN_CONTROLLER_LAYOUT *const builtin = &m_BuiltinLayout[i];
        m_Layout[INPUT_LAYOUT_DEFAULT][builtin->role] = builtin->map;
    }
    M_CheckConflicts(INPUT_LAYOUT_DEFAULT);

    for (int32_t layout = INPUT_LAYOUT_CUSTOM_1;
         layout < INPUT_LAYOUT_NUMBER_OF; layout++) {
        M_ResetLayout(layout);
    }

    int32_t result = SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_SENSOR);
    if (result < 0) {
        LOG_ERROR("Error while calling SDL_Init: 0x%lx", result);
    } else {
        M_Discover();
    }
}

static void M_Shutdown(void)
{
    if (m_Controller != nullptr) {
        SDL_GameControllerClose(m_Controller);
        m_Controller = nullptr;
    }
}

static bool M_CustomUpdate(INPUT_STATE *const result, const INPUT_LAYOUT layout)
{
    if (m_Controller == nullptr) {
        return false;
    }
    result->menu_back |= M_JoyBtn(SDL_CONTROLLER_BUTTON_Y);
    result->menu_skip = result->menu_confirm || result->menu_back;
    return true;
}

static bool M_IsPressed(const INPUT_LAYOUT layout, const INPUT_ROLE role)
{
    return M_GetBindState(layout, role);
}

static const char *M_GetAxisName(
    const SDL_GameControllerAxis axis, const int16_t axis_dir)
{
    // clang-format off
    switch (m_ControllerType) {
        case SDL_CONTROLLER_TYPE_PS3:
        case SDL_CONTROLLER_TYPE_PS4:
        case SDL_CONTROLLER_TYPE_PS5:
            switch (axis) {
                case SDL_CONTROLLER_AXIS_INVALID:         return nullptr;
                case SDL_CONTROLLER_AXIS_LEFTX:           return axis_dir == -1 ? M_NAME_L_ANALOG_LEFT : M_NAME_L_ANALOG_RIGHT;
                case SDL_CONTROLLER_AXIS_LEFTY:           return axis_dir == -1 ? M_NAME_L_ANALOG_UP : M_NAME_L_ANALOG_DOWN;
                case SDL_CONTROLLER_AXIS_RIGHTX:          return axis_dir == -1 ? M_NAME_R_ANALOG_LEFT : M_NAME_R_ANALOG_RIGHT;
                case SDL_CONTROLLER_AXIS_RIGHTY:          return axis_dir == -1 ? M_NAME_R_ANALOG_UP : M_NAME_R_ANALOG_DOWN;
                case SDL_CONTROLLER_AXIS_TRIGGERLEFT:     return M_ICON_L2;
                case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:    return M_ICON_R2;
                case SDL_CONTROLLER_AXIS_MAX:             return nullptr;
            }
            break;

        case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO:
        case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
            switch (axis) {
                case SDL_CONTROLLER_AXIS_INVALID:         return nullptr;
                case SDL_CONTROLLER_AXIS_LEFTX:           return axis_dir == -1 ? M_NAME_L_ANALOG_LEFT : M_NAME_L_ANALOG_RIGHT;
                case SDL_CONTROLLER_AXIS_LEFTY:           return axis_dir == -1 ? M_NAME_L_ANALOG_UP : M_NAME_L_ANALOG_DOWN;
                case SDL_CONTROLLER_AXIS_RIGHTX:          return axis_dir == -1 ? M_NAME_R_ANALOG_LEFT : M_NAME_R_ANALOG_RIGHT;
                case SDL_CONTROLLER_AXIS_RIGHTY:          return axis_dir == -1 ? M_NAME_R_ANALOG_UP : M_NAME_R_ANALOG_DOWN;
                case SDL_CONTROLLER_AXIS_TRIGGERLEFT:     return M_NAME_ZL;
                case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:    return M_NAME_ZR;
                case SDL_CONTROLLER_AXIS_MAX:             return nullptr;
            }
            break;

        case SDL_CONTROLLER_TYPE_XBOX360:
        case SDL_CONTROLLER_TYPE_XBOXONE:
        default:
            switch (axis) {
                case SDL_CONTROLLER_AXIS_INVALID:         return nullptr;
                case SDL_CONTROLLER_AXIS_LEFTX:           return axis_dir == -1 ? M_NAME_L_ANALOG_LEFT : M_NAME_L_ANALOG_RIGHT;
                case SDL_CONTROLLER_AXIS_LEFTY:           return axis_dir == -1 ? M_NAME_L_ANALOG_UP : M_NAME_L_ANALOG_DOWN;
                case SDL_CONTROLLER_AXIS_RIGHTX:          return axis_dir == -1 ? M_NAME_R_ANALOG_LEFT : M_NAME_R_ANALOG_RIGHT;
                case SDL_CONTROLLER_AXIS_RIGHTY:          return axis_dir == -1 ? M_NAME_R_ANALOG_UP : M_NAME_R_ANALOG_DOWN;
                case SDL_CONTROLLER_AXIS_TRIGGERLEFT:     return M_NAME_L_TRIGGER;
                case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:    return M_NAME_R_TRIGGER;
                case SDL_CONTROLLER_AXIS_MAX:             return nullptr;
            }
            break;

    }
    // clang-format on
    return nullptr;
}

static bool M_IsRoleConflicted(const INPUT_LAYOUT layout, const INPUT_ROLE role)
{
    return m_Conflicts[layout][role];
}

static const char *M_GetName(const INPUT_LAYOUT layout, const INPUT_ROLE role)
{
    const CONTROLLER_MAP check = m_Layout[layout][role];
    if (check.type == BT_BUTTON) {
        return M_GetButtonName(check.bind.button);
    } else {
        return M_GetAxisName(check.bind.axis, check.axis_dir);
    }
}

static void M_UnassignRole(const INPUT_LAYOUT layout, const INPUT_ROLE role)
{
    M_AssignButton(layout, role, -1);
}

static bool M_AssignFromJSONObject(
    const INPUT_LAYOUT layout, const INPUT_ROLE role,
    JSON_OBJECT *const bind_obj)
{
    int16_t button_type = M_GetAssignedButtonType(layout, role);
    button_type = JSON_ObjectGetInt(bind_obj, "button_type", button_type);

    int16_t bind = M_GetAssignedBind(layout, role);
    bind = JSON_ObjectGetInt(bind_obj, "bind", bind);

    int16_t axis_dir = M_GetAssignedAxisDir(layout, role);
    axis_dir = JSON_ObjectGetInt(bind_obj, "axis_dir", axis_dir);

    if (button_type == BT_BUTTON) {
        M_AssignButton(layout, role, bind);
    } else {
        M_AssignAxis(layout, role, bind, axis_dir);
    }
    return true;
}

static bool M_AssignToJSONObject(
    const INPUT_LAYOUT layout, const INPUT_ROLE role,
    JSON_OBJECT *const bind_obj)
{
    const int16_t default_button_type =
        M_GetAssignedButtonType(INPUT_LAYOUT_DEFAULT, role);
    const int16_t default_axis_dir =
        M_GetAssignedAxisDir(INPUT_LAYOUT_DEFAULT, role);
    const int16_t default_bind = M_GetAssignedBind(INPUT_LAYOUT_DEFAULT, role);

    const int16_t user_button_type = M_GetAssignedButtonType(layout, role);
    const int16_t user_axis_dir = M_GetAssignedAxisDir(layout, role);
    const int16_t user_bind = M_GetAssignedBind(layout, role);

    if (user_button_type == default_button_type
        && user_axis_dir == default_axis_dir && user_bind == default_bind) {
        return false;
    }

    JSON_ObjectAppendInt(bind_obj, "button_type", user_button_type);
    JSON_ObjectAppendInt(bind_obj, "bind", user_bind);
    JSON_ObjectAppendInt(bind_obj, "axis_dir", user_axis_dir);
    return true;
}

static bool M_ReadAndAssign(const INPUT_LAYOUT layout, const INPUT_ROLE role)
{
    for (SDL_GameControllerButton button = 0;
         button < SDL_CONTROLLER_BUTTON_MAX; button++) {
        if (M_JoyBtn(button)) {
            M_AssignButton(layout, role, button);
            return true;
        }
    }
    for (SDL_GameControllerAxis axis = 0; axis < SDL_CONTROLLER_AXIS_MAX;
         axis++) {
        int16_t axis_dir = M_JoyAxis(axis);
        if (axis_dir != 0) {
            M_AssignAxis(layout, role, axis, axis_dir);
            return true;
        }
    }
    return false;
}

INPUT_BACKEND_IMPL g_Input_Controller = {
    .init = M_Init,
    .shutdown = M_Shutdown,
    .discover = M_Discover,
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
