#pragma once

#include <trx/game/objects/ids.h>

#include <stdint.h>

typedef struct {
    OBJECT_ID object_id;
    const char *matched_name;
} OBJECT_NAME_MATCH;

// Get the current name for an object (may change on language reload).
const char *Object_GetName(OBJECT_ID obj_id);

// Get a stable pointer-to-pointer for the object name, content of which
// automatically udpates on each language reload.
const char *const *Object_GetNamePtr(OBJECT_ID obj_id);
const char *Object_GetDescription(OBJECT_ID obj_id);

void Object_ResetAllNames(void);
void Object_ClearNames(OBJECT_ID obj_id);
void Object_AddName(OBJECT_ID obj_id, const char *name);

void Object_SetDescription(OBJECT_ID obj_id, const char *description);

// Return a list of matching names, with an optional filter callback to only
// consider objects satisfying certain criteria. out_match_count may be
// nullptr. The result must be freed by the caller with Memory_Free().
OBJECT_NAME_MATCH *Object_IdsFromName(
    const char *name, int32_t *out_match_count, bool (*filter)(OBJECT_ID));

// Return an unique object id for a given programmatic string.
// Example:
//     Given a string "key_1", returns O_KEY_1.
OBJECT_ID Object_IdFromKey(const char *key);
