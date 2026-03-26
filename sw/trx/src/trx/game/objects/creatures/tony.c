#include "tony_internal.h"

#include <trx/core/json/util/read_io.h>
#include <trx/core/json/util/write_io.h>
#include <trx/core/utils.h>
#include <trx/game/creature.h>
#include <trx/game/fx/explosion_ring.h>
#include <trx/game/items/carrier.h>
#include <trx/game/lara.h>
#include <trx/game/objects/common.h>
#include <trx/game/output.h>
#include <trx/game/output/sources/poly_fx.h>
#include <trx/game/pathing.h>
#include <trx/game/random.h>
#include <trx/game/rooms.h>
#include <trx/game/sound.h>
#include <trx/game/sparks.h>
#include <trx/game/spawn.h>

typedef enum {
    M_PHASE_DORMANT = 0,
    M_PHASE_AWAKENED = 1,
    M_PHASE_PHASE2 = 2,
} M_PHASE;

typedef enum {
    M_STATE_WAIT,
    M_STATE_RISE,
    M_STATE_FLOAT,
    M_STATE_ZAPP,
    M_STATE_ROCK_ZAPP,
    M_STATE_BIG_ROOM,
    M_STATE_DEATH
} M_STATE;

typedef struct {
    XYZ_16 pos;
    RGB_888 sub;
    RGB_888 color;
} M_SHIELD_POINT;

typedef struct {
    bool dropped_item;
    uint8_t ring_count;
    int16_t explode_count;
    bool dead;
    M_SHIELD_POINT shield[5][8];
    M_PHASE phase;

    // Alternates the chosen attack while in the FLOAT state (ROCK_ZAPP vs ZAPP)
    bool attack_toggle;
} M_PRIV;

static const int32_t m_Heights[5] = { -1536, -1280, -832, -384, 0 };
static const int32_t m_Dist[5] = { 200, 400, 500, 500, 475 };
static const int32_t m_DDist[5] = { 1600, 5600, 6400, 5600, 1600 };
static const int32_t m_DHeights1[5] = { -7680, -4224, -768, 2688, 6144 };
static const int32_t m_DHeights2[5] = { -1536, -1152, -768, -384, 0 };
static int32_t m_DeathDist[5] = {};
static int32_t m_DeathHeights[5] = {};

static void M_LoadPriv(ITEM *const item, JSON_READ_IO *const io)
{
    M_PRIV *const p = item->priv;
    JSON_SHOULD(JSON_READ(io, "dropped_item", &p->dropped_item));
    JSON_SHOULD(JSON_READ(io, "ring_count", &p->ring_count));
    JSON_SHOULD(JSON_READ(io, "explode_count", &p->explode_count));
    JSON_SHOULD(JSON_READ(io, "dead", &p->dead));
    JSON_SHOULD(JSON_READ(io, "phase", &p->phase));
    JSON_SHOULD(JSON_READ(io, "attack_toggle", &p->attack_toggle));
}

static void M_SavePriv(const ITEM *const item, JSON_WRITE_IO *const io)
{
    const M_PRIV *const p = item->priv;
    JSONW_WRITE(io, "dropped_item", p->dropped_item);
    JSONW_WRITE(io, "ring_count", p->ring_count);
    JSONW_WRITE(io, "explode_count", p->explode_count);
    JSONW_WRITE(io, "dead", p->dead);
    JSONW_WRITE(io, "phase", p->phase);
    JSONW_WRITE(io, "attack_toggle", p->attack_toggle);
}

static void M_TriggerFlame(int16_t item_num, int32_t node)
{
    const ITEM *const lara_item = Lara_GetItem();

    ITEM *const item = Item_Get(item_num);
    const int32_t dx = lara_item->pos.x - item->pos.x;
    const int32_t dz = lara_item->pos.z - item->pos.z;

    if (dx < -0x4000 || dx > 0x4000 || dz < -0x4000 || dz > 0x4000) {
        return;
    }

    SPARK *const spark = Sparks_GetFreeSpark();
    spark->on = true;
    spark->src_color.r = 255;
    spark->src_color.g = (Random_GetControl() & 0x1F) + 48;
    spark->src_color.b = 48;
    spark->dst_color.r = (Random_GetControl() & 0x3F) + 192;
    spark->dst_color.g = (Random_GetControl() & 0x3F) + 128;
    spark->dst_color.b = 32;

    spark->fade_to_black = 8;
    spark->col_fade_speed = (Random_GetControl() & 3) + 12;
    spark->draw_type = DRAW_BLEND_ADD;
    spark->extras = 0;
    spark->life = (Random_GetControl() & 7) + 24;
    spark->s_life = spark->life;
    spark->dynamic = -1;
    spark->pos.x = (Random_GetControl() & 0xF) - 8;
    spark->pos.y = 0;
    spark->pos.z = (Random_GetControl() & 0xF) - 8;
    spark->vel.x = (Random_GetControl() & 0xFF) - 128;
    spark->vel.y = -16 - (Random_GetControl() & 0xF);
    spark->vel.z = (Random_GetControl() & 0xFF) - 128;
    spark->friction = 5;

    if (Random_GetControl() & 1) {
        spark->flags = SPARK_F_ATTACHED_NODE | SPARK_F_ALT_SPRITE | SPARK_F_ITEM
            | SPARK_F_ROTATE | SPARK_F_SPRITE | SPARK_F_SCALE;
        spark->rot_angle = Random_GetControl() & 0xFFF;

        if (Random_GetControl() & 1) {
            spark->rot_add = -16 - (Random_GetControl() & 0xF);
        } else {
            spark->rot_add = (Random_GetControl() & 0xF) + 16;
        }
    } else {
        spark->flags = SPARK_F_ATTACHED_NODE | SPARK_F_ALT_SPRITE | SPARK_F_ITEM
            | SPARK_F_SPRITE | SPARK_F_SCALE;
    }

    spark->gravity = -16 - (Random_GetControl() & 0x1F);
    spark->max_y_vel = -16 - (Random_GetControl() & 7);
    spark->item_num = item_num;
    spark->node_num = node;
    spark->sprite_idx = Object_Get(O_EXPLOSION_1)->mesh_idx;
    spark->scalar = 1;
    spark->size.width = (Random_GetControl() & 0x1F) + 64;
    spark->src_size.width = spark->size.width;
    spark->dst_size.width = spark->size.width >> 2;
    spark->size.height = spark->size.width;
    spark->src_size.height = spark->size.height;
    spark->dst_size.height = spark->size.height >> 2;
    Sparks_FinishSetup(spark);
}

static void M_Explode(ITEM *const item)
{
    M_PRIV *const p = item->priv;

    if (item->hit_points <= 0
        && (p->explode_count == 1 || p->explode_count == 15
            || p->explode_count == 25 || p->explode_count == 35
            || p->explode_count == 45 || p->explode_count == 55)) {
        const XYZ_32 pos = {
            .x = item->pos.x + (Random_GetDraw() & 0x3FF) - 512,
            .y = item->pos.y - (Random_GetDraw() & 0x3FF) - 256,
            .z = item->pos.z + (Random_GetDraw() & 0x3FF) - 512,
        };

        FX_RING *const ring =
            FX_Ring_GetRing(FX_RING_TYPE_BLAST, p->ring_count);
        if (ring != nullptr) {
            ring->pos = pos;
            ring->on = 2;
            FX_Ring_Sync(ring);
            p->ring_count++;
        }

        Sparks_TriggerExplosionSparks(pos, 3, -2, 0, 0);
        for (int32_t i = 0; i < 2; i++) {
            Sparks_TriggerExplosionSparks(pos, 3, -1, 0, 0);
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

    if (p->explode_count > 64) {
        return;
    }

    for (int32_t i = 0; i < 5; i++) {
        const int32_t y = m_DeathHeights[i];
        const int32_t dist = m_DeathDist[i];
        const int32_t time4 = Output_GetTimeInGame() * 4;
        int32_t angle = (time4 & 0x3F) << 3;

        for (int32_t j = 0; j < 8; j++) {
            M_SHIELD_POINT *const shield = &p->shield[i][j];
            shield->pos.x = (dist * Math_Sin(angle << 4)) >> 13;
            shield->pos.y = y;
            shield->pos.z = (dist * Math_Cos(angle << 4)) >> 13;

            if (i == 0 || i == 16 || p->explode_count >= 64) {
                shield->color = COLOR_RGB_888_BLACK;
            } else {
                int32_t r = (Random_GetDraw() & 0x1F) + 224;
                int32_t g = (r >> 2) + (Random_GetDraw() & 0x3F);
                int32_t b = Random_GetDraw() & 0x3F;

                if (item->hit_points <= 0) {
                    r = ((64 - p->explode_count) * r) >> 6;
                    g = ((64 - p->explode_count) * g) >> 6;
                    b = ((64 - p->explode_count) * b) >> 6;
                } else {
                    r = ((128 - p->explode_count) * r) >> 7;
                    g = ((128 - p->explode_count) * g) >> 7;
                    b = ((128 - p->explode_count) * b) >> 7;
                }

                shield->color = (RGB_888) { r, g, b };
            }

            angle = (angle + 512) & 0xFFF;
        }
    }
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
        && Item_GetRelativeFrame(item) >= 110;
}

static bool M_CanBeExploded(const ITEM *const item)
{
    return false;
}

static void M_Die(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    item->hit_points = 0;
    item->collidable = false;
    Item_Kill(item_num);
    LOT_DisableBaddieAI(item_num);
    item->flags |= IF_INVISIBLE;
}

static void M_Initialise(int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;
    p->dead = false;
    p->dropped_item = false;
    p->ring_count = 0;
    p->explode_count = 0;
    p->attack_toggle = false;
    p->phase = M_PHASE_DORMANT;

    for (int32_t i = 0; i < 5; i++) {
        const int32_t dist = m_Dist[i];
        int32_t angle = 0;

        for (int32_t j = 0; j < 8; j++) {
            M_SHIELD_POINT *const shield = &p->shield[i][j];
            shield->pos.x = (dist * Math_Sin(angle << 4)) >> 13;
            shield->pos.y = m_Heights[i];
            shield->pos.z = (dist * Math_Cos(angle << 4)) >> 13;
            shield->color = COLOR_RGB_888_BLACK;
            angle += 512;
        }
    }
}

static void M_Control(const int16_t item_num)
{
    if (!Creature_Activate(item_num)) {
        return;
    }

    const ITEM *const lara_item = Lara_GetItem();

    ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;
    CREATURE *const tony = item->creature_data;
    int16_t angle = 0;
    int16_t torso_x = 0;
    int16_t torso_y = 0;

    if (item->hit_points <= 0) {
        if (item->current_anim_state != M_STATE_DEATH) {
            Item_SwitchToAnim(item, 6, 0);
            item->current_anim_state = M_STATE_DEATH;
        }

        if (Item_GetRelativeFrame(item) > 110) {
            Item_SwitchToAnim(item, Item_GetRelativeAnim(item), 110);
            item->mesh_bits = 0;

            if (!p->explode_count) {
                p->ring_count = 0;

                for (int32_t i = 0; i < 6; i++) {
                    FX_RING *const ring =
                        FX_Ring_GetRing(FX_RING_TYPE_BLAST, i);
                    if (ring == nullptr) {
                        continue;
                    }
                    ring->on = 0;
                    ring->life = 32;
                    ring->radius = 512;
                    ring->speed = 128 + (i << 5);
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

            if (p->explode_count > 128 && p->ring_count == 6
                && FX_Ring_GetRing(FX_RING_TYPE_BLAST, 5)->life == 0) {
                M_Die(item_num);
                p->dead = true;
            } else {
                M_Explode(item);
            }

            return;
        }
    } else {
        if (p->phase != M_PHASE_PHASE2) {
            item->hit_points = item->max_hit_points;
        }

        AI_INFO info;
        Creature_AIInfo(item, &info);

        if (p->phase != M_PHASE_DORMANT) {
            tony->target.x = lara_item->pos.x;
            tony->target.z = lara_item->pos.z;
            angle = Creature_Turn(item, tony->maximum_turn);
        } else {
            const int32_t x = item->pos.x - lara_item->pos.x;
            const int32_t z = item->pos.z - lara_item->pos.z;
            if (SQUARE(x) + SQUARE(z) < 0x1900000) {
                p->phase = M_PHASE_AWAKENED;
            }

            angle = 0;
        }

        switch (item->current_anim_state) {
        case M_STATE_WAIT:
            tony->maximum_turn = 0;
            if (item->goal_anim_state != M_STATE_RISE
                && p->phase != M_PHASE_DORMANT) {
                item->goal_anim_state = M_STATE_RISE;
            }
            break;

        case M_STATE_RISE:
            if (Item_GetRelativeFrame(item) <= 16) {
                tony->maximum_turn = 0;
            } else {
                tony->maximum_turn = 364;
            }
            break;

        case M_STATE_FLOAT:
            torso_y = info.angle;
            torso_x = info.x_angle;
            tony->maximum_turn = 364;

            if (p->explode_count == 0) {
                if (item->goal_anim_state != M_STATE_BIG_ROOM
                    && p->phase != M_PHASE_PHASE2) {
                    item->goal_anim_state = M_STATE_BIG_ROOM;
                    tony->maximum_turn = 0;
                }

                const int32_t time4 = Output_GetTimeInGame() * 4;
                if (item->goal_anim_state != M_STATE_ROCK_ZAPP
                    && p->phase == M_PHASE_PHASE2 && !(time4 & 0xFF)
                    && !p->attack_toggle) {
                    item->goal_anim_state = M_STATE_ROCK_ZAPP;
                    p->attack_toggle = true;
                }

                if (item->goal_anim_state != M_STATE_ZAPP
                    && item->goal_anim_state != M_STATE_ROCK_ZAPP
                    && p->phase == M_PHASE_PHASE2 && !(time4 & 0xFF)
                    && p->attack_toggle) {
                    item->goal_anim_state = M_STATE_ZAPP;
                    p->attack_toggle = false;
                }
            }
            break;

        case M_STATE_ZAPP:
            torso_y = info.angle;
            torso_x = info.x_angle;
            tony->maximum_turn = 182;
            if (Item_GetRelativeFrame(item) == 28) {
                TonyBoss_TriggerFireBall(
                    item, 2, nullptr, item->room_num, item->rot.y, 0);
            }
            break;

        case M_STATE_ROCK_ZAPP:
            torso_y = info.angle;
            torso_x = info.x_angle;
            tony->maximum_turn = 0;
            if (Item_GetRelativeFrame(item) == 40) {
                TonyBoss_TriggerFireBall(
                    item, 0, nullptr, item->room_num, 0, 0);
                TonyBoss_TriggerFireBall(
                    item, 1, nullptr, item->room_num, 0, 0);
            }
            break;

        case M_STATE_BIG_ROOM:
            tony->maximum_turn = 0;
            if (Item_GetRelativeFrame(item) == 56) {
                p->phase = M_PHASE_PHASE2;
                p->explode_count = 1;
            }
            break;
        }
    }

    if (item->current_anim_state == M_STATE_ROCK_ZAPP
        || item->current_anim_state == M_STATE_ZAPP
        || item->current_anim_state == M_STATE_BIG_ROOM) {
        int32_t f = Item_GetRelativeFrame(item);
        if (f > 16) {
            f = Anim_GetAnim(item->anim_num)->frame_end - item->frame_num;
            CLAMPG(f, 16);
        }

        const int32_t r = Random_GetControl();
        const RGB_888 color = {
            .r = (f * (255 - ((r >> 4) & 0x1F))) >> 4,
            .g = (f * (192 - ((r >> 6) & 0x1F))) >> 4,
            .b = (f * (r & 0x3F)) >> 4,
        };

        XYZ_32 pos = {};
        Collide_GetJointAbsPosition(item, &pos, 10);
        Output_AddDynamicLightRGB(pos, 12, color);
        M_TriggerFlame(item_num, 5);

        if (item->current_anim_state == M_STATE_ROCK_ZAPP
            || item->current_anim_state == M_STATE_BIG_ROOM) {
            pos.x = 0;
            pos.y = 0;
            pos.z = 0;
            Collide_GetJointAbsPosition(item, &pos, 13);
            Output_AddDynamicLightRGB(pos, 12, color);
            M_TriggerFlame(item_num, 4);
        }
    }

    if (p->explode_count != 0 && item->hit_points > 0) {
        M_Explode(item);
        p->explode_count++;

        if (p->explode_count == 32) {
            Room_FlipMap();
        }

        if (p->explode_count > 64) {
            p->ring_count = 0;
            p->explode_count = 0;
        }
    }

    Creature_Joint(item, 0, torso_y >> 1);
    Creature_Joint(item, 1, torso_x);
    Creature_Joint(item, 2, torso_y >> 1);
    Creature_Animate(item_num, angle, 0);
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

            const int32_t idx00 = band * 8 + j;
            const int32_t idx01 = band * 8 + j2;
            const int32_t idx10 = (band + 1) * 8 + j;
            const int32_t idx11 = (band + 1) * 8 + j2;

            RGB_888 c00 = s00->color;
            RGB_888 c01 = s01->color;
            RGB_888 c10 = s10->color;
            RGB_888 c11 = s11->color;

            if (idx00 >= 8 && idx00 <= 31) {
                c00.r = (uint8_t)MAX(0, (int32_t)c00.r - (int32_t)s00->sub.r);
                c00.g = (uint8_t)MAX(0, (int32_t)c00.g - (int32_t)s00->sub.g);
                c00.b = (uint8_t)MAX(0, (int32_t)c00.b - (int32_t)s00->sub.b);
            }
            if (idx01 >= 8 && idx01 <= 31) {
                c01.r = (uint8_t)MAX(0, (int32_t)c01.r - (int32_t)s01->sub.r);
                c01.g = (uint8_t)MAX(0, (int32_t)c01.g - (int32_t)s01->sub.g);
                c01.b = (uint8_t)MAX(0, (int32_t)c01.b - (int32_t)s01->sub.b);
            }
            if (idx10 >= 8 && idx10 <= 31) {
                c10.r = (uint8_t)MAX(0, (int32_t)c10.r - (int32_t)s10->sub.r);
                c10.g = (uint8_t)MAX(0, (int32_t)c10.g - (int32_t)s10->sub.g);
                c10.b = (uint8_t)MAX(0, (int32_t)c10.b - (int32_t)s10->sub.b);
            }
            if (idx11 >= 8 && idx11 <= 31) {
                c11.r = (uint8_t)MAX(0, (int32_t)c11.r - (int32_t)s11->sub.r);
                c11.g = (uint8_t)MAX(0, (int32_t)c11.g - (int32_t)s11->sub.g);
                c11.b = (uint8_t)MAX(0, (int32_t)c11.b - (int32_t)s11->sub.b);
            }

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
    if ((item->hit_points <= 0 && p->explode_count > 64)) {
        return false;
    }

    const bool result = Object_DrawAnimatingItem(item);
    if (p->explode_count != 0) {
        if (p->explode_count != 0 && p->explode_count <= 64) {
            M_DrawShield(item);
        }
    }
    return result;
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
    obj->can_drop_items_func = M_CanDropItems;
    obj->can_be_exploded_func = M_CanBeExploded;

    obj->shadow_size = 0;
    obj->hit_points = 100;
    obj->pivot_length = 50;
    obj->radius = 102;

    obj->intelligent = true;
    obj->save_position = true;
    obj->save_hitpoints = true;
    obj->save_flags = true;
    obj->save_anim = true;

    Object_GetBone(obj, 6)->rot.x = true;
    Object_GetBone(obj, 6)->rot.y = true;
    Object_GetBone(obj, 13)->rot.y = true;
}

REGISTER_OBJECT(O_TONY, M_Setup)
