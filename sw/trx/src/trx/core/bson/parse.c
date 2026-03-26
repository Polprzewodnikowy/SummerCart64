#include <trx/core/bson/parse.h>

#include <trx/core/log.h>
#include <trx/core/memory.h>
#include <trx/debug.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    const char *src;
    size_t size;
    size_t offset;

    char *data;
    char *dom;
    size_t dom_size;
    size_t data_size;

    size_t error;
} M_STATE;

static bool M_GetValueSize(M_STATE *state, uint8_t marker);
static void M_HandleValue(M_STATE *state, JSON_VALUE *value, uint8_t marker);

static int32_t M_ReadI32(const char *const src)
{
    int32_t value = 0;
    memcpy(&value, src, sizeof(value));
    return value;
}

static double M_ReadDouble(const char *const src)
{
    double value = 0.0;
    memcpy(&value, src, sizeof(value));
    return value;
}

static bool M_GetObjectKeySize(M_STATE *state)
{
    ASSERT(state != nullptr);
    while (state->src[state->offset]) {
        state->data_size++;
        state->offset++;
    }
    state->data_size++;
    state->offset++;
    return true;
}

static bool M_GetNullValueSize(M_STATE *state)
{
    ASSERT(state != nullptr);
    return true;
}

static bool M_GetBoolValueSize(M_STATE *state)
{
    ASSERT(state != nullptr);
    if (state->offset + sizeof(uint8_t) > state->size) {
        state->error = BSON_PARSE_ERROR_PREMATURE_END_OF_BUFFER;
        return false;
    }

    switch (state->src[state->offset]) {
    case 0x00:
        break;
    case 0x01:
        break;
    default:
        state->error = BSON_PARSE_ERROR_INVALID_VALUE;
        return false;
    }

    state->offset++;
    return true;
}

static bool M_GetInt32ValueSize(M_STATE *state)
{
    ASSERT(state != nullptr);
    if (state->offset + sizeof(int32_t) > state->size) {
        state->error = BSON_PARSE_ERROR_PREMATURE_END_OF_BUFFER;
        return false;
    }
    int32_t num = M_ReadI32(&state->src[state->offset]);
    state->offset += sizeof(int32_t);

    state->dom_size += sizeof(JSON_NUMBER);
    state->data_size += snprintf(nullptr, 0, "%d", num) + 1;
    return true;
}

static bool M_GetDoubleValueSize(M_STATE *state)
{
    ASSERT(state != nullptr);
    if (state->offset + sizeof(double) > state->size) {
        state->error = BSON_PARSE_ERROR_PREMATURE_END_OF_BUFFER;
        return false;
    }
    double num = M_ReadDouble(&state->src[state->offset]);
    state->offset += sizeof(double);

    state->dom_size += sizeof(JSON_NUMBER);
    state->data_size += snprintf(nullptr, 0, "%f", num) + 1;
    return true;
}

static bool M_GetStringValueSize(M_STATE *state)
{
    ASSERT(state != nullptr);
    if (state->offset + sizeof(int32_t) > state->size) {
        state->error = BSON_PARSE_ERROR_PREMATURE_END_OF_BUFFER;
        return false;
    }
    int32_t size = M_ReadI32(&state->src[state->offset]);
    state->offset += sizeof(int32_t);
    if (state->offset + size > state->size) {
        state->error = BSON_PARSE_ERROR_PREMATURE_END_OF_BUFFER;
        return false;
    }
    if (state->src[state->offset + size - 1] != '\0') {
        state->error = BSON_PARSE_ERROR_INVALID_VALUE;
        return false;
    }
    state->offset += size;
    state->dom_size += sizeof(JSON_STRING);
    state->data_size += size;
    return true;
}

static bool M_GetArrayElementWrappedSize(M_STATE *state)
{
    ASSERT(state != nullptr);

    if (state->offset + sizeof(uint8_t) > state->size) {
        state->error = BSON_PARSE_ERROR_PREMATURE_END_OF_BUFFER;
        return false;
    }
    uint8_t marker = state->src[state->offset];
    state->offset++;

    // BSON arrays always use keys
    state->dom_size += sizeof(JSON_STRING);
    if (!M_GetObjectKeySize(state)) {
        return false;
    }

    state->dom_size += sizeof(JSON_VALUE);
    return M_GetValueSize(state, marker);
}

static bool M_GetArraySize(M_STATE *state)
{
    ASSERT(state != nullptr);

    const size_t start_offset = state->offset;
    if (state->offset + sizeof(int32_t) > state->size) {
        state->error = BSON_PARSE_ERROR_PREMATURE_END_OF_BUFFER;
        return false;
    }
    const int size = M_ReadI32(&state->src[state->offset]);
    state->offset += sizeof(int32_t);

    while (state->offset < start_offset + size - 1) {
        state->dom_size += sizeof(JSON_ARRAY_ELEMENT);
        if (!M_GetArrayElementWrappedSize(state)) {
            return false;
        }
    }

    if (state->offset + sizeof(char) > state->size) {
        state->error = BSON_PARSE_ERROR_PREMATURE_END_OF_BUFFER;
        return false;
    }
    if (state->src[state->offset] != '\0') {
        state->error = BSON_PARSE_ERROR_INVALID_VALUE;
        return false;
    }
    state->offset++;
    return true;
}

static bool M_GetArrayValueSize(M_STATE *state)
{
    ASSERT(state != nullptr);
    state->dom_size += sizeof(JSON_ARRAY);
    return M_GetArraySize(state);
}

static bool M_GetObjectElementWrappedSize(M_STATE *state)
{
    ASSERT(state != nullptr);

    if (state->offset + sizeof(uint8_t) > state->size) {
        state->error = BSON_PARSE_ERROR_PREMATURE_END_OF_BUFFER;
        return false;
    }
    uint8_t marker = state->src[state->offset];
    state->offset++;

    state->dom_size += sizeof(JSON_STRING);
    if (!M_GetObjectKeySize(state)) {
        return false;
    }

    state->dom_size += sizeof(JSON_VALUE);
    return M_GetValueSize(state, marker);
}

static bool M_GetObjectSize(M_STATE *state)
{
    ASSERT(state != nullptr);

    const size_t start_offset = state->offset;
    if (state->offset + sizeof(int32_t) > state->size) {
        state->error = BSON_PARSE_ERROR_PREMATURE_END_OF_BUFFER;
        return false;
    }
    const int size = M_ReadI32(&state->src[state->offset]);
    state->offset += sizeof(int32_t);

    while (state->offset < start_offset + size - 1) {
        state->dom_size += sizeof(JSON_OBJECT_ELEMENT);
        if (!M_GetObjectElementWrappedSize(state)) {
            return false;
        }
    }

    if (state->offset + sizeof(char) > state->size) {
        state->error = BSON_PARSE_ERROR_PREMATURE_END_OF_BUFFER;
        return false;
    }
    if (state->src[state->offset] != '\0') {
        state->error = BSON_PARSE_ERROR_INVALID_VALUE;
        return false;
    }
    state->offset++;
    return true;
}

static bool M_GetObjectValueSize(M_STATE *state)
{
    ASSERT(state != nullptr);
    state->dom_size += sizeof(JSON_OBJECT);
    return M_GetObjectSize(state);
}

static bool M_GetValueSize(M_STATE *state, uint8_t marker)
{
    ASSERT(state != nullptr);
    switch (marker) {
    case 0x01:
        return M_GetDoubleValueSize(state);
    case 0x02:
        return M_GetStringValueSize(state);
    case 0x03:
        return M_GetObjectValueSize(state);
    case 0x04:
        return M_GetArrayValueSize(state);
    case 0x0A:
        return M_GetNullValueSize(state);
    case 0x08:
        return M_GetBoolValueSize(state);
    case 0x10:
        return M_GetInt32ValueSize(state);
    default:
        state->error = BSON_PARSE_ERROR_INVALID_VALUE;
        return false;
    }
}

static bool M_GetRootSize(M_STATE *state)
{
    // assume the root element to be an object
    state->dom_size += sizeof(JSON_VALUE);
    return M_GetObjectValueSize(state);
}

static void M_HandleObjectKey(M_STATE *state, JSON_STRING *string)
{
    ASSERT(state != nullptr);
    ASSERT(string != nullptr);
    size_t size = 0;
    string->ref_count = 1;
    string->string = state->data;
    while (state->src[state->offset]) {
        state->data[size++] = state->src[state->offset++];
    }
    string->string_size = size;
    state->data[size++] = state->src[state->offset++];
    state->data += size;
}

static void M_HandleNullValue(M_STATE *state, JSON_VALUE *value)
{
    ASSERT(state != nullptr);
    ASSERT(value != nullptr);
    value->type = JSON_TYPE_NULL;
    value->payload = nullptr;
}

static void M_HandleBoolValue(M_STATE *state, JSON_VALUE *value)
{
    ASSERT(state != nullptr);
    ASSERT(value != nullptr);
    ASSERT(state->offset + sizeof(char) <= state->size);
    switch (state->src[state->offset]) {
    case 0x00:
        value->type = JSON_TYPE_FALSE;
        value->payload = nullptr;
        break;
    case 0x01:
        value->type = JSON_TYPE_TRUE;
        value->payload = nullptr;
        break;
    default:
        ASSERT_FAIL();
    }
    state->offset++;
}

static void M_HandleInt32Value(M_STATE *state, JSON_VALUE *value)
{
    ASSERT(state != nullptr);
    ASSERT(value != nullptr);

    ASSERT(state->offset + sizeof(int32_t) <= state->size);
    int32_t num = M_ReadI32(&state->src[state->offset]);
    state->offset += sizeof(int32_t);

    JSON_NUMBER *number = (JSON_NUMBER *)state->dom;
    number->ref_count = 1;
    state->dom += sizeof(JSON_NUMBER);

    number->number = state->data;
    sprintf(state->data, "%d", num);
    number->number_size = strlen(number->number);
    state->data += number->number_size + 1;

    value->type = JSON_TYPE_NUMBER;
    value->payload = number;
}

static void M_HandleDoubleValue(M_STATE *state, JSON_VALUE *value)
{
    ASSERT(state != nullptr);
    ASSERT(value != nullptr);

    ASSERT(state->offset + sizeof(double) <= state->size);
    double num = M_ReadDouble(&state->src[state->offset]);
    state->offset += sizeof(double);

    JSON_NUMBER *number = (JSON_NUMBER *)state->dom;
    number->ref_count = 1;
    state->dom += sizeof(JSON_NUMBER);

    number->number = state->data;
    sprintf(state->data, "%f", num);
    number->number_size = strlen(number->number);
    state->data += number->number_size + 1;

    // strip trailing zeroes after decimal point
    if (strchr(number->number, '.')) {
        while (number->number[number->number_size - 1] == '0'
               && number->number_size > 1) {
            number->number_size--;
        }
        number->number[number->number_size] = '\0';
    }

    value->type = JSON_TYPE_NUMBER;
    value->payload = number;
}

static void M_HandleStringValue(M_STATE *state, JSON_VALUE *value)
{
    ASSERT(state != nullptr);
    ASSERT(value != nullptr);

    ASSERT(state->offset + sizeof(int32_t) <= state->size);
    int32_t size = M_ReadI32(&state->src[state->offset]);
    state->offset += sizeof(int32_t);

    JSON_STRING *string = (JSON_STRING *)state->dom;
    string->ref_count = 1;
    state->dom += sizeof(JSON_STRING);

    memcpy(state->data, state->src + state->offset, size);
    state->offset += size;

    string->string = state->data;
    string->string_size = size;
    state->data += size;

    value->type = JSON_TYPE_STRING;
    value->payload = string;
}

static void M_HandleArrayElementWrapped(
    M_STATE *state, JSON_ARRAY_ELEMENT *element)
{
    ASSERT(state != nullptr);
    ASSERT(element != nullptr);

    ASSERT(state->offset + sizeof(uint8_t) <= state->size);
    uint8_t marker = state->src[state->offset];
    state->offset++;

    // BSON arrays always use keys
    JSON_STRING *key = (JSON_STRING *)state->dom;
    key->ref_count = 1;
    state->dom += sizeof(JSON_STRING);
    M_HandleObjectKey(state, key);

    JSON_VALUE *value = (JSON_VALUE *)state->dom;
    value->ref_count = 1;
    state->dom += sizeof(JSON_VALUE);

    element->value = value;

    M_HandleValue(state, value, marker);
}

static void M_HandleArray(M_STATE *state, JSON_ARRAY *array)
{
    ASSERT(state != nullptr);
    ASSERT(array != nullptr);

    const size_t start_offset = state->offset;
    ASSERT(state->offset + sizeof(int32_t) <= state->size);
    const int size = M_ReadI32(&state->src[state->offset]);
    state->offset += sizeof(int32_t);

    JSON_ARRAY_ELEMENT *previous = nullptr;
    int count = 0;
    while (state->offset < start_offset + size - 1) {
        JSON_ARRAY_ELEMENT *element = (JSON_ARRAY_ELEMENT *)state->dom;
        element->ref_count = 1;
        state->dom += sizeof(JSON_ARRAY_ELEMENT);
        if (!previous) {
            array->start = element;
        } else {
            previous->next = element;
        }
        previous = element;
        M_HandleArrayElementWrapped(state, element);
        count++;
    }
    if (previous) {
        previous->next = nullptr;
    }
    if (!count) {
        array->start = nullptr;
    }
    array->ref_count = 1;
    array->length = count;
    ASSERT(state->offset + sizeof(char) <= state->size);
    ASSERT(state->src[state->offset] == '\0');
    state->offset++;
}

static void M_HandleArrayValue(M_STATE *state, JSON_VALUE *value)
{
    ASSERT(state != nullptr);
    ASSERT(value != nullptr);

    JSON_ARRAY *array = (JSON_ARRAY *)state->dom;
    array->ref_count = 1;
    state->dom += sizeof(JSON_ARRAY);

    M_HandleArray(state, array);

    value->type = JSON_TYPE_ARRAY;
    value->payload = array;
}

static void M_HandleObjectElementWrapped(
    M_STATE *state, JSON_OBJECT_ELEMENT *element)
{
    ASSERT(state != nullptr);
    ASSERT(element != nullptr);

    ASSERT(state->offset + sizeof(uint8_t) <= state->size);
    uint8_t marker = state->src[state->offset];
    state->offset++;

    JSON_STRING *key = (JSON_STRING *)state->dom;
    key->ref_count = 1;
    state->dom += sizeof(JSON_STRING);
    M_HandleObjectKey(state, key);

    JSON_VALUE *value = (JSON_VALUE *)state->dom;
    value->ref_count = 1;
    state->dom += sizeof(JSON_VALUE);

    element->name = key;
    element->value = value;

    M_HandleValue(state, value, marker);
}

static void M_HandleObject(M_STATE *state, JSON_OBJECT *object)
{
    ASSERT(state != nullptr);
    ASSERT(object != nullptr);

    const size_t start_offset = state->offset;
    ASSERT(state->offset + sizeof(int32_t) <= state->size);
    const int size = M_ReadI32(&state->src[state->offset]);
    state->offset += sizeof(int32_t);

    JSON_OBJECT_ELEMENT *previous = nullptr;
    int count = 0;
    while (state->offset < start_offset + size - 1) {
        JSON_OBJECT_ELEMENT *element = (JSON_OBJECT_ELEMENT *)state->dom;
        element->ref_count = 1;
        state->dom += sizeof(JSON_OBJECT_ELEMENT);
        if (!previous) {
            object->start = element;
        } else {
            previous->next = element;
        }
        previous = element;
        M_HandleObjectElementWrapped(state, element);
        count++;
    }
    if (previous) {
        previous->next = nullptr;
    }
    if (!count) {
        object->start = nullptr;
    }
    object->ref_count = 1;
    object->length = count;
    ASSERT(state->offset + sizeof(char) <= state->size);
    ASSERT(state->src[state->offset] == '\0');
    state->offset++;
}

static void M_HandleObjectValue(M_STATE *state, JSON_VALUE *value)
{
    ASSERT(state != nullptr);
    ASSERT(value != nullptr);

    JSON_OBJECT *object = (JSON_OBJECT *)state->dom;
    object->ref_count = 1;
    state->dom += sizeof(JSON_OBJECT);

    M_HandleObject(state, object);

    value->type = JSON_TYPE_OBJECT;
    value->payload = object;
}

static void M_HandleValue(M_STATE *state, JSON_VALUE *value, uint8_t marker)
{
    ASSERT(state != nullptr);
    ASSERT(value != nullptr);
    switch (marker) {
    case 0x01:
        M_HandleDoubleValue(state, value);
        break;
    case 0x02:
        M_HandleStringValue(state, value);
        break;
    case 0x03:
        M_HandleObjectValue(state, value);
        break;
    case 0x04:
        M_HandleArrayValue(state, value);
        break;
    case 0x0A:
        M_HandleNullValue(state, value);
        break;
    case 0x08:
        M_HandleBoolValue(state, value);
        break;
    case 0x10:
        M_HandleInt32Value(state, value);
        break;
    default:
        ASSERT_FAIL();
    }
}

JSON_VALUE *BSON_Parse(const char *src, size_t src_size)
{
    return BSON_ParseEx(src, src_size, nullptr);
}

JSON_VALUE *BSON_ParseEx(
    const char *src, size_t src_size, BSON_PARSE_RESULT *result)
{
    M_STATE state;
    void *allocation;
    JSON_VALUE *value;
    size_t total_size;

    if (result) {
        result->error = BSON_PARSE_ERROR_NONE;
        result->error_offset = 0;
    }

    if (!src) {
        return nullptr;
    }

    state.src = src;
    state.size = src_size;
    state.offset = 0;
    state.error = BSON_PARSE_ERROR_NONE;
    state.dom_size = 0;
    state.data_size = 0;

    if (M_GetRootSize(&state)) {
        if (state.offset != state.size) {
            state.error = BSON_PARSE_ERROR_UNEXPECTED_TRAILING_BYTES;
        }
    }

    if (state.error != BSON_PARSE_ERROR_NONE) {
        if (result) {
            result->error = state.error;
            result->error_offset = state.offset;
        }
        LOG_ERROR(
            "Error while reading BSON near offset %d: %s", state.offset,
            BSON_GetErrorDescription(state.error));
        return nullptr;
    }

    total_size = state.dom_size + state.data_size;

    allocation = Memory_Alloc(total_size);
    state.offset = 0;
    state.dom = (char *)allocation;
    state.data = state.dom + state.dom_size;

    // assume the root element to be an object
    value = (JSON_VALUE *)state.dom;
    value->ref_count = 0;
    state.dom += sizeof(JSON_VALUE);
    M_HandleObjectValue(&state, value);

    ASSERT(state.dom == (char *)allocation + state.dom_size);
    ASSERT(state.data == (char *)allocation + state.dom_size + state.data_size);

    return value;
}

const char *BSON_GetErrorDescription(BSON_PARSE_ERROR error)
{
    switch (error) {
    case BSON_PARSE_ERROR_NONE:
        return "no error";

    case BSON_PARSE_ERROR_INVALID_VALUE:
        return "invalid value";

    case BSON_PARSE_ERROR_PREMATURE_END_OF_BUFFER:
        return "premature end of buffer";

    case BSON_PARSE_ERROR_UNEXPECTED_TRAILING_BYTES:
        return "unexpected trailing bytes";

    case BSON_PARSE_ERROR_UNKNOWN:
    default:
        return "unknown";
    }
}
