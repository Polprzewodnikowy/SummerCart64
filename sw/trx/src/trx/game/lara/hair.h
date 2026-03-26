#pragma once

#include <trx/core/math.h>
#include <trx/game/objects/types.h>

typedef struct {
    XYZ_32 pos;
    XYZ_16 rot;
    struct {
        struct {
            XYZ_32 pos;
            XYZ_16 rot;
        } result, prev;
    } interp;
} HAIR_SEGMENT;

void Lara_Hair_Initialise(void);
bool Lara_Hair_IsActive(void);
void Lara_Hair_Control(bool in_cutscene);
void Lara_Hair_Draw(void);

int32_t Lara_Hair_GetSegmentCount(void);
HAIR_SEGMENT *Lara_Hair_GetSegment(int32_t n);
