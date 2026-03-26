#include <trx/core/log.h>
#include <trx/core/memory.h>
#include <trx/debug.h>
#include <trx/game/game_flow.h>
#include <trx/game/game_strings/entries.h>
#include <trx/game/game_strings/table.h>
#include <trx/game/game_strings/table/priv.h>
#include <trx/game/objects/names.h>
#include <trx/game/shell.h>

#include <string.h>

typedef void (*M_LOAD_STRING_FUNC)(const char *, const char *);

static VECTOR *m_GST_Layers = nullptr;

static void M_Apply(const GS_TABLE *const table)
{
    for (const GS_GAME_STRING_ENTRY *cur = table->game_strings;
         cur != nullptr && cur->key != nullptr; cur++) {
        if (cur->value == nullptr) {
            LOG_ERROR("Invalid game string value: %s", cur->key);
        } else {
            GameString_Define(cur->key, cur->value);
        }
    }

    for (const GS_OBJECT_ENTRY *cur = table->objects;
         cur != nullptr && cur->key != nullptr; cur++) {
        const OBJECT_ID obj_id = Object_IdFromKey(cur->key);
        if (obj_id == NO_OBJECT) {
            LOG_ERROR("Invalid object id: %s", cur->key);
        } else if (cur->names == nullptr) {
            LOG_ERROR("Invalid object name(s): %s", cur->key);
        } else {
            Object_ClearNames(obj_id);
            for (const char *const *name = cur->names; *name != nullptr;
                 name++) {
                Object_AddName(obj_id, *name);
            }
            Object_SetDescription(obj_id, cur->description);
        }
    }
}

static void M_ApplyLevelTitles(
    const GS_FILE *const gs_file, const GF_LEVEL_TABLE_TYPE level_table_type)
{
    const GF_LEVEL_TABLE *const level_table =
        GF_GetLevelTable(level_table_type);
    const GS_LEVEL_TABLE *const gs_level_table =
        &gs_file->level_tables[level_table_type];
    if (gs_level_table->count == 0) {
        return;
    }
    ASSERT(gs_level_table->count == level_table->count);
    for (int32_t i = 0; i < level_table->count; i++) {
        if (gs_level_table->entries[i].title != nullptr) {
            GF_SetLevelTitle(
                &level_table->levels[i], gs_level_table->entries[i].title);
        }
    }
}

static void M_ApplyLayer(
    const GF_LEVEL *const level, const GS_FILE *const gs_file)
{
    LOG_DEBUG("applying layer: %s", gs_file->path);
    M_Apply(&gs_file->global);

    for (int32_t i = 0; i < GFLT_NUMBER_OF; i++) {
        M_ApplyLevelTitles(gs_file, i);
    }

    if (level != nullptr) {
        const GS_LEVEL_TABLE *gs_level_table = nullptr;
        switch (level->type) {
        case GFL_TITLE:
            gs_level_table = nullptr;
            break;
        default: {
            const GF_LEVEL_TABLE_TYPE level_table_type =
                GF_GetLevelTableType(level->type);
            gs_level_table = &gs_file->level_tables[level_table_type];
        }
        }

        if (gs_level_table != nullptr && gs_level_table->count != 0) {
            ASSERT(level->num >= 0);
            ASSERT(level->num < gs_level_table->count);
            M_Apply(&gs_level_table->entries[level->num].table);
        }
    }
}

void GameStringTable_Apply(const GF_LEVEL *const level)
{
    Object_ResetAllNames();
    ASSERT(m_GST_Layers != nullptr);
    for (int32_t i = 0; i < m_GST_Layers->count; i++) {
        const GS_FILE *const gs_file = *(GS_FILE **)Vector_Get(m_GST_Layers, i);
        M_ApplyLayer(level, gs_file);
    }
}

void GameStringTable_Init(void)
{
    m_GST_Layers = Vector_Create(sizeof(GS_FILE *));
}

void GameStringTable_Shutdown(void)
{
    if (m_GST_Layers != nullptr) {
        for (int32_t i = 0; i < m_GST_Layers->count; i++) {
            GS_FILE *const gs_file = *(GS_FILE **)Vector_Get(m_GST_Layers, i);
            GS_File_Free(gs_file);
        }
        Vector_Free(m_GST_Layers);
        m_GST_Layers = nullptr;
    }
}

bool GameStringTable_Load(const char *const path, const bool load_levels)
{
    GS_FILE *gs_file = GS_File_CreateFromPath(path, load_levels);
    if (gs_file == nullptr) {
        return false;
    }
    ASSERT(m_GST_Layers != nullptr);
    Vector_Add(m_GST_Layers, &gs_file);
    return true;
}
