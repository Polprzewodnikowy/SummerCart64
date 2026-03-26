#include <trx/core/json/base.h>

#include <trx/core/memory.h>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static JSON_NUMBER *M_NumberNewInt(const int number)
{
    const size_t size = snprintf(nullptr, 0, "%d", number) + 1;
    char *const buf = Memory_Alloc(size);
    sprintf(buf, "%d", number);
    JSON_NUMBER *const elem = Memory_Alloc(sizeof(JSON_NUMBER));
    elem->number = buf;
    elem->number_size = strlen(buf);
    return elem;
}

static JSON_NUMBER *M_NumberNewInt64(const int64_t number)
{
    const size_t size = snprintf(nullptr, 0, "%" PRId64, number) + 1;
    char *const buf = Memory_Alloc(size);
    sprintf(buf, "%" PRId64, number);
    JSON_NUMBER *const elem = Memory_Alloc(sizeof(JSON_NUMBER));
    elem->number = buf;
    elem->number_size = strlen(buf);
    return elem;
}

static JSON_NUMBER *M_NumberNewDouble(const double number)
{
    const size_t size = snprintf(nullptr, 0, "%f", number) + 3;
    char *const buf = Memory_Alloc(size);
    sprintf(buf, "%f", number);

    // Remove trailing zeros, keeping at least one digit after the decimal point
    char *const dot = strchr(buf, '.');
    if (dot == nullptr) {
        strcat(buf, ".0");
    } else {
        char *end = buf + strlen(buf) - 1;
        while (end > dot && *end == '0') {
            end--;
        }
        if (*end == '.') {
            // All fractional digits removed => append a single 0 to get "1.0".
            end[1] = '0';
            end[2] = '\0';
        } else {
            // Terminate string after the last non-zero digit to get "1.123".
            end[1] = '\0';
        }
    }

    JSON_NUMBER *const elem = Memory_Alloc(sizeof(JSON_NUMBER));
    elem->number = buf;
    elem->number_size = strlen(buf);
    return elem;
}

static void M_NumberFree(JSON_NUMBER *const num)
{
    if (num->ref_count == 0) {
        Memory_Free(num->number);
        Memory_Free(num);
    }
}

static JSON_STRING *M_StringNew(const char *const string)
{
    JSON_STRING *const str = Memory_Alloc(sizeof(JSON_STRING));
    str->string = Memory_DupStr(string);
    str->string_size = strlen(string);
    return str;
}

static void M_StringFree(JSON_STRING *const str)
{
    if (str->ref_count == 0) {
        Memory_Free(str->string);
        Memory_Free(str);
    }
}

static JSON_VALUE *M_ValueFromNumber(JSON_NUMBER *const num)
{
    JSON_VALUE *const value = Memory_Alloc(sizeof(JSON_VALUE));
    value->type = JSON_TYPE_NUMBER;
    value->payload = num;
    return value;
}

static void M_ArrayElementFree(JSON_ARRAY_ELEMENT *const element)
{
    if (element->ref_count == 0) {
        Memory_Free(element);
    }
}

static void M_ObjectElementFree(JSON_OBJECT_ELEMENT *element)
{
    if (element->ref_count == 0) {
        Memory_FreePointer(&element);
    }
}

JSON_VALUE *JSON_ValueFromBool(const int b)
{
    JSON_VALUE *const value = Memory_Alloc(sizeof(JSON_VALUE));
    value->type = b ? JSON_TYPE_TRUE : JSON_TYPE_FALSE;
    value->payload = nullptr;
    return value;
}

JSON_VALUE *JSON_ValueFromInt(const int number)
{
    return M_ValueFromNumber(M_NumberNewInt(number));
}

JSON_VALUE *JSON_ValueFromInt64(const int64_t number)
{
    return M_ValueFromNumber(M_NumberNewInt64(number));
}

JSON_VALUE *JSON_ValueFromDouble(const double number)
{
    return M_ValueFromNumber(M_NumberNewDouble(number));
}

JSON_VALUE *JSON_ValueFromString(const char *const string)
{
    JSON_VALUE *const value = Memory_Alloc(sizeof(JSON_VALUE));
    value->type = JSON_TYPE_STRING;
    value->payload = M_StringNew(string);
    return value;
}

JSON_VALUE *JSON_ValueFromArray(JSON_ARRAY *const arr)
{
    JSON_VALUE *const value = Memory_Alloc(sizeof(JSON_VALUE));
    value->type = JSON_TYPE_ARRAY;
    value->payload = arr;
    return value;
}

JSON_VALUE *JSON_ValueFromObject(JSON_OBJECT *const obj)
{
    JSON_VALUE *const value = Memory_Alloc(sizeof(JSON_VALUE));
    value->type = JSON_TYPE_OBJECT;
    value->payload = obj;
    return value;
}

void JSON_ValueFree(JSON_VALUE *const value)
{
    if (value == nullptr) {
        return;
    }

    switch (value->type) {
    case JSON_TYPE_NUMBER:
        M_NumberFree((JSON_NUMBER *)value->payload);
        break;
    case JSON_TYPE_STRING:
        M_StringFree((JSON_STRING *)value->payload);
        break;
    case JSON_TYPE_ARRAY:
        JSON_ArrayFree((JSON_ARRAY *)value->payload);
        break;
    case JSON_TYPE_OBJECT:
        JSON_ObjectFree((JSON_OBJECT *)value->payload);
        break;
    case JSON_TYPE_TRUE:
    case JSON_TYPE_NULL:
    case JSON_TYPE_FALSE:
        break;
    }

    if (value->ref_count == 0) {
        Memory_Free(value);
    }
}

bool JSON_ValueIsNull(const JSON_VALUE *const value)
{
    return value != nullptr && value->type == JSON_TYPE_NULL;
}

bool JSON_ValueIsTrue(const JSON_VALUE *const value)
{
    return value != nullptr && value->type == JSON_TYPE_TRUE;
}

bool JSON_ValueIsFalse(const JSON_VALUE *const value)
{
    return value != nullptr && value->type == JSON_TYPE_FALSE;
}

int JSON_ValueGetBool(const JSON_VALUE *const value, const int d)
{
    if (JSON_ValueIsTrue(value)) {
        return 1;
    } else if (JSON_ValueIsFalse(value)) {
        return 0;
    }
    return d;
}

const JSON_NUMBER *JSON_ValueGetNumber(const JSON_VALUE *const value)
{
    if (value == nullptr || value->type != JSON_TYPE_NUMBER) {
        return nullptr;
    }
    return (const JSON_NUMBER *)value->payload;
}

int JSON_ValueGetInt(const JSON_VALUE *const value, const int d)
{
    const JSON_NUMBER *const num = JSON_ValueGetNumber(value);
    if (num == nullptr) {
        return d;
    }
    const char *const s = num->number;
    if (strncmp(s, "0x", 2) == 0 || strncmp(s, "0X", 2) == 0) {
        return strtol(s, nullptr, 16);
    }
    if (strncmp(s, "0b", 2) == 0 || strncmp(s, "0B", 2) == 0) {
        return strtol(s + 2, nullptr, 2);
    }
    return atoi(s);
}

int64_t JSON_ValueGetInt64(const JSON_VALUE *const value, const int64_t d)
{
    const JSON_NUMBER *const num = JSON_ValueGetNumber(value);
    return num != nullptr ? strtoll(num->number, nullptr, 10) : d;
}

double JSON_ValueGetDouble(const JSON_VALUE *const value, const double d)
{
    const JSON_NUMBER *const num = JSON_ValueGetNumber(value);
    return num != nullptr ? atof(num->number) : d;
}

const char *JSON_ValueGetString(
    const JSON_VALUE *const value, const char *const d)
{
    if (value == nullptr || value->type != JSON_TYPE_STRING) {
        return nullptr;
    }
    const JSON_STRING *const string = value->payload;
    return string != nullptr ? string->string : d;
}

JSON_ARRAY *JSON_ValueAsArray_Impl(const JSON_VALUE *const value)
{
    if (value == nullptr || value->type != JSON_TYPE_ARRAY) {
        return nullptr;
    }
    return (JSON_ARRAY *)value->payload;
}

JSON_OBJECT *JSON_ValueAsObject_Impl(const JSON_VALUE *const value)
{
    if (value == nullptr || value->type != JSON_TYPE_OBJECT) {
        return nullptr;
    }
    return (JSON_OBJECT *)value->payload;
}

JSON_ARRAY *JSON_ArrayNew(void)
{
    JSON_ARRAY *const arr = Memory_Alloc(sizeof(JSON_ARRAY));
    arr->start = nullptr;
    arr->length = 0;
    return arr;
}

void JSON_ArrayFree(JSON_ARRAY *const arr)
{
    JSON_ARRAY_ELEMENT *elem = arr->start;
    while (elem) {
        JSON_ARRAY_ELEMENT *const next = elem->next;
        JSON_ValueFree(elem->value);
        M_ArrayElementFree(elem);
        elem = next;
    }
    if (arr->ref_count == 0) {
        Memory_Free(arr);
    }
}

void JSON_ArrayAppend(JSON_ARRAY *const arr, JSON_VALUE *const value)
{
    JSON_ARRAY_ELEMENT *elem = Memory_Alloc(sizeof(JSON_ARRAY_ELEMENT));
    elem->value = value;
    elem->next = nullptr;
    if (arr->start) {
        JSON_ARRAY_ELEMENT *target = arr->start;
        while (target->next) {
            target = target->next;
        }
        target->next = elem;
    } else {
        arr->start = elem;
    }
    arr->length++;
}

void JSON_ArrayAppendBool(JSON_ARRAY *const arr, const int b)
{
    JSON_ArrayAppend(arr, JSON_ValueFromBool(b));
}

void JSON_ArrayAppendInt(JSON_ARRAY *const arr, const int number)
{
    JSON_ArrayAppend(arr, JSON_ValueFromInt(number));
}

void JSON_ArrayAppendDouble(JSON_ARRAY *const arr, const double number)
{
    JSON_ArrayAppend(arr, JSON_ValueFromDouble(number));
}

void JSON_ArrayAppendString(JSON_ARRAY *const arr, const char *string)
{
    JSON_ArrayAppend(arr, JSON_ValueFromString(string));
}

void JSON_ArrayAppendArray(JSON_ARRAY *const arr, JSON_ARRAY *const arr2)
{
    JSON_ArrayAppend(arr, JSON_ValueFromArray(arr2));
}

void JSON_ArrayAppendObject(JSON_ARRAY *const arr, JSON_OBJECT *const obj)
{
    JSON_ArrayAppend(arr, JSON_ValueFromObject(obj));
}

JSON_VALUE *JSON_ArrayGetValue(const JSON_ARRAY *const arr, const size_t idx)
{
    if (arr == nullptr || idx >= arr->length) {
        return nullptr;
    }
    JSON_ARRAY_ELEMENT *elem = arr->start;
    for (size_t i = 0; i < idx; i++) {
        elem = elem->next;
    }
    return elem->value;
}

int JSON_ArrayGetBool(
    const JSON_ARRAY *const arr, const size_t idx, const int d)
{
    const JSON_VALUE *const value = JSON_ArrayGetValue(arr, idx);
    return JSON_ValueGetBool(value, d);
}

int JSON_ArrayGetInt(const JSON_ARRAY *const arr, const size_t idx, const int d)
{
    const JSON_VALUE *const value = JSON_ArrayGetValue(arr, idx);
    return JSON_ValueGetInt(value, d);
}

double JSON_ArrayGetDouble(
    const JSON_ARRAY *const arr, const size_t idx, const double d)
{
    const JSON_VALUE *const value = JSON_ArrayGetValue(arr, idx);
    return JSON_ValueGetDouble(value, d);
}

const char *JSON_ArrayGetString(
    const JSON_ARRAY *const arr, const size_t idx, const char *const d)
{
    const JSON_VALUE *const value = JSON_ArrayGetValue(arr, idx);
    return JSON_ValueGetString(value, d);
}

JSON_ARRAY *JSON_ArrayGetArray_Impl(
    const JSON_ARRAY *const arr, const size_t idx)
{
    JSON_VALUE *const value = JSON_ArrayGetValue(arr, idx);
    return JSON_ValueAsArray(value);
}

JSON_OBJECT *JSON_ArrayGetObject_Impl(
    const JSON_ARRAY *const arr, const size_t idx)
{
    JSON_VALUE *const value = JSON_ArrayGetValue(arr, idx);
    return JSON_ValueAsObject(value);
}

JSON_OBJECT *JSON_ObjectNew(void)
{
    JSON_OBJECT *const obj = Memory_Alloc(sizeof(JSON_OBJECT));
    obj->start = nullptr;
    obj->length = 0;
    return obj;
}

void JSON_ObjectFree(JSON_OBJECT *const obj)
{
    JSON_OBJECT_ELEMENT *elem = obj->start;
    while (elem) {
        JSON_OBJECT_ELEMENT *next = elem->next;
        M_StringFree(elem->name);
        JSON_ValueFree(elem->value);
        M_ObjectElementFree(elem);
        elem = next;
    }
    if (obj->ref_count == 0) {
        Memory_Free(obj);
    }
}

void JSON_ObjectAppend(
    JSON_OBJECT *const obj, const char *const key, JSON_VALUE *const value)
{
    JSON_OBJECT_ELEMENT *elem = Memory_Alloc(sizeof(JSON_OBJECT_ELEMENT));
    elem->name = M_StringNew(key);
    elem->value = value;
    elem->next = nullptr;
    if (obj->start) {
        JSON_OBJECT_ELEMENT *target = obj->start;
        while (target->next) {
            target = target->next;
        }
        target->next = elem;
    } else {
        obj->start = elem;
    }
    obj->length++;
}

void JSON_ObjectAppendBool(
    JSON_OBJECT *const obj, const char *const key, const int b)
{
    JSON_ObjectAppend(obj, key, JSON_ValueFromBool(b));
}

void JSON_ObjectAppendInt(
    JSON_OBJECT *const obj, const char *const key, const int number)
{
    JSON_ObjectAppend(obj, key, JSON_ValueFromInt(number));
}

void JSON_ObjectAppendInt64(
    JSON_OBJECT *const obj, const char *const key, const int64_t number)
{
    JSON_ObjectAppend(obj, key, JSON_ValueFromInt64(number));
}

void JSON_ObjectAppendDouble(
    JSON_OBJECT *const obj, const char *const key, const double number)
{
    JSON_ObjectAppend(obj, key, JSON_ValueFromDouble(number));
}

void JSON_ObjectAppendString(
    JSON_OBJECT *const obj, const char *const key, const char *string)
{
    JSON_ObjectAppend(obj, key, JSON_ValueFromString(string));
}

void JSON_ObjectAppendArray(
    JSON_OBJECT *const obj, const char *const key, JSON_ARRAY *const arr)
{
    JSON_ObjectAppend(obj, key, JSON_ValueFromArray(arr));
}

void JSON_ObjectAppendObject(
    JSON_OBJECT *const obj, const char *const key, JSON_OBJECT *const obj2)
{
    JSON_ObjectAppend(obj, key, JSON_ValueFromObject(obj2));
}

bool JSON_ObjectContainsKey(JSON_OBJECT *const obj, const char *const key)
{
    JSON_OBJECT_ELEMENT *elem = obj->start;
    while (elem != nullptr) {
        if (!strcmp(elem->name->string, key)) {
            return true;
        }

        elem = elem->next;
    }

    return false;
}

void JSON_ObjectEvictKey(JSON_OBJECT *const obj, const char *const key)
{
    if (obj == nullptr) {
        return;
    }
    JSON_OBJECT_ELEMENT *elem = obj->start;
    JSON_OBJECT_ELEMENT *prev = nullptr;
    while (elem) {
        if (!strcmp(elem->name->string, key)) {
            if (prev == nullptr) {
                obj->start = elem->next;
            } else {
                prev->next = elem->next;
            }
            M_StringFree(elem->name);
            JSON_ValueFree(elem->value);
            M_ObjectElementFree(elem);
            obj->length--;
            return;
        }
        prev = elem;
        elem = elem->next;
    }
}

void JSON_ObjectMerge(JSON_OBJECT *const root, const JSON_OBJECT *const obj)
{
    JSON_OBJECT_ELEMENT *elem = obj->start;
    while (elem != nullptr) {
        JSON_ObjectEvictKey(root, elem->name->string);
        JSON_ObjectAppend(root, elem->name->string, elem->value);
        elem = elem->next;
    }
}

JSON_VALUE *JSON_ObjectGetValue(
    const JSON_OBJECT *const obj, const char *const key)
{
    if (obj == nullptr) {
        return nullptr;
    }
    JSON_OBJECT_ELEMENT *elem = obj->start;
    while (elem) {
        if (!strcmp(elem->name->string, key)) {
            return elem->value;
        }
        elem = elem->next;
    }
    return nullptr;
}

int JSON_ObjectGetBool(
    const JSON_OBJECT *const obj, const char *const key, const int d)
{
    const JSON_VALUE *const value = JSON_ObjectGetValue(obj, key);
    return JSON_ValueGetBool(value, d);
}

int JSON_ObjectGetInt(
    const JSON_OBJECT *const obj, const char *const key, const int d)
{
    const JSON_VALUE *const value = JSON_ObjectGetValue(obj, key);
    return JSON_ValueGetInt(value, d);
}

int64_t JSON_ObjectGetInt64(
    const JSON_OBJECT *const obj, const char *const key, const int64_t d)
{
    const JSON_VALUE *const value = JSON_ObjectGetValue(obj, key);
    return JSON_ValueGetInt64(value, d);
}

double JSON_ObjectGetDouble(
    const JSON_OBJECT *const obj, const char *const key, const double d)
{
    const JSON_VALUE *const value = JSON_ObjectGetValue(obj, key);
    return JSON_ValueGetDouble(value, d);
}

const char *JSON_ObjectGetString(
    const JSON_OBJECT *const obj, const char *const key, const char *const d)
{
    const JSON_VALUE *const value = JSON_ObjectGetValue(obj, key);
    return JSON_ValueGetString(value, d);
}

JSON_ARRAY *JSON_ObjectGetArray_Impl(
    const JSON_OBJECT *const obj, const char *const key)
{
    JSON_VALUE *const value = JSON_ObjectGetValue(obj, key);
    return JSON_ValueAsArray(value);
}

JSON_OBJECT *JSON_ObjectGetObject_Impl(
    const JSON_OBJECT *const obj, const char *const key)
{
    JSON_VALUE *const value = JSON_ObjectGetValue(obj, key);
    return JSON_ValueAsObject(value);
}
