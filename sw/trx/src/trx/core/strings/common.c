#include <trx/core/log.h>
#include <trx/core/memory.h>
#include <trx/core/strings.h>
#include <trx/core/utils.h>
#include <trx/debug.h>

#include <ctype.h>
#include <pcre2.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Number of static buffers in a rotating ring for String_FormatStatic, etc.
#define M_MAX_STATIC_BUFFERS 8

typedef struct {
    char *buf;
    size_t capacity;
} M_STATIC_BUFFER;

static M_STATIC_BUFFER m_StaticBufferRing[M_MAX_STATIC_BUFFERS] = {};
static int m_StaticBufNext = 0;

__attribute__((destructor)) static void M_Shutdown(void)
{
    for (int32_t i = 0; i < M_MAX_STATIC_BUFFERS; i++) {
        Memory_FreePointer(&m_StaticBufferRing[i].buf);
        m_StaticBufferRing[i].capacity = 0;
    }
}

static M_STATIC_BUFFER *M_CycleStaticBuffer(void)
{
    const int32_t idx = m_StaticBufNext;
    m_StaticBufNext = (m_StaticBufNext + 1) % M_MAX_STATIC_BUFFERS;
    return &m_StaticBufferRing[idx];
}

static void M_AddPage(
    const char *text, int32_t start_pos, int32_t length, VECTOR *const pages)
{
    char substr[length + 1];
    while (text[start_pos] == '\n' && text[start_pos] != '\0') {
        start_pos++;
        length--;
    }
    strncpy(substr, text + start_pos, length);
    substr[length] = '\0';

    const char *page = Memory_DupStr(substr);
    Vector_Add(pages, &page);
}

bool String_EndsWith(const char *str, const char *suffix)
{
    int str_len = strlen(str);
    int suffix_len = strlen(suffix);

    if (suffix_len > str_len) {
        return 0;
    }

    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

bool String_Equivalent(const char *a, const char *b)
{
    if (a == nullptr || b == nullptr) {
        return false;
    }

    size_t a_size = strlen(a);
    size_t b_size = strlen(b);
    if (a_size != b_size) {
        return false;
    }

    for (size_t i = 0; i < a_size; i++) {
        if (tolower(a[i]) != tolower(b[i])) {
            return false;
        }
    }
    return true;
}

const char *String_CaseSubstring(const char *subject, const char *pattern)
{
    if (subject == nullptr || pattern == nullptr) {
        return nullptr;
    }

    size_t str_size = strlen(subject);
    size_t substr_size = strlen(pattern);
    if (substr_size > str_size) {
        return nullptr;
    }
    if (substr_size == 0) {
        return subject;
    }

    for (size_t i = 0; i < str_size + 1 - substr_size; i++) {
        bool equivalent = true;
        for (size_t j = 0; j < substr_size; j++) {
            if (tolower(subject[i + j]) != tolower(pattern[j])) {
                equivalent = false;
                break;
            }
        }
        if (equivalent) {
            return subject + i;
        }
    }
    return nullptr;
}

bool String_Match(const char *const subject, const char *const pattern)
{
    if (subject == nullptr || pattern == nullptr) {
        return 0;
    }

    const unsigned char *const usubject = (const unsigned char *)subject;
    const unsigned char *const upattern = (const unsigned char *)pattern;
    const uint32_t options = PCRE2_CASELESS;

    const uint32_t ovec_size = 128;

    int err_code;
    PCRE2_SIZE err_offset;
    pcre2_code *const re = pcre2_compile(
        upattern, PCRE2_ZERO_TERMINATED, options, &err_code, &err_offset,
        nullptr);
    if (re == nullptr) {
        PCRE2_UCHAR8 buffer[128];
        pcre2_get_error_message(err_code, buffer, 120);
        LOG_ERROR("%d\t%s", err_code, buffer);
        return false;
    }

    pcre2_match_data *const match_data =
        pcre2_match_data_create(ovec_size, nullptr);
    const int flags = 0;
    const int rc = pcre2_match(
        re, usubject, PCRE2_ZERO_TERMINATED, 0, flags, match_data, nullptr);
    pcre2_match_data_free(match_data);
    pcre2_code_free(re);

    return rc > 0;
}

bool String_IsEmpty(const char *const value)
{
    return String_Match(value, "^\\s*$");
}

bool String_ParseBool(const char *const value, bool *const target)
{
    if (String_Match(value, "^(0|false|off)$")) {
        if (target != nullptr) {
            *target = false;
        }
        return true;
    }

    if (String_Match(value, "^(1|true|on)$")) {
        if (target != nullptr) {
            *target = true;
        }
        return true;
    }

    return false;
}

bool String_ParseInteger(const char *const value, int32_t *const target)
{
    return sscanf(value, "%d", target) == 1;
}

bool String_ParseDecimal(const char *const value, float *const target)
{
    bool has_dot = false;
    for (size_t i = 0; i < strlen(value); i++) {
        if (i == 0 && value[i] == '-') {
            continue;
        }
        if (!isdigit(value[i])) {
            if (value[i] == '.') {
                if (has_dot) {
                    return false;
                }
                has_dot = true;
            } else {
                return false;
            }
        }
    }
    if (target != nullptr) {
        *target = atof(value);
    }
    return true;
}

bool String_ParseRGB888(const char *value, RGB_888 *const target)
{
    if (value[0] == '#') {
        value++;
    }
    return sscanf(
               value, "%02hhX%02hhX%02hhX", &target->r, &target->g, &target->b)
        == 3;
}

bool String_ParseRGBA8888(const char *value, RGBA_8888 *const target)
{
    if (value[0] == '#') {
        value++;
    }
    return sscanf(
               value, "%02hhX%02hhX%02hhX%02hhX", &target->r, &target->g,
               &target->b, &target->a)
        == 4;
}

VECTOR *String_Paginate(const char *const text, const int32_t max_lines)
{
    VECTOR *const pages = Vector_Create(sizeof(char *));
    int32_t line_count = 0;
    int32_t start_pos = 0;
    int32_t current_length = 0;

    const char *iter_text = text;

    while (*iter_text != '\0') {
        current_length++;
        if (*iter_text == '\n') {
            line_count++;
        }

        if (line_count == max_lines || *iter_text == '\f') {
            M_AddPage(text, start_pos, current_length, pages);

            start_pos += current_length;
            current_length = 0;
            line_count = 0;
        }

        *iter_text++;
    }

    // Anything that is left becomes its own page.
    if (pages->count == 0 || current_length != 0) {
        M_AddPage(text, start_pos, current_length, pages);
    }

    return pages;
}

char *String_Format(const char *const fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    int len = vsnprintf(nullptr, 0, fmt, args);
    if (len < 0) {
        va_end(args);
        return nullptr;
    }

    char *const result = Memory_Alloc(len + 1);
    va_end(args);
    va_start(args, fmt);
    vsnprintf(result, len + 1, fmt, args);
    va_end(args);
    return result;
}

void String_FormatInto(
    char **target_buf, size_t *target_cap, const char *const fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    String_FormatIntoV(target_buf, target_cap, fmt, args);
    va_end(args);
}

void String_FormatIntoV(
    char **target_buf, size_t *target_cap, const char *const fmt, va_list args)
{
    va_list args_copy;
    va_copy(args_copy, args);
    const int32_t len = vsnprintf(nullptr, 0, fmt, args_copy);
    va_end(args_copy);
    if (len < 0) {
        return;
    }
    const size_t needed = (size_t)len + 1;
    if (*target_cap < needed) {
        *target_buf = Memory_Realloc(*target_buf, needed);
        *target_cap = needed;
    }
    vsnprintf(*target_buf, *target_cap, fmt, args);
}

const char *String_FormatStatic(const char *const fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    const char *const result = String_FormatStaticV(fmt, args);
    va_end(args);
    return result;
}

const char *String_FormatStaticV(const char *const fmt, va_list args)
{
    M_STATIC_BUFFER *const buffer = M_CycleStaticBuffer();
    String_FormatIntoV(&buffer->buf, &buffer->capacity, fmt, args);
    return buffer->buf;
}
