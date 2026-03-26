#include <trx/config.h>
#include <trx/core/memory.h>
#include <trx/core/strings.h>
#include <trx/game/game_strings/manager.h>
#include <trx/game/ui/dialogs/setting_helpers/handlers.h>

#include <stdlib.h>

static VECTOR *m_Languages = nullptr;

static void M_Language_Cleanup(void)
{
    // Free the languages vector and its strings.
    if (m_Languages != nullptr) {
        for (int32_t i = 0; i < m_Languages->count; i++) {
            char *lang = *(char **)Vector_Get(m_Languages, i);
            Memory_Free(lang);
        }
        Vector_Free(m_Languages);
        m_Languages = nullptr;
    }
}

static const VECTOR *M_Language_GetLanguages(void)
{
    if (m_Languages == nullptr) {
        // Initialize available languages for the language option.
        m_Languages = GameStringManager_GetAvailableLanguages();
        atexit(M_Language_Cleanup);
    }
    return m_Languages;
}

static int32_t M_Language_FindIndex(const UI_SETTINGS_OPTION *const option)
{
    const VECTOR *const langs = M_Language_GetLanguages();
    const char *const cur = *(char **)option->target;
    for (int32_t i = 0; i < langs->count; i++) {
        const char *const lang = *(char **)Vector_Get(langs, i);
        if (String_Equivalent(lang, cur)) {
            return i;
        }
    }
    return -1;
}

const char *UI_Settings_Language_FormatValue(
    const UI_SETTINGS_OPTION *const option)
{
    const char *const code = *(const char **)option->target;
    const char *const name = GameStringManager_GetLanguageName(code);
    return name != nullptr ? name : code;
}

bool UI_Settings_Language_CanChangeValue(
    const UI_SETTINGS_OPTION *const option, const int32_t dir)
{
    const VECTOR *const langs = M_Language_GetLanguages();
    const int32_t idx = M_Language_FindIndex(option);
    if (idx < 0) {
        // If the language from the user config somehow is no longer on the list
        // (the file was deleted), let the player return to the default language
        return true;
    }
    if (langs->count < 2) {
        return false;
    }
    return idx + dir >= 0 && idx + dir < langs->count;
}

bool UI_Settings_Language_RequestChangeValue(
    const UI_SETTINGS_OPTION *const option, const int32_t dir)
{
    const VECTOR *const langs = M_Language_GetLanguages();
    if (!UI_Settings_Language_CanChangeValue(option, dir)) {
        return false;
    }
    const char *new_lang;
    const int32_t idx = M_Language_FindIndex(option);
    if (idx != -1) {
        new_lang = *(char **)Vector_Get(langs, idx + dir);
    } else {
        // If the language from the user config somehow is no longer on the list
        // (the file was deleted), default to the first entry, which is English
        new_lang = *(char **)Vector_Get(langs, 0);
    }
    Config_SetOptionValueFromString(Config_GetOption(option->target), new_lang);
    GameStringManager_ReloadLanguage(new_lang);
    return true;
}
