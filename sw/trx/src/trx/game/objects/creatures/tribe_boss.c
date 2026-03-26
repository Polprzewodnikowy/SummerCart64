#include <trx/game/objects/creatures/tribe_boss.h>

#include <trx/config.h>
#include <trx/core/json/util/read_io.h>
#include <trx/core/json/util/write_io.h>
#include <trx/core/utils.h>
#include <trx/game/creature.h>
#include <trx/game/fx/explosion_ring.h>
#include <trx/game/items.h>
#include <trx/game/items/carrier.h>
#include <trx/game/lara.h>
#include <trx/game/lara/electric.h>
#include <trx/game/los.h>
#include <trx/game/objects.h>
#include <trx/game/output.h>
#include <trx/game/output/sources/poly_fx.h>
#include <trx/game/pathing.h>
#include <trx/game/random.h>
#include <trx/game/rooms.h>
#include <trx/game/sound.h>
#include <trx/game/sparks.h>
#include <trx/game/spawn.h>

#define M_MAX_HEAD_ATTACKS 4

typedef enum {
    M_STATE_WAIT = 0,
    M_STATE_ATTACK_HEAD = 1,
    M_STATE_ATTACK_HAND = 2,
    M_STATE_DEATH = 3,
} M_STATE;

typedef enum {
    M_ANIM_DEATH = 3,
} M_ANIM;

typedef enum {
    M_ATTACK_HEAD,
    M_ATTACK_HAND_1,
    M_ATTACK_HAND_2,
} M_ATTACK_TYPE;

typedef struct {
    XYZ_16 pos;
    RGB_888 sub;
    RGB_888 color;
} M_SHIELD_POINT;

typedef struct {
    bool lizard_active;
} M_SHARED_PRIV;

typedef struct {
    XYZ_32 pos;
    uint16_t y_rot;
} M_LIZARD_SUMMON_COORDS;

typedef struct {
    uint8_t dead;
    int16_t attack_count;
    int16_t death_count;
    uint8_t attack_flag;
    M_ATTACK_TYPE attack_type;
    uint8_t attack_head_count;
    uint8_t ring_count;
    int16_t explode_count;
    int16_t lizard_item_num;
    int16_t lizard_room_num;
    bool dropped_item;
    XYZ_32 beam_target;
    M_SHIELD_POINT shield[5][8];
    XYZ_32 trig_dynamics[3];
    bool shield_on;
    bool shield_active;
    bool turned;
    M_LIZARD_SUMMON_COORDS lizard_summon_coords[2];
    M_SHARED_PRIV *shared;
} M_PRIV;

static const BITE m_HelmetSpikes[5] = {
    // Helmet spikes
    { .pos = { .x = 120, .y = 68, .z = 136 }, .mesh_num = 8 },
    { .pos = { .x = 128, .y = -64, .z = 136 }, .mesh_num = 8 },
    { .pos = { .x = 8, .y = -120, .z = 136 }, .mesh_num = 8 },
    { .pos = { .x = -128, .y = -64, .z = 136 }, .mesh_num = 8 },
    { .pos = { .x = -124, .y = 64, .z = 126 }, .mesh_num = 8 },
};

static BITE m_EnergyHit = {
    .pos = { .x = 8, .y = 32, .z = 400 },
    .mesh_num = 8,
};

static const int32_t m_Heights[5] = { -1536, -1280, -832, -384, 0 };
static const int32_t m_Dist[5] = { 200, 400, 500, 500, 475 };
static const int32_t m_DDist[5] = { 1600, 5600, 6400, 5600, 1600 };
static const int32_t m_DHeights1[5] = { -7680, -4224, -768, 2688, 6144 };
static const int32_t m_DHeights2[5] = { -1536, -1152, -768, -384, 0 };
static int32_t m_DeathDist[5] = {};
static int32_t m_DeathHeights[5] = {};

static M_SHARED_PRIV m_SharedPriv = {};

static int16_t M_FindLizard(const int16_t room_num)
{
    for (int32_t item_num = 0; item_num < Item_GetTotalCount(); item_num++) {
        const ITEM *const item = Item_Get(item_num);
        if (item->object_id == O_LIZARD && item->room_num == room_num) {
            return item_num;
        }
    }
    return NO_ITEM;
}

static void M_RotateHeadXAngle(ITEM *const item)
{
    XYZ_32 lpos = {};
    Collide_GetJointAbsPosition(Lara_GetItem(), &lpos, LM_HIPS);

    XYZ_32 pos = {};
    Collide_GetJointAbsPosition(item, &pos, 0);

    const int32_t dx = ABS(pos.x - lpos.x);
    const int32_t dy = pos.y - lpos.y;
    const int32_t dz = ABS(pos.z - lpos.z);
    const int16_t ang = Math_Atan(Math_Sqrt(SQUARE(dx) + SQUARE(dz)), dy);
    if (ABS(ang) < 0x2000) {
        Creature_Joint(item, 2, ang);
    } else {
        Creature_Joint(item, 2, 0);
    }
}

static void M_Explode(ITEM *const item)
{
    M_PRIV *const p = item->priv;
    p->shield_on = false;

    if (p->explode_count == 1 || p->explode_count == 15
        || p->explode_count == 25 || p->explode_count == 35
        || p->explode_count == 45 || p->explode_count == 55) {
        const XYZ_32 pos = {
            .x = (Random_GetDraw() & 0x3FF) + item->pos.x - 512,
            .y = item->pos.y - (Random_GetDraw() & 0x3FF) - 256,
            .z = (Random_GetDraw() & 0x3FF) + item->pos.z - 512,
        };
        FX_RING *const ring =
            FX_Ring_GetRing(FX_RING_TYPE_BLAST, p->ring_count);
        if (ring != nullptr) {
            ring->pos = pos;
            ring->on = 4;
            FX_Ring_Sync(ring);
        }
        p->ring_count++;
        Sparks_TriggerExplosionSparks(pos, 3, -2, 2, 0);

        for (int32_t i = 0; i < 2; i++) {
            Sparks_TriggerExplosionSparks(pos, 3, -1, 2, 0);
        }

        Sound_Effect(SFX_BLAST_CIRCLE, &item->pos, 0x800000 | SPM_PITCH);
    }

    for (int32_t i = 0; i < 5; i++) {
        if (p->explode_count < 128) {
            m_DeathDist[i] =
                (m_DDist[i] >> 4) + ((p->explode_count * m_DDist[i]) >> 7);
            m_DeathHeights[i] = m_DHeights2[i]
                + ((p->explode_count * (m_DHeights1[i] - m_DHeights2[i])) >> 7);
        }
    }

    const int32_t time4 = Output_GetTimeInGame() * 4;
    for (int32_t i = 0; i < 5; i++) {
        const int32_t y = m_DeathHeights[i];
        const int32_t rad = m_DeathDist[i];
        int32_t angle = (time4 & 0x3F) << 3;
        for (int32_t j = 0; j < 8; j++) {
            M_SHIELD_POINT *const shield = &p->shield[i][j];
            shield->pos = (XYZ_16) {
                .x = ((rad * Math_Sin(angle << 4)) >> 13),
                .y = y,
                .z = ((rad * Math_Cos(angle << 4)) >> 13),
            };

            if (i == 0 || i == 16 || p->explode_count >= 64) {
                shield->color = (RGB_888) {};
            } else {
                const int32_t m = 64 - p->explode_count;
                int32_t r = (m * (Random_GetDraw() & 0x1F)) >> 6;
                int32_t b = (Random_GetDraw() & 0x3F) + 224;
                int32_t g = (m * ((b >> 2) + (Random_GetDraw() & 0x3F))) >> 6;
                b = (m * b) >> 6;
                shield->color = (RGB_888) { r, g, b };
            }

            angle = (angle + 512) & 0xFFF;
        }
    }
}

static void M_Die(int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    item->collidable = 0;
    item->hit_points = 0;
    Item_Kill(item_num);
    LOT_DisableBaddieAI(item_num);
    item->flags |= IF_INVISIBLE;
}

static bool M_CanBeExploded(const ITEM *const item)
{
    return false;
}

static void M_TriggerSummonSmoke(const XYZ_32 pos)
{
    SPARK *const spark = Sparks_GetFreeSpark();
    spark->on = true;
    spark->src_color.r = 16;
    spark->src_color.g = 64;
    spark->src_color.b = 0;
    spark->dst_color.r = 8;
    spark->dst_color.g = 32;
    spark->dst_color.b = 0;
    spark->fade_to_black = 64;
    spark->col_fade_speed = (Random_GetControl() & 7) + 16;
    spark->draw_type = DRAW_BLEND_ADD;
    spark->life = (Random_GetControl() & 0xF) + 96;
    spark->s_life = spark->life;
    spark->extras = 0;
    spark->dynamic = -1;
    spark->pos.x = pos.x + (Random_GetControl() & 0x7F) - 64;
    spark->pos.y = pos.y - (Random_GetControl() & 0x1F);
    spark->pos.z = pos.z + (Random_GetControl() & 0x7F) - 64;
    spark->vel.x = (Random_GetControl() & 0xFF) - 128;
    spark->vel.y = -16 - (Random_GetControl() & 0xF);
    spark->vel.z = (Random_GetControl() & 0xFF) - 128;
    spark->friction = 0;

    if (Random_GetControl() & 1) {
        spark->flags = SPARK_F_ALT_SPRITE | SPARK_F_OUTSIDE | SPARK_F_ROTATE
            | SPARK_F_SPRITE | SPARK_F_SCALE;
        spark->rot_angle = Random_GetControl() & 0xFFF;

        if (Random_GetControl() & 1) {
            spark->rot_add = -4 - (Random_GetControl() & 7);
        } else {
            spark->rot_add = (Random_GetControl() & 7) + 4;
        }
    } else {
        spark->flags = SPARK_F_ALT_SPRITE | SPARK_F_OUTSIDE | SPARK_F_SPRITE
            | SPARK_F_SCALE;
    }

    spark->scalar = 3;
    spark->sprite_idx = (uint8_t)Object_Get(O_EXPLOSION_1)->mesh_idx;
    spark->gravity = -8 - (Random_GetControl() & 7);
    spark->max_y_vel = -4 - (Random_GetControl() & 7);
    spark->dst_size.width = (Random_GetControl() & 0x1F) + 128;
    spark->src_size.width = spark->dst_size.width >> 1;
    spark->size.width = spark->dst_size.width >> 1;
    spark->dst_size.height =
        spark->dst_size.width + (Random_GetControl() & 0x1F) + 32;
    spark->src_size.height = spark->dst_size.height >> 1;
    spark->size.height = spark->dst_size.height >> 1;
    Sparks_FinishSetup(spark);
}

static bool M_CanDropItems(const ITEM *const item)
{
    if (item->hit_points > 0) {
        return false;
    }
    if ((item->flags & IF_KILLED) != 0) {
        return true;
    }
    return item->current_anim_state == M_STATE_DEATH
        && Item_GetRelativeFrame(item) > 119;
}

static const M_LIZARD_SUMMON_COORDS *M_GetLizardSummonCoords(
    const M_PRIV *const p)
{
    return &p->lizard_summon_coords[p->attack_type == M_ATTACK_HAND_1 ? 0 : 1];
}

static void M_TriggerLizard(M_PRIV *const p)
{
    ITEM *const item = Item_Get(p->lizard_item_num);
    int16_t room_num = p->lizard_room_num;

    item->object_id = O_LIZARD;
    item->pos = M_GetLizardSummonCoords(p)->pos;
    item->anim_num = Object_Get(O_LIZARD)->anim_idx;
    item->frame_num = Item_GetAnim(item)->frame_base;
    item->current_anim_state = Item_GetAnim(item)->current_anim_state;
    item->goal_anim_state = Item_GetAnim(item)->current_anim_state;
    item->required_anim_state = M_STATE_WAIT;
    item->rot.x = 0;

    if (p->attack_type == M_ATTACK_HAND_1) {
        item->rot.y = -0x8000;
    } else {
        item->rot.y = 0;
    }

    item->rot.z = 0;
    item->timer = 0;
    item->flags = 0;
    item->creature_data = nullptr;
    item->mesh_bits = -1;
    item->hit_points = Object_Get(O_LIZARD)->hit_points;
    item->active = 0;
    item->status = IS_ACTIVE;
    item->collidable = 1;
    item->flags &= ~(IF_KILLED | IF_ONE_SHOT);
    item->include_in_kill_stats = false;

    // Item_Kill removes it from room item chains; reinsert even when room is
    // unchanged.
    Item_UpdateRoom(p->lizard_item_num, NO_ROOM);
    Item_UpdateRoom(p->lizard_item_num, room_num);

    Item_AddActive(p->lizard_item_num);
    LOT_EnableBaddieAI(p->lizard_item_num, true);

    Room_GetSector(item->pos, &room_num);

    if (item->room_num != room_num) {
        Item_UpdateRoom(p->lizard_item_num, room_num);
    }

    TribeBoss_SetLizardActive(true);
}

static void M_TriggerElectricSparks(
    const ITEM *const item, const XYZ_32 pos, const bool shield)
{
    M_PRIV *const p = item->priv;
    const ITEM *const lara_item = Lara_GetItem();
    const int32_t dx = lara_item->pos.x - pos.x;
    const int32_t dz = lara_item->pos.z - pos.z;
    if (dx < -0x5000 || dx > 0x5000 || dz < -0x5000 || dz > 0x5000) {
        return;
    }

    p->trig_dynamics[1] = pos;

    SPARK *const spark = Sparks_GetFreeSpark();
    spark->on = true;
    spark->src_color.r = 255;
    spark->src_color.g = 255;
    spark->src_color.b = 255;

    if (shield) {
        spark->dst_color.r = 255;
        spark->dst_color.g = (Random_GetControl() & 0x7F) + 64;
        spark->dst_color.b = 0;
    } else if (
        p->attack_type == M_ATTACK_HAND_1
        || p->attack_type == M_ATTACK_HAND_2) {
        spark->dst_color.r = 0;
        spark->dst_color.b = (Random_GetControl() & 0x7F) + 64;
        spark->dst_color.g = (spark->dst_color.b >> 1) + 128;
    } else {
        spark->dst_color.r = 0;
        spark->dst_color.g = (Random_GetControl() & 0x7F) + 64;
        spark->dst_color.b = (spark->dst_color.g >> 1) + 128;
    }

    spark->col_fade_speed = 3;
    spark->fade_to_black = 8;
    spark->life = 16;
    spark->s_life = 16;
    spark->draw_type = DRAW_BLEND_ADD;
    spark->extras = 0;
    spark->dynamic = -1;
    spark->pos.x = (Random_GetControl() & 0x1F) + pos.x - 16;
    spark->pos.y = (Random_GetControl() & 0x1F) + pos.y - 16;
    spark->pos.z = (Random_GetControl() & 0x1F) + pos.z - 16;
    spark->vel.x = 4 * (Random_GetControl() & 0x1FF) - 1024;
    spark->vel.y = 2 * (Random_GetControl() & 0x1FF) - 512;
    spark->vel.z = 4 * (Random_GetControl() & 0x1FF) - 1024;

    if (shield) {
        spark->vel.x >>= 1;
        spark->vel.y >>= 1;
        spark->vel.z >>= 1;
    }

    spark->friction = 4;
    spark->flags = SPARK_F_SCALE;
    spark->scalar = 3;
    spark->size.width = (Random_GetControl() & 1) + 1;
    spark->src_size.width = spark->size.width;
    spark->dst_size.width = (Random_GetControl() & 3) + 4;
    spark->size.height = (Random_GetControl() & 1) + 1;
    spark->src_size.height = spark->size.height;
    spark->dst_size.height = (Random_GetControl() & 3) + 4;
    spark->gravity = 15;
    spark->max_y_vel = 0;
    Sparks_FinishSetup(spark);
}

static bool M_LaraOnLOS(
    const GAME_VECTOR *const src, const GAME_VECTOR *const beam_target)
{
    XYZ_32 lara_pos = {};
    Collide_GetJointAbsPosition(Lara_GetItem(), &lara_pos, LM_HIPS);

    const int32_t bx = beam_target->x - src->x;
    const int32_t by = beam_target->y - src->y;
    const int32_t bz = beam_target->z - src->z;
    const int32_t lx = lara_pos.x - src->x;
    const int32_t ly = lara_pos.y - src->y;
    const int32_t lz = lara_pos.z - src->z;

    const int64_t beam_len2 =
        (int64_t)bx * bx + (int64_t)by * by + (int64_t)bz * bz;
    if (beam_len2 == 0) {
        return false;
    }

    const int64_t proj = (int64_t)lx * bx + (int64_t)ly * by + (int64_t)lz * bz;
    if (proj <= 0 || proj > beam_len2) {
        return false;
    }

    // Keep kill logic close to the actual beam spine, not just generic LOS.
    const int64_t lara_len2 =
        (int64_t)lx * lx + (int64_t)ly * ly + (int64_t)lz * lz;
    int64_t dist2_num = lara_len2 * beam_len2 - proj * proj;
    if (dist2_num < 0) {
        dist2_num = 0;
    }

    const int32_t hit_radius = 192;
    const int64_t max_dist2_num = (int64_t)hit_radius * hit_radius * beam_len2;
    if (dist2_num > max_dist2_num) {
        return false;
    }

    GAME_VECTOR lara_target = {
        .pos = { .x = lara_pos.x, .y = lara_pos.y, .z = lara_pos.z },
        .room_num = Lara_GetItem()->room_num,
    };
    return LOS_Check(src, &lara_target, true);
}

static int32_t M_GetElectricIntensity(
    const int32_t c, const int32_t scale, const bool copy)
{
    int32_t value = c;
    if (value > 255) {
        value = 511 - value;
        if (value < 0) {
            value = 0;
        }
    }
    if (copy) {
        value >>= 1;
    }
    return (scale * value) >> 6;
}

static void M_DrawElectricBeamQuad(
    const ITEM *const item, const XYZ_32 prev_l, const XYZ_32 prev_r,
    const XYZ_32 cur_l, const XYZ_32 cur_r, const int32_t c0, const int32_t c1,
    const bool copy)
{
    const M_PRIV *const p = item->priv;
    const int32_t i0 = M_GetElectricIntensity(c0, 64, copy);
    const int32_t i1 = M_GetElectricIntensity(c1, 64, copy);
    RGBA_8888 c_prev = {};
    RGBA_8888 c_cur = {};

    if (p->attack_type == M_ATTACK_HEAD) {
        c_prev = (RGBA_8888) { i0 >> 1, i0, i0, 0xC0 };
        c_cur = (RGBA_8888) { i1 >> 1, i1, i1, 0xC0 };
    } else {
        c_prev = (RGBA_8888) { i0 >> 1, i0, i0 >> 1, 0xC0 };
        c_cur = (RGBA_8888) { i1 >> 1, i1, i1 >> 1, 0xC0 };
    }

    const XYZ_32 world_pos[4] = { prev_l, prev_r, cur_r, cur_l };
    const RGBA_8888 color[4] = { c_prev, c_prev, c_cur, c_cur };
    const float disp[4][2] = {};
    OutputSource_PolyFX_StageQuadExt(
        -1, world_pos, disp, color,
        VERT_FLAT_SHADED | VERT_NO_LIGHTING | VERT_NO_WIBBLE, DRAW_BLEND_ADD);
}

static void M_TriggerElectricBeam(
    const ITEM *const item, GAME_VECTOR *const src, const bool copy,
    const bool do_state, const bool do_render)
{
    M_PRIV *const p = item->priv;
    GAME_VECTOR target = {};
    const int16_t angle = (item->rot.y >> 4) & 0xFFF;
    src->room_num = item->room_num;

    int32_t dx = p->beam_target.x - src->x;
    int32_t dy = p->beam_target.y - src->y;
    int32_t dz = p->beam_target.z - src->z;
    int32_t longest = ABS(dx);
    if (ABS(dy) > longest) {
        longest = ABS(dy);
    }
    if (ABS(dz) > longest) {
        longest = ABS(dz);
    }
    if (longest < 20480) {
        longest = 20480 / longest + 1;
        dx *= longest;
        dy *= longest;
        dz *= longest;
    }

    target.x = src->x + dx;
    target.y = src->y + dy;
    target.z = src->z + dz;
    LOS_Check(src, &target, true);

    if (do_state) {
        LARA_INFO *const lara_info = Lara_GetLaraInfo();
        ITEM *const lara_item = Lara_GetItem();
        if (lara_info->electric == 0 && !copy && p->attack_type == M_ATTACK_HEAD
            && M_LaraOnLOS(src, &target)
            && !g_Config.debug.enable_invulnerability) {
            lara_item->hit_points = 0;
            lara_info->electric = 1;
        }

        M_TriggerElectricSparks(item, target.pos, false);
    }

    if (!do_render) {
        return;
    }

    dx = ABS(target.x - src->x);
    dz = ABS(target.z - src->z);
    int32_t n_segments = dx >= dz ? dx >> 8 : dz >> 8;
    CLAMP(n_segments, 8, 24);

    const XYZ_32 src_pos = { src->x, src->y, src->z };
    const XYZ_32 dst_pos = { target.x, target.y, target.z };

    const int16_t side_angle = (angle + 1024) & 0xFFF;
    const int32_t x_off = (p->attack_type == M_ATTACK_HEAD)
        ? (Math_Sin(side_angle << 4) >> 10)
        : (Math_Sin(side_angle << 4) >> 11);
    const int32_t z_off = (p->attack_type == M_ATTACK_HEAD)
        ? (Math_Cos(side_angle << 4) >> 10)
        : (Math_Cos(side_angle << 4) >> 11);

    int32_t y_off1 = 0;
    int32_t y_off2 = 0;
    XYZ_32 prev_l = src_pos;
    XYZ_32 prev_r = src_pos;
    int32_t prev_c = 0;

    for (int32_t i = 1; i <= n_segments; i++) {
        const XYZ_32 center = {
            .x = src_pos.x + ((dst_pos.x - src_pos.x) * i) / n_segments,
            .y = src_pos.y + ((dst_pos.y - src_pos.y) * i) / n_segments,
            .z = src_pos.z + ((dst_pos.z - src_pos.z) * i) / n_segments,
        };

        int32_t c = 0;
        XYZ_32 cur_l = center;
        XYZ_32 cur_r = center;
        if (i != n_segments) {
            const XYZ_16 point = Lara_Electricity_GetPoint((copy ? 4 : 0) + i);
            const int32_t xs = copy ? -point.x : point.x;
            const int32_t ys = copy ? -(point.y >> 1) : (point.y >> 1);
            const int32_t zs = copy ? -point.z : point.z;

            y_off1 += (Random_GetDraw() & 0x1F) - 16;
            y_off2 += (Random_GetDraw() & 0x1F) - 16;
            CLAMP(y_off1, -192, 192);
            CLAMP(y_off2, -192, 192);

            cur_l = (XYZ_32) {
                center.x + xs + x_off,
                center.y + ys + y_off1,
                center.z + zs + z_off,
            };
            cur_r = (XYZ_32) {
                center.x + xs - x_off,
                center.y + ys + y_off2,
                center.z + zs - z_off,
            };
            c = (Random_GetDraw() & 0xFF) >> copy;
        }

        M_DrawElectricBeamQuad(
            item, prev_l, prev_r, cur_l, cur_r, prev_c, c, copy);
        prev_l = cur_l;
        prev_r = cur_r;
        prev_c = c;
    }
}

static void M_DrawElectricChain(
    const XYZ_32 start, const XYZ_32 end, const bool copy, const int32_t scale,
    int32_t *const point_idx)
{
    XYZ_32 prev = start;
    int32_t prev_c = 0;
    for (int32_t j = 1; j <= 4; j++) {
        XYZ_32 cur = {
            .x = start.x + ((end.x - start.x) * j) / 4,
            .y = start.y + ((end.y - start.y) * j) / 4,
            .z = start.z + ((end.z - start.z) * j) / 4,
        };
        int32_t cur_c = 0;

        if (j != 4) {
            const XYZ_16 point = Lara_Electricity_GetPoint(*point_idx);
            (*point_idx)++;
            const int32_t ex = copy ? -point.x : point.x;
            const int32_t ey = copy ? -point.y : point.y;
            const int32_t ez = copy ? -point.z : point.z;
            cur.x += ex >> 3;
            cur.y += ey >> 3;
            cur.z += ez >> 3;

            cur_c = ABS(ex);
            if (ABS(ey) > cur_c) {
                cur_c = ABS(ey);
            }
            if (ABS(ez) > cur_c) {
                cur_c = ABS(ez);
            }
        }

        int32_t i0 = M_GetElectricIntensity(prev_c, scale, copy);
        int32_t i1 = M_GetElectricIntensity(cur_c, scale, copy);
        CLAMP(i0, 0, 255);
        CLAMP(i1, 0, 255);
        const RGBA_8888 c0 = { 0, (uint8_t)i0, (uint8_t)i0, 0xC0 };
        const RGBA_8888 c1 = { 0, (uint8_t)i1, (uint8_t)i1, 0xC0 };
        const float width = 4.0f;
        OutputSource_PolyFX_StageLineSegment(
            prev, c0, cur, c1, width, DRAW_BLEND_ADD);
        prev = cur;
        prev_c = cur_c;
    }
}

static void M_TriggerHeadElectricity(
    const ITEM *const item, const bool copy, const bool do_state,
    const bool do_render)
{
    M_PRIV *const p = item->priv;
    const ITEM *const lara_item = Lara_GetItem();
    const int32_t dx = lara_item->pos.x - item->pos.x;
    const int32_t dz = lara_item->pos.z - item->pos.z;
    if (dx < -0x4800 || dx > 0x4800 || dz < -0x4800 || dz > 0x4800) {
        return;
    }

    const int32_t time4 = Output_GetTimeInGame() * 4;
    const int32_t s = (Math_Sin(time4 << 8) >> 10) + 64;
    int32_t point_idx = 0;

    for (int32_t i = 0; i < 4; i++) {
        XYZ_32 pos1 = m_HelmetSpikes[i].pos;
        XYZ_32 pos2 = m_HelmetSpikes[i + 1].pos;
        Collide_GetJointAbsPosition(item, &pos1, m_HelmetSpikes[i].mesh_num);
        Collide_GetJointAbsPosition(
            item, &pos2, m_HelmetSpikes[i + 1].mesh_num);

        if (i == 2 && do_state) {
            p->trig_dynamics[0] = pos1;
        }
        if (do_render) {
            M_DrawElectricChain(pos1, pos2, copy, s, &point_idx);
        }
    }

    if (p->attack_count != 0 && p->death_count == 0
        && p->attack_type == M_ATTACK_HEAD) {
        int32_t extra_idx = 16;
        for (int32_t i = 0; i < 5; i++) {
            XYZ_32 pos1 = m_HelmetSpikes[i].pos;
            XYZ_32 pos2 = m_EnergyHit.pos;
            Collide_GetJointAbsPosition(
                item, &pos1, m_HelmetSpikes[i].mesh_num);
            Collide_GetJointAbsPosition(item, &pos2, m_EnergyHit.mesh_num);

            int32_t scale = s;
            if (p->attack_count < 64) {
                scale = (p->attack_count * scale) >> 6;
            }
            if (do_render) {
                M_DrawElectricChain(pos1, pos2, copy, scale, &extra_idx);
            }

            if (do_state && i == 4 && p->attack_count >= 64
                && p->attack_count <= 128) {
                p->trig_dynamics[2] = pos2;
            }
        }
    }

    if (p->attack_count != 0 && p->death_count == 0
        && (p->attack_type == M_ATTACK_HAND_1
            || p->attack_type == M_ATTACK_HAND_2)) {
        XYZ_32 pos = {};
        Collide_GetJointAbsPosition(item, &pos, 14);
        GAME_VECTOR src = {
            .pos = { .x = pos.x, .y = pos.y, .z = pos.z },
            .room_num = item->room_num,
        };

        if (p->attack_count >= 64) {
            if (p->attack_count <= 128) {
                if (do_state) {
                    p->trig_dynamics[2] = pos;
                }
            }

            if (p->attack_count <= 96) {
                if (do_state && p->attack_count > 90
                    && !p->shared->lizard_active) {
                    M_TriggerLizard(p);
                }

                M_TriggerElectricBeam(item, &src, copy, do_state, do_render);

                if (do_state) {
                    for (int32_t i = 0; i < 3; i++) {
                        M_TriggerSummonSmoke(p->beam_target);
                    }
                }
            }
        }
    } else if (
        p->attack_count > 64 && p->death_count == 0
        && p->attack_type == M_ATTACK_HEAD) {
        GAME_VECTOR src = { .pos = m_EnergyHit.pos,
                            .room_num = item->room_num };
        Collide_GetJointAbsPosition(item, &src.pos, 8);
        M_TriggerElectricBeam(item, &src, copy, do_state, do_render);
    }
}

static void M_FindClosestShieldPoint(ITEM *const item, const XYZ_32 pos)
{
    M_PRIV *const p = item->priv;
    const int32_t affected[5] = {
        0, -1, 1, -8, 8,
    };
    int32_t best_dist = INT32_MAX;
    int32_t point = 0;

    for (int32_t i = 0; i < 40; i++) {
        const M_SHIELD_POINT *const shield = &p->shield[i >> 3][i & 7];
        if (i >= 16 && i <= 23) {
            const int32_t dx = shield->pos.x + item->pos.x - pos.x;
            const int32_t dy = shield->pos.y + item->pos.y - pos.y;
            const int32_t dz = shield->pos.z + item->pos.z - pos.z;
            const int32_t dist = SQUARE(dx) + SQUARE(dy) + SQUARE(dz);
            if (dist < best_dist) {
                best_dist = dist;
                point = i;
            }
        }
    }

    LARA_INFO *const lara_info = Lara_GetLaraInfo();
    int32_t c;
    switch (lara_info->gun_type) {
    case LGT_PISTOLS:
    case LGT_UZIS:
        c = 144;
        break;

    case LGT_MAGNUMS:
    case LGT_AUTOS:
    case LGT_DESERT_EAGLE:
    case LGT_HARPOON:
        c = 200;
        break;

    case LGT_SHOTGUN:
    case LGT_M16:
    case LGT_MP5:
        c = 192;
        break;

    case LGT_ROCKET:
    case LGT_GRENADE:
        c = 224;
        break;

    default:
        c = 180;
        break;
    }

    for (int32_t i = 0; i < 5; i++) {
        int32_t n = point + affected[i];

        if ((n & 7) == 7 && affected[i] == -1) {
            n += 8;
        }

        if ((n & 7) == 0 && affected[i] == 1) {
            n -= 8;
        }

        M_SHIELD_POINT *const shield = &p->shield[n >> 3][n & 7];
        int32_t r = shield->color.r;
        int32_t g = shield->color.g;
        int32_t b = shield->color.b;

        if (i == 0) {
            if (c >= 200) {
                r = c;
            } else {
                r += c >> 2;
                if (r > c) {
                    r = c;
                }
            }
        } else {
            if (c >= 200) {
                r = c >> 1;
            } else {
                r += c >> 3;
                if (r > (c >> 1)) {
                    r = c >> 1;
                }
            }
        }

        if (i == 0) {
            if (c >= 200) {
                g = c;
            } else {
                g += c >> 2;
                if (g > c) {
                    g = c;
                }
            }
        } else {
            if (c >= 200) {
                g = c >> 1;
            } else {
                g += c >> 3;
                if (g > (c >> 1)) {
                    g = c >> 1;
                }
            }
        }

        if (i == 0) {
            if (c >= 200) {
                b = c;
            } else {
                b += c >> 2;
                if (b > c) {
                    b = c;
                }
            }
        } else {
            if (c >= 200) {
                b = c >> 1;
            } else {
                b += c >> 3;
                if (b > (c >> 1)) {
                    b = c >> 1;
                }
            }
        }

        shield->sub.r = (Random_GetControl() & 7) + 8;
        shield->sub.g = (Random_GetControl() & 7) + 8;
        shield->sub.b = (Random_GetControl() & 7) + 8;

        if (lara_info->gun_type == LGT_ROCKET
            || lara_info->gun_type == LGT_GRENADE) {
            shield->sub.r >>= 1;
            shield->sub.g >>= 1;
            shield->sub.b >>= 1;
        }

        shield->color = (RGB_888) { r, g, b };
    }

    for (int32_t i = 0; i < 7; i++) {
        M_TriggerElectricSparks(item, pos, true);
    }
}

static bool M_GunHit(
    ITEM *const item, const GAME_VECTOR *const start,
    const GAME_VECTOR *const hit_pos, int32_t *const damage)
{
    M_PRIV *const p = item->priv;
    if (p->shield_on) {
        p->shield_active = true;
        if (hit_pos != nullptr) {
            M_FindClosestShieldPoint(item, hit_pos->pos);
        }
        if (damage != nullptr) {
            *damage = 0;
        }
        return false;
    }
    return true;
}

static bool M_ShouldSpawnBlood(const ITEM *const item)
{
    M_PRIV *const p = item->priv;
    return !p->shield_on;
}

static void M_UpdateShield(ITEM *const item)
{
    M_PRIV *const p = item->priv;
    for (int32_t i = 8; i <= 31; i++) {
        M_SHIELD_POINT *const shield = &p->shield[i >> 3][i & 7];
        shield->color.r = MAX(0, (int32_t)shield->color.r - shield->sub.r);
        shield->color.g = MAX(0, (int32_t)shield->color.g - shield->sub.g);
        shield->color.b = MAX(0, (int32_t)shield->color.b - shield->sub.b);
    }
}

static void M_DrawShield(const ITEM *const item)
{
    const M_PRIV *const p = item->priv;
    const int32_t time4 = Output_GetTimeInGame() * 4;
    const int32_t sprite_base = Object_Get(O_EXPLOSION_1)->mesh_idx;

    for (int32_t band = 0; band < 4; band++) {
        const int32_t sprite_idx =
            sprite_base + 18 + ((band + (time4 >> 3)) & 7);

        for (int32_t j = 0; j < 8; j++) {
            const int32_t j2 = (j == 7) ? 0 : (j + 1);
            const M_SHIELD_POINT *const s00 = &p->shield[band][j];
            const M_SHIELD_POINT *const s01 = &p->shield[band][j2];
            const M_SHIELD_POINT *const s10 = &p->shield[band + 1][j];
            const M_SHIELD_POINT *const s11 = &p->shield[band + 1][j2];

            const RGB_888 c00 = s00->color;
            const RGB_888 c01 = s01->color;
            const RGB_888 c10 = s10->color;
            const RGB_888 c11 = s11->color;

            if (((c00.r | c00.g | c00.b | c01.r | c01.g | c01.b | c11.r | c11.g
                  | c11.b | c10.r | c10.g | c10.b)
                 == 0U)) {
                continue;
            }
            const XYZ_32 world_pos[4] = {
                {
                    item->pos.x + s00->pos.x,
                    item->pos.y + s00->pos.y,
                    item->pos.z + s00->pos.z,
                },
                {
                    item->pos.x + s01->pos.x,
                    item->pos.y + s01->pos.y,
                    item->pos.z + s01->pos.z,
                },
                {
                    item->pos.x + s11->pos.x,
                    item->pos.y + s11->pos.y,
                    item->pos.z + s11->pos.z,
                },
                {
                    item->pos.x + s10->pos.x,
                    item->pos.y + s10->pos.y,
                    item->pos.z + s10->pos.z,
                },
            };
            const RGBA_8888 color[4] = {
                { c00.r, c00.g, c00.b, 255 },
                { c01.r, c01.g, c01.b, 255 },
                { c11.r, c11.g, c11.b, 255 },
                { c10.r, c10.g, c10.b, 255 },
            };
            OutputSource_PolyFX_StageSpriteQuadWorld(
                sprite_idx, world_pos, color, DRAW_BLEND_ADD);
        }
    }
}

static bool M_Draw(const ITEM *const item)
{
    M_PRIV *const p = item->priv;
    Object_DrawAnimatingItem(item);

    if (p->explode_count == 0) {
        M_TriggerHeadElectricity(item, false, false, true);
        M_TriggerHeadElectricity(item, true, false, true);
    }

    M_DrawShield(item);
    return true;
}

static void M_LoadPriv(ITEM *const item, JSON_READ_IO *const io)
{
    M_PRIV *const p = item->priv;
    p->shared = &m_SharedPriv;
    JSON_SHOULD(JSON_READ(io, "dead", &p->dead));
    JSON_SHOULD(JSON_READ(io, "attack_count", &p->attack_count));
    JSON_SHOULD(JSON_READ(io, "death_count", &p->death_count));
    JSON_SHOULD(JSON_READ(io, "attack_flag", &p->attack_flag));
    JSON_SHOULD(JSON_READ(io, "attack_type", &p->attack_type));
    JSON_SHOULD(JSON_READ(io, "attack_head_count", &p->attack_head_count));
    JSON_SHOULD(JSON_READ(io, "ring_count", &p->ring_count));
    JSON_SHOULD(JSON_READ(io, "explode_count", &p->explode_count));
    JSON_SHOULD(JSON_READ(io, "lizard_item_num", &p->lizard_item_num));
    JSON_SHOULD(JSON_READ(io, "lizard_room_num", &p->lizard_room_num));
    JSON_SHOULD(JSON_READ(io, "dropped_item", &p->dropped_item));
    JSON_SHOULD(JSON_READ(io, "shield_on", &p->shield_on));
    JSON_SHOULD(JSON_READ(io, "shield_active", &p->shield_active));
    JSON_SHOULD(JSON_READ(io, "turned", &p->turned));
    JSON_SHOULD(JSON_READ(io, "beam_target_x", &p->beam_target.x));
    JSON_SHOULD(JSON_READ(io, "beam_target_y", &p->beam_target.y));
    JSON_SHOULD(JSON_READ(io, "beam_target_z", &p->beam_target.z));
    JSON_SHOULD(
        JSON_READ(io, "shared_lizard_active", &p->shared->lizard_active));
}

static void M_SavePriv(const ITEM *const item, JSON_WRITE_IO *const io)
{
    const M_PRIV *const p = item->priv;
    JSONW_WRITE(io, "dead", p->dead);
    JSONW_WRITE(io, "attack_count", p->attack_count);
    JSONW_WRITE(io, "death_count", p->death_count);
    JSONW_WRITE(io, "attack_flag", p->attack_flag);
    JSONW_WRITE(io, "attack_type", p->attack_type);
    JSONW_WRITE(io, "attack_head_count", p->attack_head_count);
    JSONW_WRITE(io, "ring_count", p->ring_count);
    JSONW_WRITE(io, "explode_count", p->explode_count);
    JSONW_WRITE(io, "lizard_item_num", p->lizard_item_num);
    JSONW_WRITE(io, "lizard_room_num", p->lizard_room_num);
    JSONW_WRITE(io, "dropped_item", p->dropped_item);
    JSONW_WRITE(io, "shield_on", p->shield_on);
    JSONW_WRITE(io, "shield_active", p->shield_active);
    JSONW_WRITE(io, "turned", p->turned);
    JSONW_WRITE(io, "beam_target_x", p->beam_target.x);
    JSONW_WRITE(io, "beam_target_y", p->beam_target.y);
    JSONW_WRITE(io, "beam_target_z", p->beam_target.z);
    JSONW_WRITE(io, "shared_lizard_active", p->shared->lizard_active);
}

static void M_Initialise(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;
    p->shared = &m_SharedPriv;
    p->lizard_item_num = M_FindLizard(item->room_num);
    p->lizard_room_num = Item_Get(p->lizard_item_num)->room_num;

    for (int32_t i = 0; i < 2; i++) {
        const int32_t sign = i == 0 ? -1 : 1;
        int32_t y_rot = item->rot.y + DEG_180; // after turning
        XYZ_32 pos = item->pos;
        pos = XYZ_32_OffsetYaw(pos, y_rot, WALL_L * 3);
        pos = XYZ_32_OffsetYaw(pos, y_rot + DEG_270 * sign, WALL_L * 5);

        int16_t room_num = item->room_num;
        const SECTOR *const sector = Room_GetSector(pos, &room_num);
        pos.y = Room_GetHeight(sector, pos);

        p->lizard_summon_coords[i].pos = pos;
        p->lizard_summon_coords[i].y_rot = y_rot - DEG_45 * sign;
    }

    for (int32_t i = 0; i < 3; i++) {
        p->trig_dynamics[i].x = 0;
    }

    p->dropped_item = false;
    p->dead = 0;
    p->ring_count = 0;
    p->explode_count = 0;
    p->attack_head_count = 0;
    p->death_count = 0;
    p->attack_flag = 0;
    p->attack_count = 0;
    p->shield_active = false;
    p->shield_on = false;
    TribeBoss_SetLizardActive(false);

    for (int32_t i = 0; i < 5; i++) {
        const int32_t y = m_Heights[i];
        const int32_t r = m_Dist[i];
        int32_t angle = 0;

        for (int32_t j = 0; j < 8; j++) {
            p->shield[i][j].pos.x = (int16_t)((r * Math_Sin(angle << 4)) >> 13);
            p->shield[i][j].pos.y = (int16_t)y;
            p->shield[i][j].pos.z = (int16_t)((r * Math_Cos(angle << 4)) >> 13);
            p->shield[i][j].color = (RGB_888) {};
            angle += 512;
        }
    }
}

static void M_Control(const int16_t item_num)
{
    if (!Creature_Activate(item_num)) {
        return;
    }

    ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;
    CREATURE *const creature = item->creature_data;
    int16_t angle = 0;
    int16_t head = 0;
    int16_t old_y_rot = INT16_MAX;

    Lara_Electricity_UpdatePoints();
    p->shield_active = false;
    M_UpdateShield(item);

    if (item->hit_points > 0) {
        AI_INFO info;
        Creature_AIInfo(item, &info);

        if (item->hit_status) {
            Sound_Effect(SFX_TRIBOSS_TAKE_HIT, &item->pos, SPM_NORMAL);
        }

        old_y_rot = item->rot.y;

        const ITEM *const lara_item = Lara_GetItem();
        if (p->attack_flag == 0) {
            const int32_t x_dist = item->pos.x - lara_item->pos.x;
            const int32_t z_dist = item->pos.z - lara_item->pos.z;

            if (SQUARE(x_dist) + SQUARE(z_dist) < 0x400000) {
                p->attack_flag = 1;
            }
        }

        creature->target.x = lara_item->pos.x;
        creature->target.z = lara_item->pos.z;

        if (!p->shared->lizard_active
            || item->current_anim_state != M_STATE_WAIT) {
            angle = Creature_Turn(item, creature->maximum_turn);
        } else {
            const uint16_t y_test = item->rot.y;
            if (ABS(0xC000 - y_test) > DEG_1) {
                item->rot.y += (0xC000 - y_test) >> 3;
            } else {
                item->rot.y = -0x4000;
            }
        }

        M_RotateHeadXAngle(item);

        if (info.ahead) {
            head = info.angle;
        }

        switch (item->current_anim_state) {
        case M_STATE_WAIT:
            p->attack_count = 0;

            if (item->goal_anim_state != M_STATE_ATTACK_HEAD
                && item->goal_anim_state != M_STATE_ATTACK_HAND) {
                p->shield_on = true;
            }

            if (p->shared->lizard_active) {
                Creature_Joint(item, 1, head);
            } else {
                Creature_Joint(item, 1, 0);
            }

            if (p->attack_flag == 0 || p->shared->lizard_active) {
                creature->maximum_turn = 0;
            } else {
                creature->maximum_turn = 546;
            }

            if (item->goal_anim_state != M_STATE_ATTACK_HEAD
                && info.angle > -128 && info.angle < 128
                && lara_item->hit_points > 0
                && p->attack_head_count < M_MAX_HEAD_ATTACKS
                && !p->shared->lizard_active && !p->shield_active) {
                XYZ_32 pos = {};
                Collide_GetJointAbsPosition(lara_item, &pos, 0);
                p->beam_target = pos;
                item->goal_anim_state = M_STATE_ATTACK_HEAD;
                creature->maximum_turn = 0;
                p->shield_on = false;
                p->attack_head_count++;
                break;
            }

            if (item->goal_anim_state == M_STATE_ATTACK_HAND
                || p->attack_head_count < M_MAX_HEAD_ATTACKS
                || lara_item->hit_points <= 0) {
                break;
            }

            creature->maximum_turn = 0;

            if (p->attack_type == M_ATTACK_HEAD) {
                const int32_t x1 =
                    lara_item->pos.x - p->lizard_summon_coords[0].pos.x;
                const int32_t z1 =
                    lara_item->pos.z - p->lizard_summon_coords[0].pos.z;
                const int32_t x2 =
                    lara_item->pos.x - p->lizard_summon_coords[1].pos.x;
                const int32_t z2 =
                    lara_item->pos.z - p->lizard_summon_coords[1].pos.z;

                if (SQUARE(x1) + SQUARE(z1) > SQUARE(x2) + SQUARE(z2)) {
                    p->attack_type = M_ATTACK_HAND_1;
                } else {
                    p->attack_type = M_ATTACK_HAND_2;
                }
            }

            const uint16_t y_test = item->rot.y;
            const uint16_t y_rot = M_GetLizardSummonCoords(p)->y_rot;
            if (ABS(y_rot - y_test) >= DEG_1) {
                item->rot.y += (y_rot - y_test) >> 4;
            } else {
                item->rot.y = y_rot;

                if (!p->shield_active) {
                    item->goal_anim_state = M_STATE_ATTACK_HAND;
                    p->beam_target = M_GetLizardSummonCoords(p)->pos;
                    creature->maximum_turn = 0;
                    p->attack_head_count = 0;
                    p->shield_on = false;
                }
            }
            break;

        case M_STATE_ATTACK_HEAD:
            creature->maximum_turn = 0;
            p->attack_count += 3;
            p->attack_type = M_ATTACK_HEAD;
            Creature_Joint(item, 1, 0);
            break;

        case M_STATE_ATTACK_HAND:
            creature->maximum_turn = 0;

            if (p->attack_count < 64) {
                p->attack_count += 2;
            } else {
                p->attack_count += 3;
            }

            Creature_Joint(item, 1, 0);
            break;
        }
    } else {
        if (item->current_anim_state != M_STATE_DEATH) {
            Item_SwitchToAnim(item, M_ANIM_DEATH, 0);
            item->current_anim_state = M_STATE_DEATH;
            p->death_count = 1;
        }

        if (Item_GetRelativeFrame(item) > 119) {
            item->mesh_bits = 0;
            Item_SwitchToAnim(item, Item_GetRelativeAnim(item), 120);
            p->death_count = -1;

            if (p->explode_count == 0) {
                p->ring_count = 0;
                Sound_Effect(SFX_EXPLOSION_2, &item->pos, SPM_NORMAL);

                for (int32_t i = 0; i < 6; i++) {
                    FX_RING *const ring =
                        FX_Ring_GetRing(FX_RING_TYPE_BLAST, i);
                    if (ring == nullptr) {
                        continue;
                    }
                    ring->on = 0;
                    ring->life = 32;
                    ring->radius = 512;
                    ring->speed = (i << 5) + 128;
                    ring->rot.x = ((Random_GetControl() & 0x1FF) - 256) & 0xFFF;
                    ring->rot.z = ((Random_GetControl() & 0x1FF) - 256) & 0xFFF;
                    FX_Ring_Sync(ring);
                }

                if (!p->dropped_item) {
                    Carrier_TestItemDrops(item_num);
                    p->dropped_item = true;
                }
            }

            if (p->explode_count < 256) {
                p->explode_count++;
            }

            if (p->explode_count <= 128 || p->ring_count != 6
                || FX_Ring_GetRing(FX_RING_TYPE_BLAST, 5)->life != 0) {
                M_Explode(item);
            } else {
                M_Die(item_num);
                p->dead = 1;
            }

            return;
        }

        item->rot.z =
            Random_GetControl() % p->death_count - (p->death_count >> 1);

        if (p->death_count < 2048) {
            p->death_count += 32;
        }
    }

    if (p->attack_count != 0 && p->attack_type == M_ATTACK_HEAD
        && p->attack_count < 64) {
        m_EnergyHit.pos.z = 4 * p->attack_count + 136;
    }

    creature->joint_rotation[0] += 1274;
    Creature_Animate(item_num, angle, 0);

    p->trig_dynamics[0].x = 0;
    p->trig_dynamics[1].x = 0;
    p->trig_dynamics[2].x = 0;
    if (p->explode_count == 0) {
        M_TriggerHeadElectricity(item, false, true, false);
        M_TriggerHeadElectricity(item, true, true, false);
    }

    for (int32_t i = 0; i < 3; i++) {
        if (!p->trig_dynamics[i].x) {
            continue;
        }

        XYZ_32 pos;
        RGB_888 color;
        int32_t falloff;
        if (i == 0) {
            pos = p->trig_dynamics[0];
            falloff = (Random_GetControl() & 3) + 8;

            color.r = 0;
            color.g = (Random_GetControl() & 0x3F) + 64;
            color.b = (Random_GetControl() & 0x3F) + 128;
        } else if (i == 1) {
            pos = p->trig_dynamics[1];
            falloff = (Random_GetControl() & 7) + 8;

            color.r = 0;
            if (p->attack_type == M_ATTACK_HEAD) {
                color.g = (Random_GetControl() & 0x3F) + 64;
                color.b = (Random_GetControl() & 0x3F) + 128;
            } else {
                color.g = (Random_GetControl() & 0x3F) + 128;
                color.b = (Random_GetControl() & 0x3F) + 64;
            }
        } else {
            if (p->attack_count == 0) {
                continue;
            }

            pos = p->trig_dynamics[2];
            falloff = (128 - p->attack_count) >> 1;
            CLAMPG(falloff, 31);
            if (falloff <= 0) {
                continue;
            }

            color.r = falloff << 1;
            if (p->attack_type == M_ATTACK_HEAD) {
                color.g = (Random_GetControl() & 0x3F) + 128;
                color.b = (Random_GetControl() & 0x3F) + 192;
            } else {
                color.g = (Random_GetControl() & 0x3F) + 192;
                color.b = (Random_GetControl() & 0x3F) + 128;
            }
        }

        Output_AddDynamicLightRGB(pos, falloff, color);
    }

    if (old_y_rot != item->rot.y && !p->turned) {
        p->turned = true;
        Sound_Effect(SFX_TRIBOSS_TURN_CHAIR, &item->pos, 0x800000 | SPM_PITCH);
        return;
    }

    if (old_y_rot == item->rot.y) {
        p->turned = false;
    }
}

static void M_Setup(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }

    obj->priv_size = sizeof(M_PRIV);
    obj->priv_load_func = M_LoadPriv;
    obj->priv_save_func = M_SavePriv;
    obj->initialise_func = M_Initialise;
    obj->control_func = M_Control;
    obj->collision_func = Creature_Collision;
    obj->draw_func = M_Draw;
    obj->gun_hit_func = M_GunHit;
    obj->should_spawn_blood_func = M_ShouldSpawnBlood;
    obj->can_drop_items_func = M_CanDropItems;
    obj->can_be_exploded_func = M_CanBeExploded;

    obj->shadow_size = 0;
    obj->hit_points = 200;
    obj->pivot_length = 50;
    obj->radius = 102;

    obj->intelligent = true;
    obj->save_position = true;
    obj->save_hitpoints = true;
    obj->save_flags = true;
    obj->save_anim = true;

    Object_GetBone(obj, 4)->rot.y = true;
    Object_GetBone(obj, 7)->rot.y = true;
    Object_GetBone(obj, 7)->rot.x = true;
}

bool TribeBoss_IsLizardActive(void)
{
    return m_SharedPriv.lizard_active;
}

void TribeBoss_SetLizardActive(const bool active)
{
    m_SharedPriv.lizard_active = active;
}

REGISTER_OBJECT(O_TRIBE_BOSS, M_Setup)
