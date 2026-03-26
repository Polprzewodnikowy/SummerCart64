#pragma once

#include <trx/core/json/enum.h>
#include <trx/core/json/types.h>

// values
JSON_VALUE *JSON_ValueFromBool(int b);
JSON_VALUE *JSON_ValueFromInt(int number);
JSON_VALUE *JSON_ValueFromInt64(int64_t number);
JSON_VALUE *JSON_ValueFromDouble(double number);
JSON_VALUE *JSON_ValueFromString(const char *string);
JSON_VALUE *JSON_ValueFromArray(JSON_ARRAY *arr);
JSON_VALUE *JSON_ValueFromObject(JSON_OBJECT *obj);
void JSON_ValueFree(JSON_VALUE *value);

bool JSON_ValueIsNull(const JSON_VALUE *value);
bool JSON_ValueIsTrue(const JSON_VALUE *value);
bool JSON_ValueIsFalse(const JSON_VALUE *value);
int JSON_ValueGetBool(const JSON_VALUE *value, int d);
int JSON_ValueGetInt(const JSON_VALUE *value, int d);
int64_t JSON_ValueGetInt64(const JSON_VALUE *value, int64_t d);
double JSON_ValueGetDouble(const JSON_VALUE *value, double d);
const JSON_NUMBER *JSON_ValueGetNumber(const JSON_VALUE *value);
const char *JSON_ValueGetString(const JSON_VALUE *value, const char *d);

JSON_ARRAY *JSON_ValueAsArray_Impl(const JSON_VALUE *value);
#define JSON_ValueAsArray(value)                                               \
    JSON_CONST_DISPATCH(                                                       \
        value, const JSON_ARRAY *, JSON_ValueAsArray_Impl(value))

JSON_OBJECT *JSON_ValueAsObject_Impl(const JSON_VALUE *value);
#define JSON_ValueAsObject(value)                                              \
    JSON_CONST_DISPATCH(                                                       \
        value, const JSON_OBJECT *, JSON_ValueAsObject_Impl(value))

// arrays
JSON_ARRAY *JSON_ArrayNew(void);
void JSON_ArrayFree(JSON_ARRAY *arr);

void JSON_ArrayAppend(JSON_ARRAY *arr, JSON_VALUE *value);
void JSON_ArrayAppendBool(JSON_ARRAY *arr, int b);
void JSON_ArrayAppendInt(JSON_ARRAY *arr, int number);
void JSON_ArrayAppendDouble(JSON_ARRAY *arr, double number);
void JSON_ArrayAppendString(JSON_ARRAY *arr, const char *string);
void JSON_ArrayAppendArray(JSON_ARRAY *arr, JSON_ARRAY *arr2);
void JSON_ArrayAppendObject(JSON_ARRAY *arr, JSON_OBJECT *obj);

JSON_VALUE *JSON_ArrayGetValue(const JSON_ARRAY *arr, size_t idx);
int JSON_ArrayGetBool(const JSON_ARRAY *arr, size_t idx, int d);
int JSON_ArrayGetInt(const JSON_ARRAY *arr, size_t idx, int d);
double JSON_ArrayGetDouble(const JSON_ARRAY *arr, size_t idx, double d);
const char *JSON_ArrayGetString(
    const JSON_ARRAY *arr, size_t idx, const char *d);

JSON_ARRAY *JSON_ArrayGetArray_Impl(const JSON_ARRAY *arr, size_t idx);
#define JSON_ArrayGetArray(value, ...)                                         \
    JSON_CONST_DISPATCH(                                                       \
        value, const JSON_ARRAY *,                                             \
        JSON_ArrayGetArray_Impl(value, __VA_ARGS__))

JSON_OBJECT *JSON_ArrayGetObject_Impl(const JSON_ARRAY *arr, size_t idx);
#define JSON_ArrayGetObject(value, ...)                                        \
    JSON_CONST_DISPATCH(                                                       \
        value, const JSON_ARRAY *,                                             \
        JSON_ArrayGetObject_Impl(value, __VA_ARGS__))

// objects
JSON_OBJECT *JSON_ObjectNew(void);
void JSON_ObjectFree(JSON_OBJECT *obj);

void JSON_ObjectAppend(JSON_OBJECT *obj, const char *key, JSON_VALUE *value);
void JSON_ObjectAppendBool(JSON_OBJECT *obj, const char *key, int b);
void JSON_ObjectAppendInt(JSON_OBJECT *obj, const char *key, int number);
void JSON_ObjectAppendInt64(JSON_OBJECT *obj, const char *key, int64_t number);
void JSON_ObjectAppendDouble(JSON_OBJECT *obj, const char *key, double number);
void JSON_ObjectAppendString(
    JSON_OBJECT *obj, const char *key, const char *string);
void JSON_ObjectAppendArray(JSON_OBJECT *obj, const char *key, JSON_ARRAY *arr);
void JSON_ObjectAppendObject(
    JSON_OBJECT *obj, const char *key, JSON_OBJECT *obj2);

bool JSON_ObjectContainsKey(JSON_OBJECT *obj, const char *key);
void JSON_ObjectEvictKey(JSON_OBJECT *obj, const char *key);
void JSON_ObjectMerge(JSON_OBJECT *root, const JSON_OBJECT *obj);

JSON_VALUE *JSON_ObjectGetValue(const JSON_OBJECT *obj, const char *key);
int JSON_ObjectGetBool(const JSON_OBJECT *obj, const char *key, int d);
int JSON_ObjectGetInt(const JSON_OBJECT *obj, const char *key, int d);
int64_t JSON_ObjectGetInt64(const JSON_OBJECT *obj, const char *key, int64_t d);
double JSON_ObjectGetDouble(const JSON_OBJECT *obj, const char *key, double d);
const char *JSON_ObjectGetString(
    const JSON_OBJECT *obj, const char *key, const char *d);

JSON_ARRAY *JSON_ObjectGetArray_Impl(const JSON_OBJECT *obj, const char *key);
#define JSON_ObjectGetArray(value, ...)                                        \
    JSON_CONST_DISPATCH(                                                       \
        value, const JSON_ARRAY *,                                             \
        JSON_ObjectGetArray_Impl(value, __VA_ARGS__))

JSON_OBJECT *JSON_ObjectGetObject_Impl(const JSON_OBJECT *obj, const char *key);
#define JSON_ObjectGetObject(value, ...)                                       \
    JSON_CONST_DISPATCH(                                                       \
        value, const JSON_OBJECT *,                                            \
        JSON_ObjectGetObject_Impl(value, __VA_ARGS__))
