#include <trx/game/fx/weather.h>

#include <trx/config.h>
#include <trx/core/math.h>
#include <trx/core/utils.h>
#include <trx/game/camera.h>
#include <trx/game/game/state.h>
#include <trx/game/interpolation.h>
#include <trx/game/lara.h>
#include <trx/game/objects.h>
#include <trx/game/output/shaders/mesh.h>
#include <trx/game/output/sources/poly_fx.h>
#include <trx/game/output/state.h>
#include <trx/game/output/vars.h>
#include <trx/game/random.h>
#include <trx/game/rooms.h>
#include <trx/game/viewport.h>

#include <string.h>

#define M_MAX_WEATHER 256
#define M_MAX_WEATHER_ALIVE 16

#define M_RAIN_MAX_DISTANCE 6000
#define M_RAIN_BASE_Y_OFF (-1024)
#define M_RAIN_BASE_YV 16
#define M_RAIN_YV_RND_MASK 7
#define M_RAIN_SPAWN_DIST_MASK 0xFFF
#define M_RAIN_SPAWN_ANGLE_MASK 0x1FFE
#define M_RAIN_Y_RND_MASK 0x7FF
#define M_RAIN_LIFE_BASE 88

#define M_SNOW_LIFE_BASE 96
#define M_SNOW_YV_MIN 8
#define M_SNOW_YV_RANGE 24

typedef struct {
    XYZ_32 pos;
    XYZ_32 prev_pos;
    int32_t prev_yv;
    int8_t xv;
    uint8_t yv;
    int8_t zv;
    uint8_t life;
} M_RAINDROP;

typedef struct {
    XYZ_32 pos;
    XYZ_32 prev_pos;
    bool stopped;
    int32_t prev_yv;
    int32_t prev_life;
    int8_t xv;
    uint8_t yv;
    int8_t zv;
    uint8_t life;
} M_SNOWFLAKE;

static M_RAINDROP m_Raindrops[M_MAX_WEATHER];
static M_SNOWFLAKE m_Snowflakes[M_MAX_WEATHER];
static WEATHER_TYPE m_WeatherType = WEATHER_NONE;

static void M_ClearWeather(void)
{
    memset(m_Raindrops, 0, sizeof(m_Raindrops));
    memset(m_Snowflakes, 0, sizeof(m_Snowflakes));
}

static int64_t M_GetViewDepth(const XYZ_32 pos)
{
    // clang-format off
    return
        g_ViewMatrix._20 * pos.x +
        g_ViewMatrix._21 * pos.y +
        g_ViewMatrix._22 * pos.z +
        g_ViewMatrix._23;
    // clang-format on
}

static bool M_SpawnParticle(XYZ_32 *const pos)
{
    const ITEM *const lara_item = Lara_GetItem();
    if (lara_item == nullptr) {
        return false;
    }

    XYZ_32 lara_pos = {};
    if (!Lara_GetMeshPos(LM_HIPS, &lara_pos)) {
        lara_pos = lara_item->pos;
    }

    const int32_t dist = Random_GetDraw() & M_RAIN_SPAWN_DIST_MASK;
    const int32_t angle = (Random_GetDraw() & M_RAIN_SPAWN_ANGLE_MASK) * 8;
    *pos = XYZ_32_OffsetYaw(lara_pos, angle, dist);
    pos->y += M_RAIN_BASE_Y_OFF - (Random_GetDraw() & M_RAIN_Y_RND_MASK);

    int16_t room_num = NO_ROOM;
    if (Room_GetOutsideStatus(*pos, &room_num) != 1) {
        pos->x = 0;
        return false;
    }

    return true;
}

static void M_SpawnRainDrop(M_RAINDROP *const drop)
{
    if (!M_SpawnParticle(&drop->pos)) {
        return;
    }

    drop->yv =
        (uint8_t)((Random_GetDraw() & M_RAIN_YV_RND_MASK) + M_RAIN_BASE_YV);
    drop->xv = (int8_t)((Random_GetDraw() & 7) - 4);
    drop->zv = (int8_t)((Random_GetDraw() & 7) - 4);
    drop->life = (uint8_t)(M_RAIN_LIFE_BASE - ((int32_t)drop->yv << 1));
    drop->prev_pos = drop->pos;
    drop->prev_yv = drop->yv;
}

static void M_UpdateRain(void)
{
    const XZ_32 wind = Sparks_GetSmokeWind();

    int32_t num_alive = 0;
    for (int32_t i = 0; i < M_MAX_WEATHER; i++) {
        M_RAINDROP *const drop = &m_Raindrops[i];

        if (drop->pos.x == 0 && num_alive < M_MAX_WEATHER_ALIVE) {
            num_alive++;
            M_SpawnRainDrop(drop);
        }

        if (drop->pos.x == 0) {
            continue;
        }

        drop->prev_pos = drop->pos;
        drop->prev_yv = drop->yv;

        int16_t room_num = NO_ROOM;
        const int32_t outside = Room_GetOutsideStatus(drop->pos, &room_num);
        if (outside == -2
            || (room_num != NO_ROOM && Room_Get(room_num)->flags.underwater)
            || drop->life > 240
            || ABS(g_Camera.pos.x - drop->pos.x) > M_RAIN_MAX_DISTANCE
            || ABS(g_Camera.pos.z - drop->pos.z) > M_RAIN_MAX_DISTANCE) {
            drop->pos.x = 0;
            continue;
        }

        drop->pos.x += (int32_t)drop->xv + 4 * wind.x;
        drop->pos.y += (int32_t)drop->yv * 8;
        drop->pos.z += (int32_t)drop->zv + 4 * wind.z;

        int32_t rnd = Random_GetDraw();
        if ((rnd & 3) != 3) {
            drop->xv += (int8_t)((rnd & 3) - 1);
            if (drop->xv < -4) {
                drop->xv = -4;
            } else if (drop->xv > 4) {
                drop->xv = 4;
            }
        }

        rnd = (rnd >> 2) & 3;
        if (rnd != 3) {
            drop->zv += (int8_t)(rnd - 1);
            if (drop->zv < -4) {
                drop->zv = -4;
            } else if (drop->zv > 4) {
                drop->zv = 4;
            }
        }

        drop->life -= 2;
        if (drop->life > 240) {
            drop->pos.x = 0;
        }
    }
}

static void M_DrawRain(void)
{
    const XZ_32 wind = Sparks_GetSmokeWind();
    const double ratio = Interpolation_GetWorldRate();
    const bool do_interp =
        Interpolation_IsActive() && ratio > 0.0 && ratio < 1.0;

    for (int32_t i = 0; i < M_MAX_WEATHER; i++) {
        const M_RAINDROP *const drop = &m_Raindrops[i];
        if (drop->pos.x == 0) {
            continue;
        }

        const int32_t yv = do_interp
            ? (int32_t)LERP(drop->prev_yv, drop->yv, ratio)
            : (int32_t)drop->yv;
        const XYZ_32 to = do_interp
            ? (XYZ_32) {
                  .x = (int32_t)LERP(drop->prev_pos.x, drop->pos.x, ratio),
                  .y = (int32_t)LERP(drop->prev_pos.y, drop->pos.y, ratio),
                  .z = (int32_t)LERP(drop->prev_pos.z, drop->pos.z, ratio),
              }
            : drop->pos;
        const XYZ_32 from = {
            to.x - (wind.x * 4),
            to.y - (yv * 8),
            to.z - (wind.z * 4),
        };

        const RGBA_8888 from_color = { 0, 0, 0x20, 0x00 };
        const RGBA_8888 to_color = { 0x30, 0x40, 0x60, 0x80 };
        OutputSource_PolyFX_StageLineSegment(
            from, from_color, to, to_color, 1.0f, DRAW_BLEND);
    }
}

static void M_SpawnSnowflake(M_SNOWFLAKE *const snow)
{
    if (!M_SpawnParticle(&snow->pos)) {
        return;
    }

    snow->stopped = false;
    snow->xv = (int8_t)((Random_GetDraw() & 7) - 4);
    snow->yv =
        (uint8_t)(((Random_GetDraw() % M_SNOW_YV_RANGE) + M_SNOW_YV_MIN) * 8);
    snow->zv = (int8_t)((Random_GetDraw() & 7) - 4);
    snow->life = (uint8_t)(M_SNOW_LIFE_BASE - ((int32_t)snow->yv << 1));
    snow->prev_pos = snow->pos;
    snow->prev_yv = snow->yv;
    snow->prev_life = snow->life;
}

static void M_UpdateSnow(void)
{
    int32_t num_alive = 0;
    for (int32_t i = 0; i < M_MAX_WEATHER; i++) {
        M_SNOWFLAKE *const snow = &m_Snowflakes[i];

        if (snow->pos.x == 0 && num_alive < M_MAX_WEATHER_ALIVE) {
            num_alive++;
            M_SpawnSnowflake(snow);
        }

        if (snow->pos.x == 0) {
            continue;
        }

        snow->prev_pos = snow->pos;
        snow->prev_yv = snow->yv;
        snow->prev_life = snow->life;

        const XYZ_32 old_pos = snow->pos;

        int16_t room_num = NO_ROOM;
        int32_t outside = 1;
        if (!snow->stopped) {
            snow->pos.x += snow->xv;
            snow->pos.y += (snow->yv & 0xF8) >> 2;
            snow->pos.z += snow->zv;

            outside = Room_GetOutsideStatus(snow->pos, &room_num);
            if (outside == -3) {
                snow->pos.x = 0;
                continue;
            }

            if (outside == -2
                || (room_num != NO_ROOM
                    && Room_Get(room_num)->flags.underwater)) {
                snow->stopped = true;
                snow->pos = old_pos;

                if (snow->life > 16) {
                    snow->life = 16;
                }
                if (snow->yv > 16) {
                    snow->yv -= 16;
                }
            }
        }

        if (snow->life == 0) {
            snow->pos.x = 0;
            continue;
        }

        if ((ABS(g_Camera.pos.x - snow->pos.x) > M_RAIN_MAX_DISTANCE
             || ABS(g_Camera.pos.z - snow->pos.z) > M_RAIN_MAX_DISTANCE)
            && snow->life > 16) {
            snow->life = 16;
        }

        const XZ_32 wind = Sparks_GetSmokeWind();

        if (snow->xv < (wind.x * 2)) {
            snow->xv++;
        } else if (snow->xv > (wind.x * 2)) {
            snow->xv--;
        }

        if (snow->zv < (wind.z * 2)) {
            snow->zv++;
        } else if (snow->zv > (wind.z * 2)) {
            snow->zv--;
        }

        snow->life -= 2;
        if ((snow->yv & 7) != 7) {
            snow->yv++;
        }
    }
}

static void M_DrawSnow(void)
{
    const OBJECT *const obj = Object_Get(O_SNOWFLAKE);
    if (!obj->loaded) {
        return;
    }

    const int32_t sprite_idx = obj->mesh_idx;
    const double ratio = Interpolation_GetWorldRate();
    const bool do_interp =
        Interpolation_IsActive() && ratio > 0.0 && ratio < 1.0;
    for (int32_t i = 0; i < M_MAX_WEATHER; i++) {
        M_SNOWFLAKE *const snow = &m_Snowflakes[i];
        if (snow->pos.x == 0) {
            continue;
        }

        const XYZ_32 center = do_interp
            ? (XYZ_32) {
                  .x = (int32_t)LERP(snow->prev_pos.x, snow->pos.x, ratio),
                  .y = (int32_t)LERP(snow->prev_pos.y, snow->pos.y, ratio),
                  .z = (int32_t)LERP(snow->prev_pos.z, snow->pos.z, ratio),
              }
            : snow->pos;
        const int64_t zv = M_GetViewDepth(center);
        const int64_t near_z = Output_GetNearZ();
        const int64_t far_z = Output_GetFarZ();
        const int32_t vpos_z = (int32_t)(zv >> W2V_SHIFT);

        if (vpos_z < 128 && snow->life > 16) {
            snow->life = 16;
        }

        if (zv <= near_z || zv >= far_z) {
            continue;
        }

        if (vpos_z < 128 || g_PhdPersp <= 0) {
            continue;
        }

        const int32_t yv = do_interp
            ? (int32_t)LERP(snow->prev_yv, snow->yv, ratio)
            : (int32_t)snow->yv;
        const int32_t life = do_interp
            ? (int32_t)LERP(snow->prev_life, snow->life, ratio)
            : (int32_t)snow->life;

        const int32_t game_w = Viewport_GetWidth(VIEWPORT_GAME);
        const int32_t game_h = Viewport_GetHeight(VIEWPORT_GAME);
        const int32_t ui_w = Viewport_GetWidth(VIEWPORT_UI);
        const int32_t ui_h = Viewport_GetHeight(VIEWPORT_UI);

        const XYZ_32 world_pos[4] = { center, center, center, center };
        const float s = 8.0f;
        const float disp[4][2] = {
            { -s, -s },
            { s, -s },
            { s, s },
            { -s, s },
        };

        uint32_t c;
        if ((yv & 7) < 7) {
            c = (uint32_t)(yv & 7);
        } else if (life > 18) {
            c = 15;
        } else {
            c = (uint32_t)life;
        }
        c <<= 3;
        CLAMPG(c, 255);

        {
            const int32_t fog_start = Output_GetFogStart();
            const int32_t fog_end = Output_GetFogEnd();
            float fade = 1.0f;
            if (fog_end > fog_start && vpos_z > fog_start) {
                float t = (vpos_z - fog_start) / (float)(fog_end - fog_start);
                CLAMP(t, 0.0f, 1.0f);
                fade = 1.0f - t;
            }
            c = (uint32_t)(c * fade);
        }

        const RGBA_8888 color = { c, c, c, 255 };
        const RGBA_8888 colors[4] = { color, color, color, color };

        OutputSource_PolyFX_StageQuadExt(
            sprite_idx, world_pos, disp, colors,
            VERT_NO_LIGHTING | VERT_NO_WIBBLE | VERT_BILLBOARD
                | VERT_ABS_SPRITE,
            DRAW_BLEND_ADD);
    }
}

void FX_Weather_Reset(void)
{
    M_ClearWeather();
}

WEATHER_TYPE FX_Weather_GetWeather(void)
{
    return m_WeatherType;
}

void FX_Weather_SetWeather(const WEATHER_TYPE weather_type)
{
    m_WeatherType = weather_type;
}

void FX_Weather_Control(void)
{
    if (!g_Config.visuals.enable_weather) {
        return;
    }
    const WEATHER_TYPE weather_type = m_WeatherType;
    if (weather_type == WEATHER_RAIN) {
        M_UpdateRain();
    } else if (weather_type == WEATHER_SNOW) {
        M_UpdateSnow();
    }
}

void FX_Weather_Draw(void)
{
    if (!g_Config.visuals.enable_weather) {
        return;
    }
    switch (m_WeatherType) {
    case WEATHER_RAIN:
        M_DrawRain();
        break;
    case WEATHER_SNOW:
        M_DrawSnow();
        break;
    default:
        break;
    }
}
