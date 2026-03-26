#include <trx/core/filesystem.h>

#include <trx/core/log.h>
#include <trx/core/memory.h>
#include <trx/core/strings.h>
#include <trx/core/utils.h>
#include <trx/debug.h>

#include <dirent.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
    #include <direct.h>
    #include <io.h>
    #include <sys/stat.h>
    #define PATH_SEPARATOR "\\"
#else
    #include <sys/stat.h>
    #include <unistd.h>
    #define PATH_SEPARATOR "/"
#endif

struct MYFILE {
    FILE *fp;
    const char *path;
};

#if defined(_WIN32)
    #include <wchar.h>
    #include <stdlib.h>
    #include <string.h>
    #include <windows.h>

static wchar_t *M_UTF8ToWide(const char *const utf8_str)
{
    if (utf8_str == nullptr) {
        return nullptr;
    }
    const size_t len = strlen(utf8_str);
    const size_t wide_len =
        MultiByteToWideChar(CP_UTF8, 0, utf8_str, len, nullptr, 0);
    wchar_t *wide_str = Memory_Alloc((wide_len + 1) * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, utf8_str, len, wide_str, wide_len);
    wide_str[wide_len] = L'\0';
    return wide_str;
}

static FILE *M_UTF8Fopen(const char *path, const char *mode)
{
    if (path == nullptr || mode == nullptr) {
        return nullptr;
    }
    wchar_t *const wide_path = M_UTF8ToWide(path);
    wchar_t *const wide_mode = M_UTF8ToWide(mode);
    FILE *const file = _wfopen(wide_path, wide_mode);
    Memory_Free(wide_path);
    Memory_Free(wide_mode);
    return file;
}

#else
static FILE *M_UTF8Fopen(const char *path, const char *mode)
{
    if (path == nullptr || mode == nullptr) {
        return nullptr;
    }
    return fopen(path, mode);
}
#endif

static bool M_ExistsRaw(const char *path)
{
    if (path == nullptr) {
        return false;
    }
    FILE *fp = M_UTF8Fopen(path, "rb");
    if (fp) {
        fclose(fp);
        return true;
    }
    return false;
}

bool File_IsAbsolute(const char *path)
{
    return path && (path[0] == '/' || strstr(path, ":\\"));
}

bool File_IsRelative(const char *path)
{
    return path && !File_IsAbsolute(path);
}

bool File_DirExists(const char *path)
{
    if (path == nullptr) {
        return false;
    }
    DIR *dir = opendir(path);
    if (dir != nullptr) {
        closedir(dir);
        return true;
    }
    return false;
}

bool File_Exists(const char *path)
{
    if (path == nullptr) {
        return false;
    }
    return M_ExistsRaw(path);
}

char *File_GetParentDirectory(const char *path)
{
    if (path == nullptr) {
        return nullptr;
    }

    char *last_delim = MAX(strrchr(path, '/'), strrchr(path, '\\'));
    if (last_delim != nullptr) {
        return String_Format("%.*s", last_delim - path, path);
    }

    return nullptr;
}

MYFILE *File_Open(const char *path, FILE_OPEN_MODE mode)
{
    MYFILE *file = Memory_Alloc(sizeof(MYFILE));
    file->path = Memory_DupStr(path);
    switch (mode) {
    case FILE_OPEN_WRITE:
        file->fp = M_UTF8Fopen(path, "wb");
        break;
    case FILE_OPEN_READ:
        file->fp = M_UTF8Fopen(path, "rb");
        break;
    case FILE_OPEN_READ_WRITE:
        file->fp = M_UTF8Fopen(path, "r+b");
        break;
    default:
        file->fp = nullptr;
        break;
    }
    if (file->fp == nullptr) {
        Memory_FreePointer(&file->path);
        Memory_FreePointer(&file);
    }
    return file;
}

bool File_ReadData(MYFILE *const file, void *const data, const size_t size)
{
    return fread(data, size, 1, file->fp) == 1;
}

bool File_ReadItems(
    MYFILE *const file, void *data, const size_t count, const size_t item_size)
{
    return fread(data, item_size, count, file->fp) == count;
}

int8_t File_ReadS8(MYFILE *const file)
{
    int8_t result;
    File_ReadData(file, &result, sizeof(result));
    return result;
}

int16_t File_ReadS16(MYFILE *const file)
{
    int16_t result;
    File_ReadData(file, &result, sizeof(result));
    return result;
}

int32_t File_ReadS32(MYFILE *const file)
{
    int32_t result;
    File_ReadData(file, &result, sizeof(result));
    return result;
}

uint8_t File_ReadU8(MYFILE *const file)
{
    uint8_t result;
    File_ReadData(file, &result, sizeof(result));
    return result;
}

uint16_t File_ReadU16(MYFILE *const file)
{
    uint16_t result;
    File_ReadData(file, &result, sizeof(result));
    return result;
}

uint32_t File_ReadU32(MYFILE *const file)
{
    uint32_t result;
    File_ReadData(file, &result, sizeof(result));
    return result;
}

void File_WriteData(
    MYFILE *const file, const void *const data, const size_t size)
{
    fwrite(data, size, 1, file->fp);
}

void File_WriteItems(
    MYFILE *const file, const void *const data, const size_t count,
    const size_t item_size)
{
    fwrite(data, item_size, count, file->fp);
}

void File_WriteS8(MYFILE *const file, const int8_t value)
{
    fwrite(&value, sizeof(value), 1, file->fp);
}

void File_WriteS16(MYFILE *const file, const int16_t value)
{
    fwrite(&value, sizeof(value), 1, file->fp);
}

void File_WriteS32(MYFILE *const file, const int32_t value)
{
    fwrite(&value, sizeof(value), 1, file->fp);
}

void File_WriteU8(MYFILE *const file, const uint8_t value)
{
    fwrite(&value, sizeof(value), 1, file->fp);
}

void File_WriteU16(MYFILE *const file, const uint16_t value)
{
    fwrite(&value, sizeof(value), 1, file->fp);
}

void File_WriteU32(MYFILE *const file, const uint32_t value)
{
    fwrite(&value, sizeof(value), 1, file->fp);
}

void File_WriteString(MYFILE *file, const char *fmt, ...)
{
    if (file == nullptr || file->fp == nullptr) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    const char *s = String_FormatStaticV(fmt, args);
    va_end(args);
    fputs(s, file->fp);
}

void File_Skip(MYFILE *file, size_t bytes)
{
    File_Seek(file, bytes, FILE_SEEK_CUR);
}

void File_Seek(MYFILE *file, size_t pos, FILE_SEEK_MODE mode)
{
    switch (mode) {
    case FILE_SEEK_SET:
        fseek(file->fp, pos, SEEK_SET);
        break;
    case FILE_SEEK_CUR:
        fseek(file->fp, pos, SEEK_CUR);
        break;
    case FILE_SEEK_END:
        fseek(file->fp, pos, SEEK_END);
        break;
    }
}

size_t File_Pos(MYFILE *file)
{
    return ftell(file->fp);
}

size_t File_Size(MYFILE *file)
{
    size_t old = ftell(file->fp);
    fseek(file->fp, 0, SEEK_END);
    size_t size = ftell(file->fp);
    fseek(file->fp, old, SEEK_SET);
    return size;
}

const char *File_GetPath(MYFILE *file)
{
    return file->path;
}

bool File_GetMeta(
    const char *const path, uint64_t *const out_size, uint64_t *const out_mtime)
{
    MYFILE *const file = File_Open(path, FILE_OPEN_READ);
    if (file == nullptr) {
        return false;
    }

    if (out_size != nullptr) {
        *out_size = (uint64_t)File_Size(file);
    }

    if (out_mtime != nullptr) {
        uint64_t mtime = 0;
#if defined(_WIN32)
        struct _stat64 st;
        if (_fstat64(_fileno(file->fp), &st) == 0) {
            mtime = (uint64_t)st.st_mtime;
        }
#else
        struct stat st;
        if (fstat(fileno(file->fp), &st) == 0) {
            mtime = (uint64_t)st.st_mtime;
        }
#endif
        *out_mtime = mtime;
    }

    File_Close(file);
    return true;
}

void File_Close(MYFILE *file)
{
    fclose(file->fp);
    Memory_FreePointer(&file->path);
    // free per-file line buffer
    Memory_FreePointer(&file);
}

bool File_Load(const char *path, char **output_data, size_t *output_size)
{
    ASSERT(output_data != nullptr);

    MYFILE *fp = File_Open(path, FILE_OPEN_READ);
    if (!fp) {
        LOG_ERROR("Can't open file %s", path);
        *output_data = nullptr;
        return false;
    }

    size_t data_size = File_Size(fp);
    char *data = Memory_Alloc(data_size + 1);
    File_ReadData(fp, data, data_size);
    if (File_Pos(fp) != data_size) {
        *output_data = nullptr;
        LOG_ERROR("Can't read file %s", path);
        Memory_FreePointer(&data);
        File_Close(fp);
        return false;
    }
    File_Close(fp);
    data[data_size] = '\0';

    *output_data = data;
    if (output_size != nullptr) {
        *output_size = data_size;
    }
    return true;
}

void File_CreateDirectory(const char *path)
{
    if (path == nullptr) {
        return;
    }
#if defined(_WIN32)
    _mkdir(path);
#else
    mkdir(path, 0775);
#endif
}

void File_EnsureParentDirectories(const char *path)
{
    ASSERT(path != nullptr);
    char *parent = File_GetParentDirectory(path);
    if (parent != nullptr) {
        /* Only recurse/create if there is a distinct, non-empty parent */
        if (parent[0] != '\0' && strcmp(parent, path) != 0) {
            if (!File_DirExists(parent)) {
                File_EnsureParentDirectories(parent);
                File_CreateDirectory(parent);
            }
        }
        Memory_FreePointer(&parent);
    }
}

void *File_OpenDirectory(const char *const path)
{
    ASSERT(path != nullptr);
    return opendir(path);
}

const char *File_ReadDirectory(void *const dir)
{
    DIR *path_dir = (DIR *)dir;
    struct dirent *cur_file = readdir(dir);
    if (cur_file == nullptr) {
        return nullptr;
    }
    return cur_file->d_name;
}

void File_CloseDirectory(void *const dir)
{
    ASSERT(dir != nullptr);
    closedir(dir);
}
