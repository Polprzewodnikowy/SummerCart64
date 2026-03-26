#pragma once

#include <trx/game/items/types.h>
#include <trx/game/types.h>

#include <stdint.h>

typedef struct {
    int32_t x;
    int32_t y;
    int32_t z;
    int32_t prev_size;
    int32_t prev_life;
    int32_t prev_init;
    uint8_t flags;
    uint8_t life;
    uint8_t size;
    uint8_t init;
} FX_WATER_RIPPLE;

typedef struct {
    int16_t wx;
    int16_t wy;
    int16_t wz;
    int16_t prev_wx;
    int16_t prev_wy;
    int16_t prev_wz;
    int16_t xv;
    int32_t yv;
    int16_t zv;
    int16_t oxv;
    int16_t ozv;
    uint8_t friction;
    uint8_t gravity;
} FX_WATER_SPLASH_VERT;

typedef struct {
    int32_t x;
    int32_t y;
    int32_t z;
    int32_t prev_life;
    uint8_t flags;
    uint8_t life;
    uint8_t pad[2];
    FX_WATER_SPLASH_VERT v[48];
} FX_WATER_SPLASH;

typedef struct {
    int32_t x;
    int32_t y;
    int32_t z;
    int16_t inner_xz_off;
    int16_t inner_xz_size;
    int16_t inner_y_size;
    int16_t inner_xz_vel;
    int16_t inner_y_vel;
    int16_t inner_gravity;
    int16_t inner_friction;
    int16_t middle_xz_off;
    int16_t middle_xz_size;
    int16_t middle_y_size;
    int16_t middle_xz_vel;
    int16_t middle_y_vel;
    int16_t middle_gravity;
    int16_t middle_friction;
    int16_t outer_xz_off;
    int16_t outer_xz_size;
    int16_t outer_xz_vel;
    int16_t outer_friction;
} FX_WATER_SPLASH_SETUP;

void FX_Water_Reset(void);
void FX_Water_Control(void);
void FX_Water_Draw(void);

FX_WATER_RIPPLE *FX_Water_SetupRipple(
    int32_t x, int32_t y, int32_t z, int32_t size, bool is_still);
void FX_Water_SetupSplash(const FX_WATER_SPLASH_SETUP *setup);
void FX_Water_Splash(const ITEM *item);
void FX_Water_WadeSplash(const ITEM *item, int32_t water_height, int32_t depth);

void FX_Water_TriggerUnderwaterBlood(XYZ_32 pos, int32_t size);
void FX_Water_TriggerUnderwaterBloodD(XYZ_32 pos, int32_t size);
