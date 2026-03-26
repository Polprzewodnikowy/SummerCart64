#pragma once

#include <stddef.h>
#include <stdint.h>

// Low-level filesystem module.
// Intentionally dumb wrappers over file/dir primitives. No path policy,
// no token expansion, and no case-normalization logic belongs here.

typedef enum {
    FILE_SEEK_SET,
    FILE_SEEK_CUR,
    FILE_SEEK_END,
} FILE_SEEK_MODE;

typedef enum {
    FILE_OPEN_READ,
    FILE_OPEN_READ_WRITE,
    FILE_OPEN_WRITE,
} FILE_OPEN_MODE;

typedef struct MYFILE MYFILE;

// ============================================================================
// Path functions

// Return true when path points to an existing directory.
bool File_DirExists(const char *path);

// Return true if path is absolute for current platform conventions.
bool File_IsAbsolute(const char *path);

// Return true if path is not absolute.
bool File_IsRelative(const char *path);

// Return true when path points to an existing file.
bool File_Exists(const char *path);

// Return parent directory component of path (owning string), or nullptr.
char *File_GetParentDirectory(const char *path);

// ============================================================================
// File handle functions

// Open path with requested mode and wrap as MYFILE.
// Returns nullptr on failure.
MYFILE *File_Open(const char *path, FILE_OPEN_MODE mode);
// Current byte position in file stream.
size_t File_Pos(MYFILE *file);

// Total file size in bytes.
size_t File_Size(MYFILE *file);

// Original path passed to File_Open.
const char *File_GetPath(MYFILE *file);

// Get file size and modification time (seconds since epoch).
// Returns false if the file cannot be opened/resolved.
bool File_GetMeta(const char *path, uint64_t *out_size, uint64_t *out_mtime);

// Skip forward by `bytes`.
void File_Skip(MYFILE *file, size_t bytes);

// Seek to position according to FILE_SEEK_MODE.
void File_Seek(MYFILE *file, size_t pos, FILE_SEEK_MODE mode);

// Close and free MYFILE.
void File_Close(MYFILE *file);

// Load entire file into memory as a null-terminated buffer.
// Caller owns `output_data` and must free it.
bool File_Load(const char *path, char **output_data, size_t *output_size);

// ============================================================================
// Read helpers

// Read exact byte count into `data`. Returns false on short read.
bool File_ReadData(MYFILE *file, void *data, size_t size);
// Read `count` items of `item_size` into `data`. Returns false on short read.
bool File_ReadItems(MYFILE *file, void *data, size_t count, size_t item_size);
// Typed scalar read helpers.
int8_t File_ReadS8(MYFILE *file);
int16_t File_ReadS16(MYFILE *file);
int32_t File_ReadS32(MYFILE *file);
uint8_t File_ReadU8(MYFILE *file);
uint16_t File_ReadU16(MYFILE *file);
uint32_t File_ReadU32(MYFILE *file);

// ============================================================================
// Write helpers

// Raw/typed write helpers.
void File_WriteData(MYFILE *file, const void *data, size_t size);
void File_WriteItems(
    MYFILE *file, const void *data, size_t count, size_t item_size);
void File_WriteS8(MYFILE *file, int8_t value);
void File_WriteS16(MYFILE *file, int16_t value);
void File_WriteS32(MYFILE *file, int32_t value);
void File_WriteU8(MYFILE *file, uint8_t value);
void File_WriteU16(MYFILE *file, uint16_t value);
void File_WriteU32(MYFILE *file, uint32_t value);

// Write formatted string to file using a static-format buffer.
// The formatted text is written via fputs; no trailing newline is added.
void File_WriteString(MYFILE *file, const char *fmt, ...);

// ============================================================================
// Directory functions

// Create one directory path component.
void File_CreateDirectory(const char *path);
// Recursively ensure all parent directories for `path` exist.
void File_EnsureParentDirectories(const char *path);

// Directory iteration API.
void *File_OpenDirectory(const char *path);
const char *File_ReadDirectory(void *dir);
void File_CloseDirectory(void *dir);
