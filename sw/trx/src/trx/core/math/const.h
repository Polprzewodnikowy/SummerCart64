#pragma once

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

// clang-format off
#define DEG_360 0x10000
#define DEG_315 (DEG_180 + DEG_135) // = 0xE000
#define DEG_270 (DEG_180 + DEG_90)  // = 0xC000
#define DEG_225 (DEG_180 + DEG_45)  // = 0xA000
#define DEG_180 ((DEG_360) / 2)     // = 0x8000
#define DEG_135 ((DEG_45)  * 3)     // = 0x6000
#define DEG_90  ((DEG_360) / 4)     // = 0x4000
#define DEG_45  ((DEG_360) / 8)     // = 0x2000
#define DEG_1   ((DEG_360) / 360)   // = 182
// clang-format on

#define W2V_SHIFT 14
