#include <trx/game/console/history.h>

#include <trx/core/json/util/file.h>
#include <trx/core/memory.h>
#include <trx/core/strings.h>
#include <trx/core/utils.h>
#include <trx/core/vector.h>
#include <trx/game/shell.h>
#include <trx/version.h>

#include <string.h>

#define MAX_HISTORY_ENTRIES 30

VECTOR *m_History = nullptr;

static const char *M_GetPath(void)
{
    return String_FormatStatic(
        "%s/TR%dX_console_history.json5", Shell_GetConfigDir(), g_TRVersion);
}

static void M_LoadFromJSON(JSON_VALUE *const doc)
{
    JSON_OBJECT *const root_obj = JSON_ValueAsObject(doc);
    JSON_ARRAY *const arr = JSON_ObjectGetArray(root_obj, "entries");
    if (arr == nullptr) {
        return;
    }

    Console_History_Clear();
    for (size_t i = 0; i < arr->length; i++) {
        const char *const line = JSON_ArrayGetString(arr, i, nullptr);
        if (line != nullptr) {
            Console_History_Append(line);
        }
    }
}

static JSON_VALUE *M_DumpToJSON(void)
{
    JSON_ARRAY *const arr = JSON_ArrayNew();
    for (int32_t i = 0; i < Console_History_GetLength(); i++) {
        JSON_ArrayAppendString(arr, Console_History_Get(i));
    }

    if (arr->length == 0) {
        JSON_ArrayFree(arr);
        return nullptr;
    }
    JSON_OBJECT *root_obj = JSON_ObjectNew();
    JSON_ObjectAppendArray(root_obj, "entries", arr);
    return JSON_ValueFromObject(root_obj);
}

void Console_History_Init(void)
{
    m_History = Vector_Create(sizeof(char *));
    JSON_VALUE *const doc = JSONFile_Read(M_GetPath());
    if (doc != nullptr) {
        M_LoadFromJSON(doc);
        JSON_ValueFree(doc);
    }
}

void Console_History_Shutdown(void)
{
    if (m_History == nullptr) {
        return;
    }

    JSON_VALUE *const doc = M_DumpToJSON();
    if (doc != nullptr) {
        JSONFile_Write(M_GetPath(), doc);
        JSON_ValueFree(doc);
    }

    for (int32_t i = m_History->count - 1; i >= 0; i--) {
        char *const prompt = *(char **)Vector_Get(m_History, i);
        Memory_Free(prompt);
    }
    Vector_Free(m_History);
    m_History = nullptr;
}

int32_t Console_History_GetLength(void)
{
    return m_History->count;
}

void Console_History_Clear(void)
{
    for (int32_t i = m_History->count - 1; i >= 0; i--) {
        char *const prompt = *(char **)Vector_Get(m_History, i);
        Memory_Free(prompt);
    }
    Vector_Clear(m_History);
}

void Console_History_Append(const char *const prompt)
{
    for (int32_t i = m_History->count - 1; i >= 0; i--) {
        char *const entry = *(char **)Vector_Get(m_History, i);
        if (strcmp(entry, prompt) == 0) {
            Memory_Free(entry);
            Vector_RemoveAt(m_History, i);
        }
    }

    if (m_History->count == MAX_HISTORY_ENTRIES) {
        char *const oldest = *(char **)Vector_Get(m_History, 0);
        Memory_Free(oldest);
        Vector_RemoveAt(m_History, 0);
    }

    char *const prompt_copy = Memory_DupStr(prompt);
    Vector_Add(m_History, &prompt_copy);
}

const char *Console_History_Get(const int32_t idx)
{
    if (idx < 0 || idx >= m_History->count) {
        return nullptr;
    }
    const char *const prompt = *(char **)Vector_Get(m_History, idx);
    return prompt;
}
