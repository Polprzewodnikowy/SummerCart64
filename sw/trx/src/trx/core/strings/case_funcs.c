#include <trx/core/memory.h>
#include <trx/core/strings.h>

#include <ctype.h>
#include <string.h>

// Mapping of lowercase to uppercase characters beyond ASCII.
static struct {
    const char *lower;
    const char *upper;
} m_CaseMap[] = {
#define X_CASE_MAP(low, up) { low, up },
#include <trx/core/strings/case_map.def>
#undef X_CASE_MAP
    { .lower = nullptr, .upper = nullptr },
};

size_t String_GetCharByteSize(const char *const ptr)
{
    // Check for named escape sequence.
    if (*ptr == '\\' && *(ptr + 1) == '{') {
        const char *end = strchr(ptr + 2, '}');
        if (end != nullptr) {
            return end + 1 - ptr;
        }
        return 1;
    }

    // clang-format off
    // UTF-8 sequence lengths
    if ((*ptr & 0x80) == 0x00) { return 1; } // 1-byte sequence
    if ((*ptr & 0xE0) == 0xC0) { return 2; } // 2-byte sequence
    if ((*ptr & 0xF0) == 0xE0) { return 3; } // 3-byte sequence
    if ((*ptr & 0xF8) == 0xF0) { return 4; } // 4-byte sequence
    // clang-format on

    // Fallback to 1
    return 1;
}

char *String_ToUpper(const char *const text)
{
    if (text == nullptr) {
        return nullptr;
    }

    const size_t text_len = strlen(text);
    char *const upper_text = Memory_Alloc(text_len + 1);
    const char *src = text;
    char *dest = upper_text;

    while (*src != '\0') {
        bool mapped = false;
        for (size_t i = 0; m_CaseMap[i].lower != nullptr; i++) {
            const char *const lower = m_CaseMap[i].lower;
            const char *const upper = m_CaseMap[i].upper;
            const size_t lower_len = strlen(lower);
            if (strncmp(src, lower, lower_len) == 0) {
                const size_t upper_len = strlen(upper);
                memcpy(dest, upper, upper_len);
                dest += upper_len;
                src += lower_len;
                mapped = true;
                break;
            }
        }
        if (mapped) {
            continue;
        }

        const size_t char_len = String_GetCharByteSize(src);
        memcpy(dest, src, char_len);
        dest += char_len;
        src += char_len;
    }

    *dest = '\0';
    return upper_text;
}

char *String_ToUpperPattern(const char *const pattern)
{
    char *const upper_pattern = Memory_DupStr(pattern);
    char *q = upper_pattern;
    while (*q != '\0') {
        if (*q == '%') {
            q++;
            while (*q != '\0' && !strchr("diouxXfFeEgGaAcspn%", *q)) {
                q++;
            }
            if (*q != '\0') {
                q++;
            }
        } else {
            *q = (char)toupper((unsigned char)*q);
            q++;
        }
    }
    return upper_pattern;
}
