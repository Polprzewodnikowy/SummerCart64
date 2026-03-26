// Manages autodiscovery and runtime reload of string bundles per language and
// mod/OG fallback.
#pragma once

#include <trx/core/event_manager.h>
#include <trx/core/vector.h>

// Initialize the string bundle manager.
void GameStringManager_Init(void);

// Shutdown the string bundle manager.
void GameStringManager_Shutdown(void);

// Clear all previously set source strings files.
// Must be called before GameStringManager_AddSourceFile.
void GameStringManager_ClearSourceFiles(void);

// Add a source strings file for language discovery and loading.
// base_path: path to a base strings JSON5 file (e.g. cfg/common_strings.json5).
// load_levels: true to load level names from this source; false otherwise.
void GameStringManager_AddSourceFile(const char *base_path, bool load_levels);

// Discover all available languages from the added source files.
// Must be called after AddSourceFile calls and before ReloadLanguage.
void GameStringManager_DiscoverLanguages(void);

// Returns a vector of char* of available language codes discovered.
// Caller owns the returned VECTOR and must free it via Vector_Free and free
// each string.
VECTOR *GameStringManager_GetAvailableLanguages(void);

// Get the display name for a language code as defined by "language_name"
// in the strings JSON file. Returns nullptr if unavailable.
// The returned pointer is owned by the manager; do not free.
const char *GameStringManager_GetLanguageName(const char *code);

// Reload all game strings for the given language code.
// Clears any previously loaded strings, loads OG fallback and mod overrides.
// lang: language code (e.g. "en", "fr"). Must be one returned by
// GetAvailableLanguages.
bool GameStringManager_ReloadLanguage(const char *lang);

// Subscribe to be notified when the game strings language is reloaded.
// The listener will receive an EVENT with name "reload_language",
// and .data pointing to the language code (const char *).
int32_t GameStringManager_SubscribeReload(
    EVENT_LISTENER listener, void *user_data);

// Unsubscribe from language reload events.
void GameStringManager_UnsubscribeReload(int32_t listener_id);
