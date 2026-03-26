#include <trx/core/bson/write.h>

#include <trx/core/log.h>
#include <trx/core/memory.h>
#include <trx/debug.h>

#include <float.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool M_GetValueWrappedSize(
    size_t *size, const char *key, const JSON_VALUE *value);
static char *M_WriteValueWrapped(
    char *data, const char *key, const JSON_VALUE *value);

static bool M_GetMarkerSize(size_t *size, const char *key)
{
    ASSERT(size != nullptr);
    ASSERT(key != nullptr);
    *size += 1; // marker
    *size += strlen(key); // key
    *size += 1; // nullptr terminator
    return true;
}

static bool M_GetNullWrappedSize(size_t *size, const char *key)
{
    ASSERT(size != nullptr);
    ASSERT(key != nullptr);
    return M_GetMarkerSize(size, key);
}

static bool M_GetBoolWrappedSize(size_t *size, const char *key)
{
    ASSERT(size != nullptr);
    ASSERT(key != nullptr);
    if (!M_GetMarkerSize(size, key)) {
        return false;
    }
    *size += 1;
    return true;
}

static bool M_GetInt32Size(size_t *size)
{
    ASSERT(size != nullptr);
    *size += sizeof(int32_t);
    return true;
}

static bool M_GetInt32WrappedSize(size_t *size, const char *key)
{
    ASSERT(size != nullptr);
    ASSERT(key != nullptr);
    if (!M_GetMarkerSize(size, key)) {
        return false;
    }
    if (!M_GetInt32Size(size)) {
        return false;
    }
    return true;
}

static bool M_GetDoubleSize(size_t *size)
{
    ASSERT(size != nullptr);
    *size += sizeof(double);
    return true;
}

static bool M_GetDoubleWrappedSize(size_t *size, const char *key)
{
    ASSERT(size != nullptr);
    ASSERT(key != nullptr);
    if (!M_GetMarkerSize(size, key)) {
        return false;
    }
    if (!M_GetDoubleSize(size)) {
        return false;
    }
    return true;
}

static bool M_GetNumberWrappedSize(
    size_t *size, const char *key, const JSON_NUMBER *number)
{
    ASSERT(size != nullptr);
    ASSERT(key != nullptr);

    char *str = number->number;
    ASSERT(str != nullptr);

    // hexadecimal numbers
    if (number->number_size >= 2 && (str[1] == 'x' || str[1] == 'X')) {
        return M_GetInt32WrappedSize(size, key);
    }

    // skip leading sign
    if (str[0] == '+' || str[0] == '-') {
        str += 1;
    }
    ASSERT(str[0] != '\0');

    if (!strcmp(str, "Infinity")) {
        // BSON does not support Infinity.
        return M_GetDoubleWrappedSize(size, key);
    } else if (!strcmp(str, "NaN")) {
        // BSON does not support NaN.
        return M_GetInt32WrappedSize(size, key);
    } else if (strchr(str, '.')) {
        return M_GetDoubleWrappedSize(size, key);
    } else {
        return M_GetInt32WrappedSize(size, key);
    }

    return false;
}

static bool M_GetStringSize(size_t *size, const JSON_STRING *string)
{
    ASSERT(size != nullptr);
    ASSERT(string != nullptr);
    *size += sizeof(uint32_t); // size
    *size += string->string_size; // string
    *size += 1; // nullptr terminator
    return true;
}

static bool M_GetStringWrappedSize(
    size_t *size, const char *key, const JSON_STRING *string)
{
    ASSERT(size != nullptr);
    ASSERT(key != nullptr);
    ASSERT(string != nullptr);
    if (!M_GetMarkerSize(size, key)) {
        return false;
    }
    if (!M_GetStringSize(size, string)) {
        return false;
    }
    return true;
}

static bool M_GetArraySize(size_t *size, const JSON_ARRAY *array)
{
    ASSERT(size != nullptr);
    ASSERT(array != nullptr);
    char key[12];
    int idx = 0;
    *size += sizeof(int32_t); // object size
    for (JSON_ARRAY_ELEMENT *element = array->start; element != nullptr;
         element = element->next) {
        sprintf(key, "%d", idx);
        idx++;
        if (!M_GetValueWrappedSize(size, key, element->value)) {
            return false;
        }
    }
    *size += 1; // nullptr terminator
    return true;
}

static bool M_GetArrayWrappedSize(
    size_t *size, const char *key, const JSON_ARRAY *array)
{
    ASSERT(size != nullptr);
    ASSERT(key != nullptr);
    ASSERT(array != nullptr);
    if (!M_GetMarkerSize(size, key)) {
        return false;
    }
    if (!M_GetArraySize(size, array)) {
        return false;
    }
    return true;
}

static bool M_GetObjectSize(size_t *size, const JSON_OBJECT *object)
{
    ASSERT(size != nullptr);
    ASSERT(object != nullptr);
    *size += sizeof(int32_t); // object size
    for (JSON_OBJECT_ELEMENT *element = object->start; element != nullptr;
         element = element->next) {
        if (!M_GetValueWrappedSize(
                size, element->name->string, element->value)) {
            return false;
        }
    }
    *size += 1; // nullptr terminator
    return true;
}

static bool M_GetObjectWrappedSize(
    size_t *size, const char *key, const JSON_OBJECT *object)
{
    ASSERT(size != nullptr);
    ASSERT(key != nullptr);
    ASSERT(object != nullptr);
    if (!M_GetMarkerSize(size, key)) {
        return false;
    }
    if (!M_GetObjectSize(size, object)) {
        return false;
    }
    return true;
}

static bool M_GetValueSize(size_t *size, const JSON_VALUE *value)
{
    ASSERT(size != nullptr);
    ASSERT(value != nullptr);
    switch (value->type) {
    case JSON_TYPE_ARRAY:
        return M_GetArraySize(size, (JSON_ARRAY *)value->payload);
    case JSON_TYPE_OBJECT:
        return M_GetObjectSize(size, (JSON_OBJECT *)value->payload);
    default:
        LOG_ERROR("Bad BSON root element: %d", value->type);
    }
    return false;
}

static bool M_GetValueWrappedSize(
    size_t *size, const char *key, const JSON_VALUE *value)
{
    ASSERT(size != nullptr);
    ASSERT(key != nullptr);
    ASSERT(value != nullptr);
    switch (value->type) {
    case JSON_TYPE_NULL:
        return M_GetNullWrappedSize(size, key);
    case JSON_TYPE_TRUE:
        return M_GetBoolWrappedSize(size, key);
    case JSON_TYPE_FALSE:
        return M_GetBoolWrappedSize(size, key);
    case JSON_TYPE_NUMBER:
        return M_GetNumberWrappedSize(size, key, (JSON_NUMBER *)value->payload);
    case JSON_TYPE_STRING:
        return M_GetStringWrappedSize(size, key, (JSON_STRING *)value->payload);
    case JSON_TYPE_ARRAY:
        return M_GetArrayWrappedSize(size, key, (JSON_ARRAY *)value->payload);
    case JSON_TYPE_OBJECT:
        return M_GetObjectWrappedSize(size, key, (JSON_OBJECT *)value->payload);
    default:
        LOG_ERROR("Unknown JSON element: %d", value->type);
        return false;
    }
}

static char *M_WriteMarker(char *data, const char *key, const uint8_t marker)
{
    ASSERT(data != nullptr);
    ASSERT(key != nullptr);
    *data++ = marker;
    strcpy(data, key);
    data += strlen(key);
    *data++ = '\0';
    return data;
}

static char *M_WriteNullWrapped(char *data, const char *key)
{
    ASSERT(data != nullptr);
    ASSERT(key != nullptr);
    return M_WriteMarker(data, key, '\x0A');
}

static char *M_WriteBoolWrapped(char *data, const char *key, bool value)
{
    ASSERT(data != nullptr);
    ASSERT(key != nullptr);
    data = M_WriteMarker(data, key, '\x08');
    *(int8_t *)data++ = (int8_t)value;
    return data;
}

static char *M_WriteInt32(char *data, const int32_t value)
{
    ASSERT(data != nullptr);
    memcpy(data, &value, sizeof(value));
    data += sizeof(int32_t);
    return data;
}

static char *M_WriteInt32Wrapped(
    char *data, const char *key, const int32_t value)
{
    ASSERT(data != nullptr);
    ASSERT(key != nullptr);
    data = M_WriteMarker(data, key, '\x10');
    return M_WriteInt32(data, value);
}

static char *M_WriteDouble(char *data, const double value)
{
    ASSERT(data != nullptr);
    memcpy(data, &value, sizeof(value));
    data += sizeof(double);
    return data;
}

static char *M_WriteDoubleWrapped(
    char *data, const char *key, const double value)
{
    ASSERT(data != nullptr);
    ASSERT(key != nullptr);
    data = M_WriteMarker(data, key, '\x01');
    return M_WriteDouble(data, value);
}

static char *M_WriteNumberWrapped(
    char *data, const char *key, const JSON_NUMBER *number)
{
    ASSERT(data != nullptr);
    ASSERT(key != nullptr);
    ASSERT(number != nullptr);
    char *str = number->number;

    // hexadecimal numbers
    if (number->number_size >= 2 && (str[1] == 'x' || str[1] == 'X')) {
        return M_WriteInt32Wrapped(
            data, key, json_strtoumax(number->number, nullptr, 0));
    }

    // skip leading sign
    if (str[0] == '+' || str[0] == '-') {
        str++;
    }
    ASSERT(str[0] != '\0');

    if (!strcmp(str, "Infinity")) {
        // BSON does not support Infinity.
        return M_WriteDoubleWrapped(data, key, DBL_MAX);
    } else if (!strcmp(str, "NaN")) {
        // BSON does not support NaN.
        return M_WriteInt32Wrapped(data, key, 0);
    } else if (strchr(str, '.')) {
        return M_WriteDoubleWrapped(data, key, atof(number->number));
    } else {
        return M_WriteInt32Wrapped(data, key, atoi(number->number));
    }

    return data;
}

static char *M_WriteString(char *data, const JSON_STRING *string)
{
    ASSERT(data != nullptr);
    ASSERT(string != nullptr);
    const uint32_t bson_string_size = string->string_size + 1;
    memcpy(data, &bson_string_size, sizeof(bson_string_size));
    data += sizeof(uint32_t);
    memcpy(data, string->string, string->string_size);
    data += string->string_size;
    *data++ = '\0';
    return data;
}

static char *M_WriteStringWrapped(
    char *data, const char *key, const JSON_STRING *string)
{
    ASSERT(data != nullptr);
    ASSERT(key != nullptr);
    ASSERT(string != nullptr);
    data = M_WriteMarker(data, key, '\x02');
    data = M_WriteString(data, string);
    return data;
}

static char *M_WriteArray(char *data, const JSON_ARRAY *array)
{
    ASSERT(data != nullptr);
    ASSERT(array != nullptr);
    char key[12];
    int idx = 0;
    char *old = data;
    data += sizeof(int32_t);
    for (JSON_ARRAY_ELEMENT *element = array->start; element != nullptr;
         element = element->next) {
        sprintf(key, "%d", idx);
        idx++;
        data = M_WriteValueWrapped(data, key, element->value);
    }
    *data++ = '\0';
    const int32_t object_size = data - old;
    memcpy(old, &object_size, sizeof(object_size));
    return data;
}

static char *M_WriteArrayWrapped(
    char *data, const char *key, const JSON_ARRAY *array)
{
    ASSERT(data != nullptr);
    ASSERT(key != nullptr);
    ASSERT(array != nullptr);
    data = M_WriteMarker(data, key, '\x04');
    data = M_WriteArray(data, array);
    return data;
}

static char *M_WriteObject(char *data, const JSON_OBJECT *object)
{
    ASSERT(data != nullptr);
    ASSERT(object != nullptr);
    char *old = data;
    data += sizeof(int32_t);
    for (JSON_OBJECT_ELEMENT *element = object->start; element != nullptr;
         element = element->next) {
        data = M_WriteValueWrapped(data, element->name->string, element->value);
    }
    *data++ = '\0';
    const int32_t object_size = data - old;
    memcpy(old, &object_size, sizeof(object_size));
    return data;
}

static char *M_WriteObjectWrapped(
    char *data, const char *key, const JSON_OBJECT *object)
{
    ASSERT(data != nullptr);
    ASSERT(key != nullptr);
    ASSERT(object != nullptr);
    data = M_WriteMarker(data, key, '\x03');
    data = M_WriteObject(data, object);
    return data;
}

static char *M_WriteValue(char *data, const JSON_VALUE *value)
{
    ASSERT(data != nullptr);
    ASSERT(value != nullptr);
    switch (value->type) {
    case JSON_TYPE_ARRAY:
        data = M_WriteArray(data, (JSON_ARRAY *)value->payload);
        break;
    case JSON_TYPE_OBJECT:
        data = M_WriteObject(data, (JSON_OBJECT *)value->payload);
        break;
    default:
        ASSERT_FAIL();
    }
    return data;
}

static char *M_WriteValueWrapped(
    char *data, const char *key, const JSON_VALUE *value)
{
    ASSERT(data != nullptr);
    ASSERT(key != nullptr);
    ASSERT(value != nullptr);
    switch (value->type) {
    case JSON_TYPE_NULL:
        return M_WriteNullWrapped(data, key);
    case JSON_TYPE_TRUE:
        return M_WriteBoolWrapped(data, key, true);
    case JSON_TYPE_FALSE:
        return M_WriteBoolWrapped(data, key, false);
    case JSON_TYPE_NUMBER:
        return M_WriteNumberWrapped(data, key, (JSON_NUMBER *)value->payload);
    case JSON_TYPE_STRING:
        return M_WriteStringWrapped(data, key, (JSON_STRING *)value->payload);
    case JSON_TYPE_ARRAY:
        return M_WriteArrayWrapped(data, key, (JSON_ARRAY *)value->payload);
    case JSON_TYPE_OBJECT:
        return M_WriteObjectWrapped(data, key, (JSON_OBJECT *)value->payload);
    default:
        return nullptr;
    }
}

void *BSON_Write(const JSON_VALUE *value, size_t *out_size)
{
    ASSERT(value != nullptr);
    *out_size = -1;
    if (value == nullptr) {
        return nullptr;
    }

    size_t size = 0;
    if (!M_GetValueSize(&size, value)) {
        return nullptr;
    }

    char *data = Memory_Alloc(size);
    char *data_end = M_WriteValue(data, value);
    ASSERT((size_t)(data_end - data) == size);

    if (out_size != nullptr) {
        *out_size = size;
    }

    return data;
}
