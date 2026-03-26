#pragma once

#include <trx/config/enum.h>
#include <trx/core/json.h>

#include <SDL2/SDL_events.h>
#include <stdint.h>

typedef enum {
#define X_INPUT_ROLE(role_name, state_name) role_name,
#include <trx/game/input/roles.def>
    INPUT_ROLE_NUMBER_OF,
#undef X_INPUT_ROLE
} INPUT_ROLE;

#define INPUT_STATE_ANY_WORDS ((INPUT_ROLE_NUMBER_OF + 63) / 64)

typedef union {
    uint64_t any[INPUT_STATE_ANY_WORDS];
    struct {
#define X_INPUT_ROLE(role_name, state_name) uint64_t state_name : 1;
#include <trx/game/input/roles.def>
#undef X_INPUT_ROLE
    };
} INPUT_STATE;

extern INPUT_STATE g_Input;
extern INPUT_STATE g_InputDB;
extern INPUT_STATE g_OldInputDB;

void Input_Init(void);
void Input_Shutdown(void);
void Input_Discover(void);
void Input_Update(void);

// Processes a SDL event to update global input state before polling.
// @param event     Event to process.
void Input_ProcessEvent(const SDL_Event *event);

// Checks whether the given role can be assigned to by the player.
// Hard-coded roles are exempt from conflict checks (eg will never flash in the
// controls dialog).
bool Input_IsRoleRebindable(INPUT_ROLE role);

// Checks whether the given role can be completely unbound by the player.
bool Input_IsRoleUnbindable(INPUT_ROLE role);

// Returns whether the key assigned to the given role is also used elsewhere
// within the custom layout.
bool Input_IsKeyConflicted(
    INPUT_BACKEND backend, INPUT_LAYOUT layout, INPUT_ROLE role);

// Checks if the key is currently pressed. Tied to Input_Update(), so updates
// at most at the game running FPS.
bool Input_IsPressed(INPUT_ROLE role);

// Checks if the key is currently pressed with a debounce, e.g. only true
// for the game frame the player starts to hold the key at.
bool Input_IsPressedDB(INPUT_ROLE role);

// Given the input layout and input key role, check if the assorted key is
// pressed, bypassing Input_Update.
bool Input_IsPressedEx(
    INPUT_BACKEND backend, INPUT_LAYOUT layout, INPUT_ROLE role);

// If there is anything pressed, assigns the pressed key to the given key role
// and returns true. If nothing is pressed, immediately returns false.
bool Input_ReadAndAssignRole(
    INPUT_BACKEND backend, INPUT_LAYOUT layout, INPUT_ROLE role);

// Remove assigned key from a given key role.
void Input_UnassignRole(
    INPUT_BACKEND backend, INPUT_LAYOUT layout, INPUT_ROLE role);

// Get a stable pointer to the layout human-readable name.
const char *const *Input_GetLayoutNamePtr(const INPUT_LAYOUT layout);

// Given the input layout and input key role, get the assigned key name.
const char *Input_GetKeyName(
    INPUT_BACKEND backend, INPUT_LAYOUT layout, INPUT_ROLE role);

// Reset a given layout to the default.
void Input_ResetLayout(INPUT_BACKEND backend, INPUT_LAYOUT layout);

// Disables updating g_Input.
void Input_EnterListenMode(void);

// Enables updating g_Input.
void Input_ExitListenMode(void);

// Checks whether updates are disabled.
bool Input_IsInListenMode(void);

// Restores the user configuration by converting the JSON object back into the
// original input layout.
bool Input_AssignFromJSONObject(
    INPUT_BACKEND backend, INPUT_LAYOUT layout, JSON_OBJECT *bind_obj);

// Converts the original input layout into a JSON object for storing the user
// configuration.
bool Input_AssignToJSONObject(
    INPUT_BACKEND backend, INPUT_LAYOUT layout, JSON_OBJECT *bind_obj,
    INPUT_ROLE role);

INPUT_STATE Input_GetDebounced(const INPUT_STATE input);

const char *Input_GetRoleName(INPUT_ROLE role);

// Serialize a scancode and modifier mask into a human-readable key
// description, e.g. "ctrl+shift+up". The returned string must not be held onto.
const char *Input_KeyDescFromSDL(SDL_Scancode scancode, SDL_Keymod mod);

// Parse a human-readable key description into scancode and modifier mask.
// e.g. "ctrl+shift+up" → scancode SDL_SCANCODE_UP, mod KMOD_CTRL|KMOD_SHIFT.
// Returns true if parsing succeeded, false otherwise.
bool Input_ParseKeyDesc(
    const char *desc, SDL_Scancode *scancode, SDL_Keymod *mod);

void InputState_Clear(INPUT_STATE *state);
void InputState_Copy(INPUT_STATE *dst, INPUT_STATE src);
bool InputState_IsAnyPressed(INPUT_STATE state);
