#include <trx/game/savegame/file.h>

#include <trx/core/bson.h>
#include <trx/core/log.h>
#include <trx/core/memory.h>
#include <trx/core/strings.h>
#include <trx/core/utils.h>
#include <trx/game/game.h>
#include <trx/game/game_flow.h>
#include <trx/game/savegame.h>
#include <trx/game/shell.h>
#include <trx/version.h>

#include <string.h>
#include <zconf.h>
#include <zlib.h>

#define M_MAGIC_TR1X MKTAG('T', '1', 'M', 'B') // TOOD: remove me after TRX 1.5
#define M_MAGIC_TR2X MKTAG('T', '2', 'X', 'B') // TOOD: remove me after TRX 1.5
#define M_MAGIC_TRX MKTAG('T', 'R', 'X', 'S')

#define M_MUST(x)                                                              \
    if (!(x)) {                                                                \
        goto fail;                                                             \
    }

static JSON_VALUE *M_ReadRaw(MYFILE *fp, int32_t *version_out);

const char *SG_File_GetSaveFilePattern(void)
{
    return g_GameFlow.savegame_file_fmt;
}

const char *SG_File_GetQuickSaveFilePattern(void)
{
    const char *const pattern = SG_File_GetSaveFilePattern();
    const char *const placeholder = strchr(pattern, '%');
    if (placeholder == nullptr) {
        return String_FormatStatic("%s_q", pattern);
    }
    const int32_t prefix_size = placeholder - pattern;
    return String_FormatStatic("%.*sq%s", prefix_size, pattern, placeholder);
}

static JSON_VALUE *M_ParseFromBuffer(
    const char *const buffer, int32_t *const version_out)
{
    const SAVEGAME_BSON_HEADER *const header = (SAVEGAME_BSON_HEADER *)buffer;
    if (header->magic != M_MAGIC_TR1X && header->magic != M_MAGIC_TR2X
        && header->magic != M_MAGIC_TRX) {
        LOG_ERROR("Invalid savegame magic");
        return nullptr;
    }

    if (version_out != nullptr) {
        *version_out = header->version;
    }

    const char *const compressed = buffer + sizeof(SAVEGAME_BSON_HEADER);
    char *uncompressed = Memory_Alloc(header->uncompressed_size);

    uLongf uncompressed_size = header->uncompressed_size;
    const int32_t error_code = uncompress(
        (Bytef *)uncompressed, &uncompressed_size, (const Bytef *)compressed,
        (uLongf)header->compressed_size);
    if (error_code != Z_OK) {
        LOG_ERROR("Failed to decompress the data (error %d)", error_code);
        Memory_FreePointer(&uncompressed);
        return nullptr;
    }

    JSON_VALUE *const root = BSON_Parse(uncompressed, uncompressed_size);
    Memory_FreePointer(&uncompressed);
    return root;
}

static JSON_VALUE *M_ReadRaw(MYFILE *const fp, int32_t *const version_out)
{
    const size_t buffer_size = File_Size(fp);
    char *buffer = Memory_Alloc(buffer_size);
    File_Seek(fp, 0, FILE_SEEK_SET);
    File_ReadData(fp, buffer, buffer_size);

    JSON_VALUE *const result = M_ParseFromBuffer(buffer, version_out);
    Memory_FreePointer(&buffer);
    return result;
}

static void M_SaveRaw(
    MYFILE *const fp, const JSON_VALUE *const root, const int32_t level_num,
    const bool is_quick)
{
    size_t uncompressed_size;
    char *uncompressed = BSON_Write(root, &uncompressed_size);

    uLongf compressed_size = compressBound(uncompressed_size);
    char *compressed = Memory_Alloc(compressed_size);
    const int32_t result = compress(
        (Bytef *)compressed, &compressed_size, (const Bytef *)uncompressed,
        (uLongf)uncompressed_size);
    if (result != Z_OK) {
        Shell_ExitSystem("Failed to compress savegame data");
    }

    const GF_LEVEL *const level = GF_GetLevel(GFLT_MAIN, level_num);
    const JSON_OBJECT *const root_obj = JSON_ValueAsObject(root);
    const SAVEGAME_BSON_HEADER header = {
        .magic = M_MAGIC_TRX,
        .initial_version = Savegame_GetInitialVersion(),
        .version = SG_CURRENT_VERSION,
        .compressed_size = compressed_size,
        .uncompressed_size = uncompressed_size,
    };
    const SAVEGAME_BSON_EXTENDED_HEADER extra_header = {
        .flags = Game_GetBonusFlag() | (is_quick ? SAVEGAME_EXT_FLAG_QUICK : 0),
        .counter =
            JSON_ObjectGetInt(root_obj, "save_counter", Savegame_GetCounter()),
        .level_num = level->num,
        .title_size = level->title != nullptr ? strlen(level->title) : 0,
    };

    File_WriteData(fp, &header, sizeof(header));
    File_WriteData(fp, compressed, compressed_size);
    File_WriteData(fp, &extra_header, sizeof(extra_header));
    File_WriteData(
        fp, level->title, level->title != nullptr ? strlen(level->title) : 0);

    Memory_FreePointer(&uncompressed);
    Memory_FreePointer(&compressed);
}

bool SG_File_LoadFromFile(MYFILE *const fp)
{
    bool result = false;

    int32_t sg_version = -1;
    JSON_VALUE *const root = M_ReadRaw(fp, &sg_version);
    JSON_READ_IO *const io = JSON_ReadIO_Create(root, sg_version, nullptr);

    M_MUST(SG_File_LoadResumeInfoList(io));
    M_MUST(SG_File_LoadMisc(io));
    M_MUST(SG_File_LoadInventory(io));
    M_MUST(SG_File_LoadFlipmaps(io));
    M_MUST(SG_File_LoadCameras(io));
    M_MUST(SG_File_LoadItems(io));
    M_MUST(SG_File_LoadEffects(io));
    M_MUST(SG_File_LoadFX(io));
    M_MUST(SG_File_LoadFlares(io));
    M_MUST(SG_File_LoadMusic(io));
    M_MUST(SG_File_LoadLara(io));

    result = true;

fail:
    JSON_ReadIO_Destroy(io);
    JSON_ValueFree(root);
    return result;
}

void SG_File_SaveToFile(MYFILE *const fp, SAVEGAME_INFO *const info)
{
    const GF_LEVEL *const current_level = Game_GetCurrentLevel();
    JSON_WRITE_IO *const io = JSON_WriteIO_Create();

    SG_File_DumpResumeInfoList(io);
    SG_File_DumpInventory(io);
    SG_File_DumpFlipmaps(io);
    SG_File_DumpCameras(io);
    SG_File_DumpItems(io);
    SG_File_DumpEffects(io);
    SG_File_DumpFX(io);
    SG_File_DumpLara(io);
    SG_File_DumpMusic(io);
    SG_File_DumpFlares(io);
    SG_File_DumpMisc(io);

    M_SaveRaw(
        fp, JSON_WriteIO_GetRoot(io), current_level->num,
        info != nullptr && info->is_quick);
    JSON_WriteIO_Destroy(io);
}

bool SG_File_FillInfo(MYFILE *const fp, SAVEGAME_INFO *const info)
{
    *info = (SAVEGAME_INFO) {};

    SAVEGAME_BSON_HEADER header = {};
    File_Seek(fp, 0, FILE_SEEK_SET);
    if (!File_ReadData(fp, &header, sizeof(SAVEGAME_BSON_HEADER))) {
        return false;
    }
    if (header.magic != M_MAGIC_TR1X && header.magic != M_MAGIC_TR2X
        && header.magic != M_MAGIC_TRX) {
        return false;
    }

    if (header.version < SG_MIN_SUPPORTED_VERSION) {
        LOG_WARNING(
            "Too old SG version: %d (min supported: %d)", header.version,
            SG_MIN_SUPPORTED_VERSION);
        return false;
    }
    info->initial_version = header.initial_version;
    info->features.restart = header.initial_version >= SG_VERSION_LEGACY;
    info->features.select_level = header.initial_version >= SG_VERSION_1;

    // recover the slot information from the end of the file
    File_Skip(fp, header.compressed_size);
    SAVEGAME_BSON_EXTENDED_HEADER extra_header;
    if (File_ReadData(fp, &extra_header, sizeof(extra_header))) {
        info->counter = extra_header.counter;
        info->level_num = extra_header.level_num;
        info->is_quick = (extra_header.flags & SAVEGAME_EXT_FLAG_QUICK) != 0;
        info->level_title = Memory_Alloc(extra_header.title_size + 1);
        File_ReadData(fp, info->level_title, extra_header.title_size);
        return true;
    }

    // recover the slot information from the savegame structures
    bool result = false;
    File_Seek(fp, 0, FILE_SEEK_SET);
    JSON_VALUE *root = M_ReadRaw(fp, nullptr);
    JSON_OBJECT *root_obj = JSON_ValueAsObject(root);
    if (root_obj != nullptr) {
        info->counter = JSON_ObjectGetInt(root_obj, "save_counter", -1);
        info->level_num = JSON_ObjectGetInt(root_obj, "level_num", -1);
        const char *level_title =
            JSON_ObjectGetString(root_obj, "level_title", nullptr);
        if (level_title != nullptr) {
            info->level_title = Memory_DupStr(level_title);
        }
        result = info->level_num != -1;
    }
    JSON_ValueFree(root);

    return result;
}

bool SG_File_LoadOnlyResumeInfo(MYFILE *const fp)
{
    int32_t sg_version = -1;
    JSON_VALUE *const root = M_ReadRaw(fp, &sg_version);
    JSON_READ_IO *const io = JSON_ReadIO_Create(root, sg_version, nullptr);
    const bool result = SG_File_LoadResumeInfoList(io);
    JSON_ReadIO_Destroy(io);
    JSON_ValueFree(root);
    return result;
}

bool SG_File_UpdateDeathCounters(
    MYFILE *const fp, int32_t level_num, const int32_t death_count,
    const bool is_quick)
{
    bool result = false;
    JSON_VALUE *const root = M_ReadRaw(fp, nullptr);
    JSON_OBJECT *const root_obj = JSON_ValueAsObject(root);
    if (root_obj == nullptr) {
        LOG_ERROR("Cannot find the root object");
        goto cleanup;
    }

    JSON_OBJECT *const misc_obj = JSON_ObjectGetObject(root_obj, "misc");
    if (misc_obj == nullptr) {
        LOG_ERROR("Cannot find the misc object");
        goto cleanup;
    }
    JSON_ObjectEvictKey(misc_obj, "death_count");
    JSON_ObjectAppendInt(misc_obj, "death_count", death_count);

    const GF_LEVEL_TABLE *const level_table = GF_GetLevelTable(GFLT_MAIN);
    int32_t resume_idx = -1;
    for (int32_t i = 0; i < level_table->count; i++) {
        if (level_table->levels[i].num == level_num) {
            resume_idx = i;
            break;
        }
    }

    JSON_ARRAY *const resume_arr = JSON_ObjectGetArray(root_obj, "resume_info");
    if (resume_arr != nullptr && resume_idx != -1) {
        JSON_OBJECT *const resume_obj =
            JSON_ArrayGetObject(resume_arr, resume_idx);
        if (resume_obj != nullptr) {
            JSON_ObjectEvictKey(resume_obj, "death_count");
            JSON_ObjectAppendInt(resume_obj, "death_count", death_count);
        }
    }

    File_Seek(fp, 0, FILE_SEEK_SET);
    M_SaveRaw(fp, root, level_num, is_quick);
    result = true;

cleanup:
    JSON_ValueFree(root);
    return result;
}
