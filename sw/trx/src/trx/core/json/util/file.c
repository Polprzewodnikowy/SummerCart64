#include <trx/core/json/util/file.h>

#include <trx/core/filesystem.h>
#include <trx/core/json.h>
#include <trx/core/json/util/read_io.h>
#include <trx/core/log.h>
#include <trx/core/memory.h>
#include <trx/core/shell.h>

#include <string.h>

#define M_PARSE_FLAGS                                                          \
    (JSON_PARSE_FLAGS_ALLOW_JSON5 | JSON_PARSE_FLAGS_ALLOW_LOCATION_INFORMATION)

void JSONFile_ExitWithReadIOError(
    const JSON_READ_IO *const io, const char *const fallback_message)
{
    const char *const error = JSON_ReadIO_GetError(io);
    if (error != nullptr && error[0] != '\0') {
        char log_message[1024];
        char dialog_message[1024];
        JSON_ReadIO_FormatError(io, false, log_message, sizeof(log_message));
        JSON_ReadIO_FormatError(
            io, true, dialog_message, sizeof(dialog_message));
        Shell_ExitSystemEx(log_message, dialog_message);
    }
    Shell_ExitSystem(fallback_message);
}

JSON_VALUE *JSONFile_Read(const char *path)
{
    return JSONFile_ReadEx(path, false);
}

JSON_VALUE *JSONFile_ReadEx(const char *path, const bool exit_on_error)
{
    char *file_data = nullptr;
    if (!File_Load(path, &file_data, nullptr)) {
        return nullptr;
    }

    JSON_PARSE_RESULT pr;
    JSON_VALUE *const value = JSON_ParseEx(
        file_data, strlen(file_data), M_PARSE_FLAGS, nullptr, nullptr, &pr);
    if (value == nullptr) {
        JSON_READ_IO *const io = JSON_ReadIO_Create(nullptr, 0, path);
        JSON_ReadIO_SetErrorAt(
            io, pr.error_line_no, pr.error_row_no, "%s",
            JSON_GetErrorDescription(pr.error));
        if (exit_on_error) {
            JSONFile_ExitWithReadIOError(io, "JSON parse error");
        } else {
            char log_message[1024];
            JSON_ReadIO_FormatError(
                io, false, log_message, sizeof(log_message));
            LOG_ERROR("%s", log_message);
        }
        JSON_ReadIO_Destroy(io);
    }
    Memory_FreePointer(&file_data);
    return value;
}

bool JSONFile_Write(const char *path, JSON_VALUE *const value)
{
    char *old_data = nullptr;
    File_Load(path, &old_data, nullptr);

    size_t out_len;
    char *out_data = JSON_WritePretty(value, "  ", "\n", &out_len);

    bool updated = false;
    if (old_data == nullptr || strcmp(old_data, out_data) != 0) {
        MYFILE *const fp = File_Open(path, FILE_OPEN_WRITE);
        if (fp == nullptr) {
            LOG_ERROR("unable to open '%s' for writing", path);
        } else {
            LOG_DEBUG("saving JSON to %s", path);
            File_WriteData(fp, out_data, out_len - 1); // w/o \0
            File_Close(fp);
            updated = true;
        }
    }

    Memory_FreePointer(&old_data);
    Memory_FreePointer(&out_data);
    return updated;
}
