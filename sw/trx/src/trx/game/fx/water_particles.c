#include <trx/game/fx/water_particles.h>

#include <trx/config.h>
#include <trx/core/math.h>
#include <trx/core/utils.h>
#include <trx/game/game_flow/common.h>
#include <trx/game/interpolation.h>
#include <trx/game/lara.h>
#include <trx/game/objects.h>
#include <trx/game/output/sources/poly_fx.h>
#include <trx/game/output/state.h>
#include <trx/game/output/textures.h>
#include <trx/game/output/vars.h>
#include <trx/game/random.h>
#include <trx/game/rooms.h>
#include <trx/game/viewport.h>

#include <string.h>

#define M_MAX_WATER_PARTICLES 256
#define M_MAX_WATER_PARTICLES_ALIVE 16
#define M_SPAWN_DIST_MASK 0xFFF
#define M_SPAWN_ANGLE_MASK 0x1FFE
#define M_SPAWN_Y_MASK 0x7FF
#define M_BASE_Y_OFF (-1024)
#define M_MIN_SIZE 4.0f
#define M_MAX_SIZE 16.0f

typedef struct {
    XYZ_32 pos;
    XYZ_32 prev_pos;
    uint8_t life;
    uint8_t yv;
    int8_t xv;
    int8_t zv;
} M_WATER_PARTICLE;

static M_WATER_PARTICLE m_WaterParticles[M_MAX_WATER_PARTICLES];

static bool M_IsEnabled(void)
{
    if (!g_Config.visuals.enable_weather) {
        return false;
    }

    const GF_LEVEL *const level = GF_GetCurrentLevel();
    return level != nullptr && level->water_particles;
}

static void M_Clear(void)
{
    memset(m_WaterParticles, 0, sizeof(m_WaterParticles));
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

static float M_GetFixedScale(const float unit)
{
    const float base_w = 640.0f;
    const float base_h = 480.0f;
    const float game_w = (float)Viewport_GetWidth(VIEWPORT_GAME);
    const float game_h = (float)Viewport_GetHeight(VIEWPORT_GAME);
    const float x = game_w > base_w ? (game_w * unit) / base_w : unit;
    const float y = game_h > base_h ? (game_h * unit) / base_h : unit;
    return MIN(x, y);
}

static void M_Spawn(M_WATER_PARTICLE *const particle)
{
    const ITEM *const lara_item = Lara_GetItem();
    if (lara_item == nullptr) {
        return;
    }

    const int32_t dist = Random_GetDraw() & M_SPAWN_DIST_MASK;
    const int32_t angle = (Random_GetDraw() & M_SPAWN_ANGLE_MASK) * 8;
    particle->pos = XYZ_32_OffsetYaw(lara_item->pos, angle, dist);
    particle->pos.y += (Random_GetDraw() & M_SPAWN_Y_MASK) + M_BASE_Y_OFF;

    int16_t room_num = NO_ROOM;
    Room_GetOutsideStatus(particle->pos, &room_num);
    if (room_num == NO_ROOM || !Room_Get(room_num)->flags.underwater) {
        particle->pos.x = 0;
        return;
    }

    particle->life = (uint8_t)((Random_GetDraw() & 7) + 16);
    particle->xv = (int8_t)(Random_GetDraw() & 3);
    if (particle->xv == 2) {
        particle->xv = -1;
    }

    particle->yv = (uint8_t)(((Random_GetDraw() & 7) + 8) << 3);
    particle->zv = (int8_t)(Random_GetDraw() & 3);
    if (particle->zv == 2) {
        particle->zv = -1;
    }

    particle->prev_pos = particle->pos;
}

void FX_WaterParticles_Reset(void)
{
    M_Clear();
}

void FX_WaterParticles_Control(void)
{
    if (!M_IsEnabled()) {
        M_Clear();
        return;
    }

    int32_t num_alive = 0;
    for (int32_t i = 0; i < M_MAX_WATER_PARTICLES; i++) {
        M_WATER_PARTICLE *const particle = &m_WaterParticles[i];

        if (particle->pos.x == 0 && num_alive < M_MAX_WATER_PARTICLES_ALIVE) {
            num_alive++;
            M_Spawn(particle);
        }

        if (particle->pos.x == 0) {
            continue;
        }

        particle->prev_pos = particle->pos;
        particle->pos.x += particle->xv;
        particle->pos.y += (particle->yv & 0xF8) >> 6;
        particle->pos.z += particle->zv;

        if (particle->life == 0) {
            particle->pos.x = 0;
            continue;
        }

        particle->life--;
        if ((particle->yv & 7) != 7) {
            particle->yv++;
        }
    }
}

void FX_WaterParticles_Draw(void)
{
    if (!M_IsEnabled()) {
        return;
    }

    const OBJECT *const obj = Object_Get(O_EXPLOSION_1);
    if (obj == nullptr || !obj->loaded) {
        return;
    }

    const int32_t sprite_idx = obj->mesh_idx + 17;
    if (sprite_idx < 0 || sprite_idx >= Output_GetSpriteTextureCount()) {
        return;
    }

    const int32_t atlas_idx = Output_Textures_GetSpriteUVWIndex(sprite_idx, 0);
    const OUTPUT_TEXTURE_SIZE atlas_size =
        Output_Textures_GetAtlasSize(atlas_idx / 4);
    const OUTPUT_TEXTURE_SIZE tri_size[3] = { atlas_size, atlas_size,
                                              atlas_size };
    const OUTPUT_UVW tri_uvw[3] = {
        Output_Textures_GetUVW(
            Output_Textures_GetSpriteUVWIndex(sprite_idx, 1)),
        Output_Textures_GetUVW(
            Output_Textures_GetSpriteUVWIndex(sprite_idx, 2)),
        Output_Textures_GetUVW(
            Output_Textures_GetSpriteUVWIndex(sprite_idx, 3)),
    };

    const double ratio = Interpolation_GetWorldRate();
    const bool do_interp =
        Interpolation_IsActive() && ratio > 0.0 && ratio < 1.0;

    for (int32_t i = 0; i < M_MAX_WATER_PARTICLES; i++) {
        M_WATER_PARTICLE *const particle = &m_WaterParticles[i];
        if (particle->pos.x == 0) {
            continue;
        }

        const XYZ_32 center = do_interp
            ? (XYZ_32) {
                  .x = (int32_t)LERP(
                      particle->prev_pos.x, particle->pos.x, ratio),
                  .y = (int32_t)LERP(
                      particle->prev_pos.y, particle->pos.y, ratio),
                  .z = (int32_t)LERP(
                      particle->prev_pos.z, particle->pos.z, ratio),
              }
            : particle->pos;

        const int64_t zv = M_GetViewDepth(center);
        const int64_t near_z = Output_GetNearZ();
        const int64_t far_z = Output_GetFarZ();
        const int32_t vpos_z = (int32_t)(zv >> W2V_SHIFT);

        if (vpos_z < 128) {
            if (particle->life > 16) {
                particle->life = 16;
            }
            continue;
        }

        if (zv <= near_z || zv >= far_z || g_PhdPersp <= 0) {
            continue;
        }

        float size = (float)((g_PhdPersp * (particle->yv >> 3)) / vpos_z);
        CLAMP(size, M_MIN_SIZE, M_MAX_SIZE);
        size /= 3.0f;
        size = M_GetFixedScale(size) / 2.0f;

        uint32_t c;
        if ((particle->yv & 7) < 7) {
            c = (uint32_t)(particle->yv & 7);
        } else if (particle->life > 18) {
            c = 15;
        } else {
            c = particle->life;
        }
        c <<= 2;
        CLAMPG(c, 255);

        const RGBA_8888 color = { c, c, c, 255 };
        const RGBA_8888 colors[3] = { color, color, color };
        const XYZ_32 world_pos[3] = { center, center, center };
        const float disp[3][2] = {
            { size, -2.0f * size },
            { size, size },
            { -2.0f * size, size },
        };

        OutputSource_PolyFX_StageTriExtUV(
            world_pos, tri_uvw, tri_size, disp, colors,
            VERT_NO_LIGHTING | VERT_NO_WIBBLE | VERT_BILLBOARD
                | VERT_ABS_SPRITE,
            DRAW_BLEND_ADD);
    }
}
