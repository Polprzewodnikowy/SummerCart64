#include <trx/core/virtual_file.h>

#include <trx/core/filesystem.h>
#include <trx/core/log.h>
#include <trx/core/memory.h>
#include <trx/debug.h>

#include <string.h>

VFILE *VFile_CreateFromPath(const char *const path)
{
    MYFILE *fp = File_Open(path, FILE_OPEN_READ);
    if (!fp) {
        LOG_ERROR("Can't open file %s", path);
        return nullptr;
    }

    const size_t data_size = File_Size(fp);
    char *data = Memory_Alloc(data_size);
    File_ReadData(fp, data, data_size);
    if (File_Pos(fp) != data_size) {
        LOG_ERROR("Can't read file %s", path);
        Memory_FreePointer(&data);
        File_Close(fp);
        return nullptr;
    }
    File_Close(fp);

    VFILE *const file = Memory_Alloc(sizeof(VFILE));
    file->content = data;
    file->size = data_size;
    file->cur_ptr = file->content;
    return file;
}

VFILE *VFile_CreateFromBuffer(const char *data, size_t size)
{
    VFILE *const file = Memory_Alloc(sizeof(VFILE));
    file->content = Memory_Dup(data, size);
    file->size = size;
    file->cur_ptr = file->content;
    return file;
}

void VFile_Close(VFILE *file)
{
    ASSERT(file != nullptr);
    Memory_FreePointer(&file->content);
    Memory_FreePointer(&file);
}

size_t VFile_GetPos(const VFILE *file)
{
    return file->cur_ptr - file->content;
}

void VFile_SetPos(VFILE *const file, const size_t pos)
{
    ASSERT(pos <= file->size);
    file->cur_ptr = file->content + pos;
}

void VFile_Skip(VFILE *const file, const int32_t offset)
{
    ASSERT(VFile_TrySkip(file, offset));
}

bool VFile_TrySkip(VFILE *const file, const int32_t offset)
{
    const size_t cur_pos = VFile_GetPos(file);
    if (cur_pos + offset > file->size) {
        return false;
    }
    file->cur_ptr += offset;
    return true;
}

void VFile_Read(VFILE *const file, void *const target, const size_t size)
{
    ASSERT(VFile_TryRead(file, target, size));
}

bool VFile_TryRead(VFILE *const file, void *const target, const size_t size)
{
    ASSERT(file != nullptr);
    if (size == 0) {
        return true;
    }
    ASSERT(target != nullptr);

    const size_t cur_pos = VFile_GetPos(file);
    if (cur_pos + size > file->size) {
        return false;
    }
    memcpy(target, file->cur_ptr, size);
    file->cur_ptr += size;
    return true;
}

int8_t VFile_ReadS8(VFILE *file)
{
    int8_t result;
    VFile_Read(file, &result, sizeof(result));
    return result;
}

int16_t VFile_ReadS16(VFILE *file)
{
    int16_t result;
    VFile_Read(file, &result, sizeof(result));
    return result;
}

int32_t VFile_ReadS32(VFILE *file)
{
    int32_t result;
    VFile_Read(file, &result, sizeof(result));
    return result;
}

uint8_t VFile_ReadU8(VFILE *file)
{
    uint8_t result;
    VFile_Read(file, &result, sizeof(result));
    return result;
}

uint16_t VFile_ReadU16(VFILE *file)
{
    uint16_t result;
    VFile_Read(file, &result, sizeof(result));
    return result;
}

uint32_t VFile_ReadU32(VFILE *file)
{
    uint32_t result;
    VFile_Read(file, &result, sizeof(result));
    return result;
}

#define DEFINE_TRY_READ(name, type)                                            \
    bool name(VFILE *const file, type *const dst)                              \
    {                                                                          \
        return VFile_TryRead(file, dst, sizeof(type));                         \
    }

DEFINE_TRY_READ(VFile_TryReadS8, int8_t)
DEFINE_TRY_READ(VFile_TryReadS16, int16_t)
DEFINE_TRY_READ(VFile_TryReadS32, int32_t)
DEFINE_TRY_READ(VFile_TryReadU8, uint8_t)
DEFINE_TRY_READ(VFile_TryReadU16, uint16_t)
DEFINE_TRY_READ(VFile_TryReadU32, uint32_t)
