#include <trx/core/vector.h>

#include <trx/core/memory.h>
#include <trx/debug.h>

#include <stdint.h>
#include <string.h>

#define VECTOR_DEFAULT_CAPACITY 4
#define VECTOR_GROWTH_RATE 2
#define P(obj) ((*obj->priv))

struct VECTOR_PRIV {
    char *items;
};

static void M_EnsureCapacity(VECTOR *const vector, const int32_t n)
{
    while (vector->count + n > vector->capacity) {
        vector->capacity *= VECTOR_GROWTH_RATE;
        P(vector).items = Memory_Realloc(
            P(vector).items, vector->item_size * vector->capacity);
    }
}

VECTOR *Vector_Create(const size_t item_size)
{
    return Vector_CreateAtCapacity(item_size, VECTOR_DEFAULT_CAPACITY);
}

VECTOR *Vector_CreateAtCapacity(const size_t item_size, const int32_t capacity)
{
    VECTOR *const vector = Memory_Alloc(sizeof(VECTOR));
    vector->count = 0;
    vector->capacity = capacity;
    vector->item_size = item_size;
    vector->priv = Memory_Alloc(sizeof(struct VECTOR_PRIV));
    P(vector).items = Memory_Alloc(item_size * capacity);

    return vector;
}

void Vector_EnsureCapacity(VECTOR *const vector, const int32_t capacity)
{
    if (vector->capacity >= capacity) {
        return;
    }
    vector->capacity = capacity;
    P(vector).items =
        Memory_Realloc(P(vector).items, vector->item_size * capacity);
}

void Vector_Free(VECTOR *vector)
{
    if (vector == nullptr) {
        return;
    }
    Memory_FreePointer(&P(vector).items);
    Memory_FreePointer(&vector->priv);
    Memory_FreePointer(&vector);
}

int32_t Vector_IndexOf(const VECTOR *const vector, const void *const item)
{
    for (int32_t i = 0; i < vector->count; i++) {
        if (memcmp(
                P(vector).items + i * vector->item_size, item,
                vector->item_size)
            == 0) {
            return i;
        }
    }
    return -1;
}

int32_t Vector_LastIndexOf(const VECTOR *const vector, const void *const item)
{
    const char *const items = P(vector).items;
    for (int32_t i = vector->count - 1; i >= 0; i--) {
        if (memcmp(items + i * vector->item_size, item, vector->item_size)
            == 0) {
            return i;
        }
    }
    return -1;
}

bool Vector_Contains(const VECTOR *const vector, const void *const item)
{
    return Vector_IndexOf(vector, item) != -1;
}

void *Vector_Get(const VECTOR *const vector, const int32_t index)
{
    ASSERT(index >= 0 && index < vector->count);
    char *const items = P(vector).items;
    return (void *)(items + index * vector->item_size);
}

void *Vector_GetData(const VECTOR *const vector)
{
    return P(vector).items;
}

void *Vector_Expand(VECTOR *const vector, const int32_t count)
{
    ASSERT(count >= 0);
    M_EnsureCapacity(vector, count);
    char *const items = P(vector).items;
    void *const result = items + vector->count * vector->item_size;
    vector->count += count;
    return result;
}

void Vector_Add(VECTOR *const vector, const void *const item)
{
    memcpy(Vector_Expand(vector, 1), item, vector->item_size);
}

void Vector_Insert(
    VECTOR *const vector, const int32_t index, const void *const item)
{
    ASSERT(index >= 0 && index <= vector->count);
    M_EnsureCapacity(vector, 1);
    char *const items = P(vector).items;
    if (index < vector->count) {
        memmove(
            items + (index + 1) * vector->item_size,
            items + index * vector->item_size,
            (vector->count - index) * vector->item_size);
    }
    memcpy(items + index * vector->item_size, item, vector->item_size);
    vector->count++;
}

void Vector_Swap(
    VECTOR *const vector, const int32_t index1, const int32_t index2)
{
    ASSERT(index1 >= 0 && index1 < vector->count);
    ASSERT(index2 >= 0 && index2 < vector->count);
    if (index1 == index2) {
        return;
    }
    char *const items = P(vector).items;
    void *tmp = Memory_Alloc(vector->item_size);
    memcpy(tmp, items + index1 * vector->item_size, vector->item_size);
    memcpy(
        items + index1 * vector->item_size, items + index2 * vector->item_size,
        vector->item_size);
    memcpy(items + index2 * vector->item_size, tmp, vector->item_size);
    Memory_FreePointer(&tmp);
}

bool Vector_Remove(VECTOR *const vector, const void *item)
{
    const int32_t index = Vector_IndexOf(vector, item);
    if (index == -1) {
        return false;
    }
    Vector_RemoveAt(vector, index);
    return true;
}

void Vector_RemoveAt(VECTOR *const vector, const int32_t index)
{
    ASSERT(index >= 0 && index < vector->count);
    char *const items = P(vector).items;
    memset(items + index * vector->item_size, 0, vector->item_size);
    if (index + 1 < vector->count) {
        memmove(
            items + index * vector->item_size,
            items + (index + 1) * vector->item_size,
            (vector->count - (index + 1)) * vector->item_size);
    }
    vector->count--;
}

void Vector_Reverse(VECTOR *const vector)
{
    int32_t i = 0;
    int32_t j = vector->count - 1;
    void *tmp = Memory_Alloc(vector->item_size);
    char *const items = P(vector).items;
    for (; i < j; i++, j--) {
        memcpy(tmp, items + i * vector->item_size, vector->item_size);
        memcpy(
            items + i * vector->item_size, items + j * vector->item_size,
            vector->item_size);
        memcpy(items + j * vector->item_size, tmp, vector->item_size);
    }
    Memory_FreePointer(&tmp);
}

void Vector_Clear(VECTOR *const vector)
{
    vector->count = 0;
}

void Vector_ClearRealloc(VECTOR *const vector)
{
    vector->count = 0;
    vector->capacity = VECTOR_DEFAULT_CAPACITY;
    P(vector).items =
        Memory_Realloc(P(vector).items, vector->item_size * vector->capacity);
    memset(P(vector).items, 0, vector->item_size * vector->count);
}
