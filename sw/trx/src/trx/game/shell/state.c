#include <trx/game/shell/state.h>

#include <trx/core/filesystem.h>
#include <trx/core/json.h>
#include <trx/core/json/util/file.h>
#include <trx/core/memory.h>
#include <trx/core/strings.h>
#include <trx/game/shell.h>

#include <string.h>

static char *m_LastPlayedMod = nullptr;

#define M_LAST_PLAYED_MOD_KEY "last_played_mod"

__attribute__((destructor)) static void M_Shutdown(void)
{
    Memory_FreePointer(&m_LastPlayedMod);
}

static const char *M_GetStatePath(void)
{
    return String_FormatStatic("%s/shell.json5", Shell_GetConfigDir());
}

static void M_LoadState(void)
{
    if (m_LastPlayedMod != nullptr) {
        return;
    }

    if (!File_Exists(M_GetStatePath())) {
        return;
    }

    JSON_VALUE *const root = JSONFile_Read(M_GetStatePath());
    if (root == nullptr) {
        return;
    }

    JSON_OBJECT *const root_obj = JSON_ValueAsObject(root);
    const char *const mod_name =
        JSON_ObjectGetString(root_obj, M_LAST_PLAYED_MOD_KEY, nullptr);
    if (mod_name != nullptr) {
        m_LastPlayedMod = Memory_DupStr(mod_name);
    }

    JSON_ValueFree(root);
}

const char *ShellState_GetLastPlayedMod(void)
{
    M_LoadState();
    return m_LastPlayedMod;
}

void ShellState_RememberLastPlayedMod(const char *const mod_name)
{
    if (mod_name == nullptr) {
        return;
    }

    M_LoadState();
    if (m_LastPlayedMod != nullptr && strcmp(m_LastPlayedMod, mod_name) == 0) {
        return;
    }

    Memory_FreePointer(&m_LastPlayedMod);
    m_LastPlayedMod = Memory_DupStr(mod_name);

    JSON_OBJECT *const root_obj = JSON_ObjectNew();
    JSON_ObjectAppendString(root_obj, M_LAST_PLAYED_MOD_KEY, m_LastPlayedMod);

    JSON_VALUE *const root = JSON_ValueFromObject(root_obj);
    const char *const state_path = M_GetStatePath();
    File_EnsureParentDirectories(state_path);
    if (File_Exists(state_path)) {
        JSONFile_Write(state_path, root);
    } else {
        size_t out_len = 0;
        char *out_data = JSON_WritePretty(root, "  ", "\n", &out_len);
        MYFILE *const fp = File_Open(state_path, FILE_OPEN_WRITE);
        if (fp != nullptr) {
            File_WriteData(fp, out_data, out_len - 1); // w/o \0
            File_Close(fp);
        }
        Memory_FreePointer(&out_data);
    }
    JSON_ValueFree(root);
}
