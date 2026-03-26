#pragma once

// Define a game string mapping: associates an ID with a text value.
// @param id    Compile-time identifier for the string.
// @param value Text to map to the identifier.
#define GS_DEFINE(id, value) GameString_Define(#id, value);

// Retrieve a game string by a slash-separated path literal.
#define GS(path) GameString_Get(path)

// Return a slash-separated path literal as-is.
#define GS_ID(path) (path)

// Retrieve a stable slot pointer for a slash-separated path literal.
#define GS_PTR(path) GameString_GetPtr(path)

typedef const char *GAME_STRING_ID;

// Initialize the GameString subsystem.
// Must be called before any GameString_* functions.
void GameString_Init(void);

// Shutdown the GameString subsystem and free all associated resources.
void GameString_Shutdown(void);

// Define a new game string mapping.
// @param key   Identifier for the string (e.g. "GAME_OVER").
// @param value Text to associate with the identifier.
void GameString_Define(const char *key, const char *value);

// Check whether a game string identifier has been defined.
// @param key   Identifier to test.
// @return      true if the identifier is known, false otherwise.
bool GameString_IsKnown(const char *key);

// Retrieve the game string associated with an identifier.
// @param key   Identifier for the string.
// @return      Text mapped to the identifier, or nullptr if unknown.
const char *GameString_Get(const char *key);

// Clear all existing game string mappings.
// Slots remain allocated but values reset.
void GameString_Clear(void);

// Like GameString_Get(), but returns a stable slot pointer that always
// reflects the current string value for this identifier (updates automatically
// on reload).
// @param key   Identifier for the string.
// @return      Address of the internal string pointer, or nullptr if unknown.
const char *const *GameString_GetPtr(const char *key);
