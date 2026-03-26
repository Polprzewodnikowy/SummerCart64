#include <trx/game/sparks/manager.h>

#include <trx/config.h>
#include <trx/debug.h>
#include <trx/game/effects.h>
#include <trx/game/lara.h>
#include <trx/game/output/lights.h>
#include <trx/game/output/sources/poly_fx.h>
#include <trx/game/random.h>
#include <trx/version.h>

#define M_MAX_SPARKS 400
#define M_MAX_SPARK_DYNAMICS 32

typedef struct {
    bool on;
    uint8_t falloff;
    RGB_888 color;
    uint8_t flags;
} M_SPARK_DYNAMIC;

static SPARK m_Sparks[M_MAX_SPARKS];
static M_SPARK_DYNAMIC m_Dynamics[M_MAX_SPARK_DYNAMICS];
static int32_t m_NextSpark = 0;
static XZ_32 m_SmokeWind = {};
static int32_t m_HairWindZ = 0;
static int32_t m_TR3Wind = 0;
static int32_t m_TR3WindAngle = DEG_180;
static int32_t m_TR3DWindAngle = DEG_180;

static const BITE m_NodeOffsets[16] = {
    { .pos = { 0, 340, 64 }, .mesh_num = 7 },
    { .pos = { 0, 0, -96 }, .mesh_num = 10 },
    { .pos = { 16, 48, 320 }, .mesh_num = 13 },
    { .pos = { 0, -256, 0 }, .mesh_num = 5 },
    { .pos = { 0, 64, 0 }, .mesh_num = 10 },
    { .pos = { 0, 64, 0 }, .mesh_num = 13 },
    { .pos = { -32, -16, -192 }, .mesh_num = 13 },
    { .pos = { -64, 410, 0 }, .mesh_num = 20 },
    { .pos = { 64, 410, 0 }, .mesh_num = 23 },
    { .pos = { -160, -8, 16 }, .mesh_num = 5 },
    { .pos = { -160, -8, 16 }, .mesh_num = 9 },
    { .pos = { -160, -8, 16 }, .mesh_num = 13 },
    { .pos = { 0, 0, 0 }, .mesh_num = 0 },
    { .pos = { 0, 0, 0 }, .mesh_num = 0 },
    { .pos = { 0, 0, 0 }, .mesh_num = 0 },
    { .pos = { 0, 0, 0 }, .mesh_num = 0 },
};

XYZ_32 Sparks_GetWorldPos(const SPARK *const spark)
{
    if (spark == nullptr) {
        return (XYZ_32) { 0, 0, 0 };
    }

    if ((spark->flags & SPARK_F_FX) != 0U) {
        const EFFECT *const effect = Effect_Get(spark->effect_num);
        if ((spark->flags & SPARK_F_ATTACHED_POS) != 0U) {
            return effect->pos;
        }
        return (XYZ_32) {
            .x = effect->pos.x + spark->pos.x,
            .y = effect->pos.y + spark->pos.y,
            .z = effect->pos.z + spark->pos.z,
        };
    }

    if ((spark->flags & SPARK_F_ITEM) != 0U) {
        const ITEM *const item = Item_Get(spark->item_num);
        if ((spark->flags & SPARK_F_ATTACHED_POS) != 0U) {
            return item->pos;
        }

        if ((spark->flags & SPARK_F_ATTACHED_NODE) != 0U) {
            XYZ_32 joint_pos = m_NodeOffsets[spark->node_num & 0xF].pos;
            Collide_GetJointAbsPosition(
                item, &joint_pos,
                m_NodeOffsets[spark->node_num & 0xF].mesh_num);
            return (XYZ_32) {
                .x = joint_pos.x + spark->pos.x,
                .y = joint_pos.y + spark->pos.y,
                .z = joint_pos.z + spark->pos.z,
            };
        }

        return (XYZ_32) {
            .x = item->pos.x + spark->pos.x,
            .y = item->pos.y + spark->pos.y,
            .z = item->pos.z + spark->pos.z,
        };
    }

    return spark->pos;
}

static int32_t M_GetFreeSpark(void)
{
    int32_t idx = m_NextSpark;
    for (int32_t i = 0; i < M_MAX_SPARKS; i++) {
        if (!m_Sparks[idx].on) {
            m_NextSpark = (idx + 1) & 0xBF;
            return idx;
        }
        idx = idx == (M_MAX_SPARKS - 1) ? 0 : idx + 1;
    }

    int32_t free = 0;
    int32_t min_life = INT32_MAX;
    for (int32_t i = 0; i < M_MAX_SPARKS; i++) {
        const SPARK *const spark = &m_Sparks[i];
        if ((int32_t)spark->life < min_life && spark->dynamic == -1
            && ((spark->flags & SPARK_F_BLOOD) == 0U || (i & 1) != 0)) {
            free = i;
            min_life = (int32_t)spark->life;
        }
    }

    m_NextSpark = (free + 1) & 0xBF;
    return free;
}

SPARK *Sparks_GetFreeSpark(void)
{
    const int32_t idx = M_GetFreeSpark();
    return &m_Sparks[idx];
}

SPARK *Sparks_GetSpark(const int32_t idx)
{
    ASSERT(idx >= 0 && idx < M_MAX_SPARKS);
    return &m_Sparks[idx];
}

void Sparks_Sync(SPARK *const spark)
{
    if (spark == nullptr) {
        return;
    }

    const XYZ_32 world_pos = Sparks_GetWorldPos(spark);
    spark->prev_pos = spark->pos;
    spark->prev_world_pos = world_pos;
    spark->prev_color = spark->color;
    spark->prev_size = spark->size;
    spark->prev_rot_angle = spark->rot_angle;
}

void Sparks_FinishSetup(SPARK *const spark)
{
    if (spark == nullptr) {
        return;
    }

    spark->color = spark->src_color;
    Sparks_Sync(spark);
}

int8_t Sparks_AllocDynamic(const uint8_t flags)
{
    for (int32_t i = 0; i < M_MAX_SPARK_DYNAMICS; i++) {
        if (!m_Dynamics[i].on) {
            m_Dynamics[i].on = true;
            m_Dynamics[i].falloff = 4;
            m_Dynamics[i].flags = flags;
            m_Dynamics[i].color = COLOR_RGB_888_BLACK;
            return (int8_t)i;
        }
    }
    return -1;
}

void Sparks_FreeDynamic(const int8_t idx)
{
    if (idx < 0 || idx >= M_MAX_SPARK_DYNAMICS) {
        return;
    }
    m_Dynamics[idx].on = false;
}

void Sparks_Reset(void)
{
    for (int32_t i = 0; i < M_MAX_SPARKS; i++) {
        m_Sparks[i].on = false;
        m_Sparks[i].dynamic = -1;
    }
    for (int32_t i = 0; i < M_MAX_SPARK_DYNAMICS; i++) {
        m_Dynamics[i].on = false;
    }
    m_NextSpark = 0;
    m_SmokeWind = (XZ_32) {};
    m_HairWindZ = 0;
    m_TR3Wind = 0;
    m_TR3WindAngle = DEG_180;
    m_TR3DWindAngle = DEG_180;
}

XZ_32 Sparks_GetSmokeWind(void)
{
    return m_SmokeWind;
}

void Sparks_SetSmokeWind(const XZ_32 wind)
{
    m_SmokeWind = wind;
}

int32_t Sparks_GetHairWindZ(void)
{
    return m_HairWindZ;
}

static void M_UpdateWind(void)
{
    if (!g_Config.visuals.enable_breeze) {
        m_SmokeWind = (XZ_32) {};
        m_HairWindZ = 0;
        return;
    }

    if (g_TRVersion != 3) {
        const ITEM *const lara_item = Lara_GetItem();
        if (lara_item == nullptr) {
            m_HairWindZ = 0;
            return;
        }

        const ROOM *const room = Room_Get(lara_item->room_num);
        if (room == nullptr || !room->flags.wind) {
            m_HairWindZ = 0;
            return;
        }

        const int32_t random = Random_GetDraw() & 7;
        if (random != 0) {
            m_HairWindZ += random - 4;
            if (m_HairWindZ < 0) {
                m_HairWindZ = 0;
            } else if (m_HairWindZ >= 8) {
                m_HairWindZ--;
            }
        }
        m_SmokeWind = (XZ_32) {};
        return;
    }

    // TR3 wind logic: a small random wind magnitude with a slowly-changing
    // direction, biased to the [90°, 270°] range.
    m_TR3Wind += (Random_GetControl() & 7) - 3;
    if (m_TR3Wind <= -2) {
        m_TR3Wind++;
    } else if (m_TR3Wind >= 9) {
        m_TR3Wind--;
    }

    // Original TR3 uses a 0..4095 angle space; keep the calculations faithful.
    m_TR3DWindAngle =
        (m_TR3DWindAngle + (((Random_GetControl() & 0x3F) - 32) * 2)) & 0x1FFE;

    if (m_TR3DWindAngle < 1024) { // DEG_90
        m_TR3DWindAngle += (1024 - m_TR3DWindAngle) << 1;
    } else if (m_TR3DWindAngle > 3072) { // DEG_270
        m_TR3DWindAngle -= (m_TR3DWindAngle - 3072) << 1;
    }
    m_TR3DWindAngle &= 0x1FFE;
    m_TR3WindAngle =
        (m_TR3WindAngle + ((m_TR3DWindAngle - m_TR3WindAngle) >> 3)) & 0x1FFE;

    // Promote to DEG_360 for Math_Sin/Cos just at the end.
    m_SmokeWind = (XZ_32) {
        .x = (m_TR3Wind * Math_Sin(m_TR3WindAngle << 3)) >> W2V_SHIFT,
        .z = (m_TR3Wind * Math_Cos(m_TR3WindAngle << 3)) >> W2V_SHIFT,
    };

    m_HairWindZ = 0;
}

void Sparks_Control(void)
{
    M_UpdateWind();

    for (int32_t i = 0; i < M_MAX_SPARKS; i++) {
        SPARK *const spark = &m_Sparks[i];
        if (!spark->on) {
            continue;
        }

        if ((spark->flags & SPARK_F_ATTACHED_POS) == 0U || spark->life > 16) {
            if (spark->life > 0) {
                spark->life--;
            }
        }

        if (spark->life == 0) {
            if (spark->dynamic != -1) {
                m_Dynamics[(uint8_t)spark->dynamic].on = false;
                spark->dynamic = -1;
            }

            spark->on = false;
            continue;
        }

        const int32_t lived = (int32_t)spark->s_life - (int32_t)spark->life;

        spark->prev_pos = spark->pos;
        spark->prev_world_pos = Sparks_GetWorldPos(spark);
        spark->prev_color = spark->color;
        spark->prev_size = spark->size;
        spark->prev_rot_angle = spark->rot_angle;

        // Color fade: src -> dst, then fade-to-black.
        if (lived < (int32_t)spark->col_fade_speed
            && spark->col_fade_speed != 0U) {
            const float fade = lived / (float)spark->col_fade_speed;
            spark->color.r = LERP(
                (int32_t)spark->src_color.r, (int32_t)spark->dst_color.r, fade);
            spark->color.g = LERP(
                (int32_t)spark->src_color.g, (int32_t)spark->dst_color.g, fade);
            spark->color.b = LERP(
                (int32_t)spark->src_color.b, (int32_t)spark->dst_color.b, fade);
        } else if (
            spark->life < spark->fade_to_black && spark->fade_to_black != 0U) {
            const float fade = spark->life / (float)spark->fade_to_black;
            spark->color.r = spark->dst_color.r * fade;
            spark->color.g = spark->dst_color.g * fade;
            spark->color.b = spark->dst_color.b * fade;
        } else {
            spark->color = spark->dst_color;
        }

        if (spark->life == spark->fade_to_black
            && (spark->flags & SPARK_F_UNDERWATER) != 0U) {
            spark->dst_size.width >>= 2;
            spark->dst_size.height >>= 2;
        }

        if ((spark->flags & SPARK_F_ROTATE) != 0U) {
            spark->rot_angle = (spark->rot_angle + spark->rot_add) & 0xFFF;
        }

        if ((spark->flags & SPARK_F_ALT_SPRITE) != 0U) {
            const OBJECT *const explosion = Object_Get(O_EXPLOSION_1);
            if (explosion->loaded) {
                const int32_t base = explosion->mesh_idx;
                if (spark->color.r < 16 && spark->color.g < 16
                    && spark->color.b < 16) {
                    spark->sprite_idx = base + 3;
                } else if (
                    spark->color.r < 64 && spark->color.g < 64
                    && spark->color.b < 64) {
                    spark->sprite_idx = base + 2;
                } else if (
                    spark->color.r < 96 && spark->color.g < 96
                    && spark->color.b < 96) {
                    spark->sprite_idx = base + 1;
                } else {
                    spark->sprite_idx = base;
                }
            }
        }

        if (lived == (int32_t)(spark->extras >> 3)
            && (spark->extras & 7U) != 0U) {
            int32_t uw = 0;
            if ((spark->flags & SPARK_F_UNDERWATER) != 0U) {
                uw = 1;
            } else if ((spark->flags & SPARK_F_GREEN) != 0U) {
                uw = 2;
            }

            const XYZ_32 spark_pos = Sparks_GetWorldPos(spark);

            for (int32_t j = 0; j < (int32_t)(spark->extras & 7U); j++) {
                Sparks_TriggerExplosionSparks(
                    spark_pos, (int32_t)(spark->extras & 7U) - 1,
                    spark->dynamic, uw, spark->room_num);
                spark->dynamic = -1;
            }

            if ((spark->flags & SPARK_F_UNDERWATER) != 0U) {
                Sparks_TriggerExplosionBubble(spark_pos, spark->room_num);
            }

            spark->extras = 0;
        }

        // Physics
        spark->vel.y += spark->gravity;
        if (spark->max_y_vel != 0) {
            const int32_t limit = (int32_t)spark->max_y_vel * (1 << 5);
            if ((spark->vel.y < 0 && spark->vel.y < limit)
                || (spark->vel.y > 0 && spark->vel.y > limit)) {
                spark->vel.y = limit;
            }
        }

        if ((spark->friction & 0x0FU) != 0U) {
            spark->vel.x -= spark->vel.x >> (spark->friction & 0x0FU);
            spark->vel.z -= spark->vel.z >> (spark->friction & 0x0FU);
        }

        if ((spark->friction & 0xF0U) != 0U) {
            spark->vel.y -= spark->vel.y >> (spark->friction >> 4);
        }

        spark->pos.x += spark->vel.x >> 5;
        spark->pos.y += spark->vel.y >> 5;
        spark->pos.z += spark->vel.z >> 5;

        if ((spark->flags & SPARK_F_OUTSIDE) != 0U) {
            spark->pos.x += m_SmokeWind.x >> 1;
            spark->pos.z += m_SmokeWind.z >> 1;
        }

        // Size lerp across lifetime.
        if (spark->s_life != 0U) {
            const float fade = lived / (float)spark->s_life;
            spark->size.width = LERP(
                (int32_t)spark->src_size.width, (int32_t)spark->dst_size.width,
                fade);
            spark->size.height = LERP(
                (int32_t)spark->src_size.height,
                (int32_t)spark->dst_size.height, fade);
        } else {
            spark->size = spark->src_size;
        }

        // If attached to a node, detach after a short random delay for some
        // node types.
        if ((spark->flags & (SPARK_F_ITEM | SPARK_F_ATTACHED_NODE))
                == (SPARK_F_ITEM | SPARK_F_ATTACHED_NODE)
            && (spark->node_num == 2 || spark->node_num == 3)) {
            const int32_t b = spark->node_num == 3 ? (Random_GetDraw() & 3) + 12
                                                   : (Random_GetDraw() & 3) + 8;
            if (lived > b) {
                spark->pos = Sparks_GetWorldPos(spark);
                spark->flags &= ~(SPARK_F_ATTACHED_NODE | SPARK_F_ITEM);
                Sparks_Sync(spark);
            }
        }
    }

    // Dynamic light pass.
    for (int32_t i = 0; i < M_MAX_SPARKS; i++) {
        const SPARK *const spark = &m_Sparks[i];
        if (!spark->on || spark->dynamic == -1) {
            continue;
        }

        M_SPARK_DYNAMIC *const dl = &m_Dynamics[(uint8_t)spark->dynamic];
        if (!dl->on) {
            continue;
        }

        const XYZ_32 world_pos = Sparks_GetWorldPos(spark);
        const int32_t rnd = Random_GetControl();
        XYZ_32 pos = {
            .x = world_pos.x + ((rnd & 0xF) << 4),
            .y = world_pos.y + (rnd & 0xF0),
            .z = world_pos.z + ((rnd >> 4) & 0xF0),
        };

        int32_t falloff = (int32_t)spark->s_life - (int32_t)spark->life - 1;
        int32_t r = 0;
        int32_t g = 0;
        int32_t b = 0;

        if (falloff < 2) {
            if (dl->falloff < 28) {
                dl->falloff = (uint8_t)MIN((int32_t)dl->falloff + 6, 255);
            }
            r = 255 - (rnd & 0x1F) - (falloff << 3);
            g = 255 - (rnd & 0x1F) - (falloff << 4);
            b = 255 - (rnd & 0x1F) - (falloff << 6);
        } else if (falloff < 4) {
            if (dl->falloff < 28) {
                dl->falloff = (uint8_t)MIN((int32_t)dl->falloff + 6, 255);
            }
            r = 255 - (rnd & 0x1F) - (falloff << 3);
            g = 128 - (falloff << 3);
            b = (4 - falloff) << 2;
            if (b < 0) {
                b = 0;
            } else {
                b <<= 3;
            }
        } else {
            if (dl->falloff != 0U) {
                dl->falloff--;
            }
            r = (rnd & 0x1F) + 224;
            g = ((rnd >> 4) & 0x1F) + 128;
            b = (rnd >> 8) & 0x3F;
        }

        falloff = (int32_t)dl->falloff;
        if (falloff > 31) {
            falloff = 31;
        }

        if ((spark->flags & SPARK_F_GREEN) != 0U) {
            Output_AddDynamicLightRGB(pos, falloff, (RGB_888) { b, r, g });
        } else {
            Output_AddDynamicLightRGB(pos, falloff, (RGB_888) { r, g, b });
        }
    }
}

void Sparks_Draw(void)
{
    for (int32_t i = 0; i < M_MAX_SPARKS; i++) {
        SPARK *const spark = &m_Sparks[i];
        if (!spark->on) {
            continue;
        }

        OutputSource_PolyFX_StageSpark(spark);
    }
}

void Sparks_DetachEffect(const int16_t effect_num)
{
    for (int32_t i = 0; i < M_MAX_SPARKS; i++) {
        SPARK *const spark = &m_Sparks[i];
        if (!spark->on) {
            continue;
        }

        if ((spark->flags & SPARK_F_FX) != 0U
            && spark->effect_num == effect_num) {
            if ((spark->flags & SPARK_F_ATTACHED_POS) != 0U) {
                spark->on = false;
                continue;
            }

            const EFFECT *const effect = Effect_Get(effect_num);
            spark->pos.x += effect->pos.x;
            spark->pos.y += effect->pos.y;
            spark->pos.z += effect->pos.z;
            spark->flags &= ~SPARK_F_FX;
        }
    }
}

void Sparks_DetachItem(const int16_t item_num)
{
    for (int32_t i = 0; i < M_MAX_SPARKS; i++) {
        SPARK *const spark = &m_Sparks[i];
        if (!spark->on) {
            continue;
        }

        if ((spark->flags & SPARK_F_ITEM) != 0U
            && spark->item_num == item_num) {
            if ((spark->flags & SPARK_F_ATTACHED_POS) != 0U) {
                spark->on = false;
                continue;
            }

            const ITEM *const item = Item_Get(item_num);
            spark->pos.x += item->pos.x;
            spark->pos.y += item->pos.y;
            spark->pos.z += item->pos.z;
            spark->flags &= ~SPARK_F_ITEM;
            spark->flags &= ~SPARK_F_ATTACHED_NODE;
        }
    }
}
