#pragma once

#define MAX_TEXTURE_PAGES 128

#define TEXTURE_PAGE_WIDTH 256
#define TEXTURE_PAGE_HEIGHT 256
#define TEXTURE_PAGE_SIZE (TEXTURE_PAGE_WIDTH * TEXTURE_PAGE_HEIGHT)

// clang-format off
#define SHADE_CAUSTICS 0x300
#define SHADE_MAX      0x1FFF
#define SHADE_LOW      0x1400
#define SHADE_HIGH     0x800
#define SHADE_NEUTRAL  0x1000
#define SHADE_SUNSET   0x400
// clang-format on

#define WIBBLE_SIZE 32

#define LIGHT_MAP_SIZE 32
#define LIGHT_MAP_NEUTRAL 16

// TODO: get rid of this
#define PHD_ONE 0x10000
