#pragma once

#include <trx/core/filesystem.h>
#include <trx/core/json.h>
#include <trx/core/json/util/read_io.h>
#include <trx/core/json/util/write_io.h>
#include <trx/game/savegame/types.h>

#include <stdint.h>

const char *SG_File_GetSaveFilePattern(void);
const char *SG_File_GetQuickSaveFilePattern(void);
bool SG_File_FillInfo(MYFILE *fp, SAVEGAME_INFO *info);
bool SG_File_LoadFromFile(MYFILE *fp);
bool SG_File_LoadOnlyResumeInfo(MYFILE *fp);
void SG_File_SaveToFile(MYFILE *fp, SAVEGAME_INFO *info);
bool SG_File_UpdateDeathCounters(
    MYFILE *fp, int32_t level_num, int32_t death_count, bool is_quick);

// Start of reader functions ===================================================
bool SG_File_LoadLara(JSON_READ_IO *io);
bool SG_File_LoadInventory(JSON_READ_IO *io);
bool SG_File_LoadFlipmaps(JSON_READ_IO *io);
bool SG_File_LoadCameras(JSON_READ_IO *io);
bool SG_File_LoadItems(JSON_READ_IO *io);
bool SG_File_LoadEffects(JSON_READ_IO *io);
bool SG_File_LoadFX(JSON_READ_IO *io);
bool SG_File_LoadFlares(JSON_READ_IO *io);
bool SG_File_LoadMusic(JSON_READ_IO *io);
bool SG_File_LoadResumeInfoList(JSON_READ_IO *io);
bool SG_File_LoadMisc(JSON_READ_IO *io);
// End of reader functions =====================================================

// Start of writer functions ===================================================
void SG_File_DumpFlares(JSON_WRITE_IO *io);
void SG_File_DumpEffects(JSON_WRITE_IO *io);
void SG_File_DumpInventory(JSON_WRITE_IO *io);
void SG_File_DumpFlipmaps(JSON_WRITE_IO *io);
void SG_File_DumpCameras(JSON_WRITE_IO *io);
void SG_File_DumpMusic(JSON_WRITE_IO *io);
void SG_File_DumpItems(JSON_WRITE_IO *io);
void SG_File_DumpFX(JSON_WRITE_IO *io);
void SG_File_DumpLara(JSON_WRITE_IO *io);
void SG_File_DumpResumeInfoList(JSON_WRITE_IO *io);
void SG_File_DumpMisc(JSON_WRITE_IO *io);
// End of writer functions =====================================================

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;
    int16_t initial_version;
    uint16_t version;
    int32_t compressed_size;
    int32_t uncompressed_size;
} SAVEGAME_BSON_HEADER;

typedef struct {
    uint32_t flags;
    int32_t counter;
    int32_t level_num;
    int32_t title_size;
} SAVEGAME_BSON_EXTENDED_HEADER;
#pragma pack(pop)

#define SAVEGAME_EXT_FLAG_QUICK (1U << 31)
