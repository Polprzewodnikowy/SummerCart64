#include <trx/core/json/util/write_io.h>

#include <trx/core/json.h>
#include <trx/core/memory.h>
#include <trx/debug.h>

#define M_MAX_STACK_SIZE 10

typedef struct JSON_WRITE_IO {
    JSON_VALUE *stack[M_MAX_STACK_SIZE];
    JSON_VALUE *current;
    size_t current_pos;
} JSON_WRITE_IO;

static void M_PushValue(JSON_WRITE_IO *const io, JSON_VALUE *const value)
{
    io->current_pos++;
    io->stack[io->current_pos] = value;
    io->current = value;
}

static void M_Pop(JSON_WRITE_IO *const io)
{
    io->current_pos--;
    io->current = io->stack[io->current_pos];
}

void JSON_WriteIO_PushObject(JSON_WRITE_IO *const io)
{
    JSON_OBJECT *const child = JSON_ObjectNew();
    M_PushValue(io, JSON_ValueFromObject(child));
}

void JSON_WriteIO_PushArray(JSON_WRITE_IO *const io)
{
    JSON_ARRAY *const child = JSON_ArrayNew();
    M_PushValue(io, JSON_ValueFromArray(child));
}

void JSON_WriteIO_PopAndSet(JSON_WRITE_IO *const io, const char *const key)
{
    JSON_OBJECT *const parent =
        JSON_ValueAsObject(io->stack[io->current_pos - 1]);
    ASSERT(parent != nullptr);
    JSON_ObjectAppend(parent, key, io->current);
    M_Pop(io);
}

void JSON_WriteIO_DiscardCurrent(JSON_WRITE_IO *const io)
{
    JSON_ValueFree(io->current);
    M_Pop(io);
}

void JSON_WriteIO_PopAndSetNZ(JSON_WRITE_IO *const io, const char *const key)
{
    const JSON_OBJECT *const obj = JSON_ValueAsObject(io->current);
    if (obj != nullptr && obj->length == 0) {
        JSON_WriteIO_DiscardCurrent(io);
        return;
    }

    const JSON_ARRAY *const arr = JSON_ValueAsArray(io->current);
    if (arr != nullptr && arr->length == 0) {
        JSON_WriteIO_DiscardCurrent(io);
        return;
    }

    JSON_WriteIO_PopAndSet(io, key);
}

void JSON_WriteIO_PopAndAppend(JSON_WRITE_IO *const io)
{
    JSON_ARRAY *const parent =
        JSON_ValueAsArray(io->stack[io->current_pos - 1]);
    ASSERT(parent != nullptr);
    JSON_ArrayAppend(parent, io->current);
    M_Pop(io);
}

JSON_OBJECT *JSON_WriteIO_GetCurrentObject(JSON_WRITE_IO *const io)
{
    return JSON_ValueAsObject(io->current);
}

void JSON_WriteIO_PushString(JSON_WRITE_IO *const io, const char *const value)
{
    M_PushValue(io, JSON_ValueFromString(value));
}

void JSON_WriteIO_PushBool(JSON_WRITE_IO *const io, const bool value)
{
    M_PushValue(io, JSON_ValueFromBool(value));
}

void JSON_WriteIO_PushInt(JSON_WRITE_IO *const io, const int32_t value)
{
    M_PushValue(io, JSON_ValueFromInt(value));
}

void JSON_WriteIO_PushDouble(JSON_WRITE_IO *const io, const double value)
{
    M_PushValue(io, JSON_ValueFromDouble(value));
}

void JSON_WriteIO_PushXYZ16(JSON_WRITE_IO *const io, const XYZ_16 value)
{
    JSON_WriteIO_PushObject(io);
    JSON_WriteIO_PushInt(io, value.x);
    JSON_WriteIO_PopAndSet(io, "x");
    JSON_WriteIO_PushInt(io, value.y);
    JSON_WriteIO_PopAndSet(io, "y");
    JSON_WriteIO_PushInt(io, value.z);
    JSON_WriteIO_PopAndSet(io, "z");
}

void JSON_WriteIO_PushXYZ32(JSON_WRITE_IO *const io, const XYZ_32 value)
{
    JSON_WriteIO_PushObject(io);
    JSON_WriteIO_PushInt(io, value.x);
    JSON_WriteIO_PopAndSet(io, "x");
    JSON_WriteIO_PushInt(io, value.y);
    JSON_WriteIO_PopAndSet(io, "y");
    JSON_WriteIO_PushInt(io, value.z);
    JSON_WriteIO_PopAndSet(io, "z");
}

JSON_WRITE_IO *JSON_WriteIO_Create(void)
{
    JSON_WRITE_IO *const io = Memory_Alloc(sizeof(*io));
    JSON_OBJECT *const root_obj = JSON_ObjectNew();
    io->stack[0] = JSON_ValueFromObject(root_obj);
    io->current = io->stack[0];
    return io;
}

void JSON_WriteIO_Destroy(JSON_WRITE_IO *const io)
{
    JSON_ValueFree(io->stack[0]);
    Memory_Free(io);
}

JSON_VALUE *JSON_WriteIO_GetRoot(JSON_WRITE_IO *const io)
{
    ASSERT(io->stack[0] != nullptr);
    return io->stack[0];
}
