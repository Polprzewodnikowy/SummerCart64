#include "sophia_internal.h"

#include <trx/core/json/util/read_io.h>
#include <trx/core/json/util/write_io.h>
#include <trx/core/math/geom.h>
#include <trx/game/creature.h>
#include <trx/game/effects.h>
#include <trx/game/fx/explosion_ring.h>
#include <trx/game/items/anim.h>
#include <trx/game/items/carrier.h>
#include <trx/game/lara.h>
#include <trx/game/lara/electric.h>
#include <trx/game/objects.h>
#include <trx/game/output.h>
#include <trx/game/output/sources/poly_fx.h>
#include <trx/game/pathing.h>
#include <trx/game/random.h>
#include <trx/game/sound.h>
#include <trx/game/sparks.h>

// clang-format off
#define M_SMALL_FLASH    10

#define M_RIGHT_PRONG    0
#define M_CENTER_PRONG   1
#define M_LEFT_PRONG     2

#define M_VAULT_SHIFT    96
#define M_AWARE_DISTANCE SQUARE(WALL_L)
#define M_WALK_TURN      (4 * DEG_1)
#define M_RUN_TURN       (7 * DEG_1)
#define M_WALK_RANGE     SQUARE(WALL_L)
#define M_LAUGH_CHANCE   0x100
#define M_FINAL_HEIGHT   -11776
#define M_BIG_ZAP_TIMER  600
// clang-format on

typedef enum {
    M_STATE_NULL,
    M_STATE_STAND,
    M_STATE_WALK,
    M_STATE_RUN,
    M_STATE_SUMMON,
    M_STATE_BIG_ZAP,
    M_STATE_DEATH,
    M_STATE_LAUGH,
    M_STATE_LITTLE_ZAP,
    M_STATE_VAULT_2,
    M_STATE_VAULT_3,
    M_STATE_VAULT_4,
    M_STATE_GO_DOWN
} M_STATE;

typedef enum {
    // clang-format off
    M_ANIM_STAND_TO_SUMMON = 1,
    M_ANIM_SUMMON          = 2,
    M_ANIM_UP_2            = 9,
    M_ANIM_UP_3            = 18,
    M_ANIM_UP_4            = 15,
    M_ANIM_DEATH           = 17,
    M_ANIM_DOWN_4          = 21,
    // clang-format on
} M_ANIM;

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
    bool charged;
    int16_t death_counter;
    int16_t hp_counter;
    int16_t fuse_box_num;
    uint8_t wand_glow_phase;
    bool knockback_active;
    M_SHIELD_POINT shield[5][8];
} M_PRIV;

static const BITE m_WandBite[3] = {
    { .pos = { .x = 16, .y = 56, .z = 356 }, .mesh_num = 10 },
    { .pos = { .x = -28, .y = 48, .z = 304 }, .mesh_num = 10 },
    { .pos = { .x = -72, .y = 48, .z = 356 }, .mesh_num = 10 },
};

static const int32_t m_Heights[5] = { -1536, -1280, -832, -384, 0 };
static const int32_t m_Dist[5] = { 200, 400, 500, 500, 475 };
static const int32_t m_DDist[5] = { 1600, 5600, 6400, 5600, 1600 };
static const int32_t m_DHeights1[5] = { -7680, -4224, -768, 2688, 6144 };
static const int32_t m_DHeights2[5] = { -1536, -1152, -768, -384, 0 };
static int32_t m_DeathDist[5] = {};
static int32_t m_DeathHeights[5] = {};

static void M_ExplodeLondonBoss(const ITEM *const item)
{
    M_PRIV *const p = item->priv;

    if (p->explode_count == 1 || p->explode_count == 15
        || p->explode_count == 25 || p->explode_count == 35
        || p->explode_count == 45 || p->explode_count == 55) {
        const XYZ_32 pos = {
            .x = item->pos.x + (Random_GetDraw() & 0x3FF) - 512,
            .y = item->pos.y - (Random_GetDraw() & 0x3FF) - 256,
            .z = item->pos.z + (Random_GetDraw() & 0x3FF) - 512,
        };

        FX_RING *const ring =
            FX_Ring_GetRing(FX_RING_TYPE_BLAST, p->ring_count);
        if (ring != nullptr) {
            ring->pos = pos;
            ring->on = 3;
            FX_Ring_Sync(ring);
            p->ring_count++;
        }

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
                const int32_t r = Random_GetDraw() & 0x3F;
                const int32_t g = (Random_GetDraw() & 0x1F) + 224;
                const int32_t b = (g >> 2) + (Random_GetDraw() & 0x3F);
                shield->color = (RGB_888) {
                    .r = ((64 - p->explode_count) * r) >> 6,
                    .g = ((64 - p->explode_count) * g) >> 6,
                    .b = ((64 - p->explode_count) * b) >> 6,
                };
            }

            angle = (angle + 512) & 0xFFF;
        }
    }
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

static bool M_KnockBackCollision(const FX_RING *const ring)
{
    ITEM *const lara_item = Lara_GetItem();
    if (Lara_GetLaraInfo()->water_status == LWS_CHEAT) {
        return false;
    }

    const XYZ_32 delta = {
        .x = lara_item->pos.x - ring->pos.x,
        .y = 0,
        .z = lara_item->pos.z - ring->pos.z,
    };
    if (XYZ_32_GetLength2(delta) >= SQUARE(ring->radius)) {
        return false;
    }

    Lara_TakeDamage(200, true);

    const int16_t angle = Math_Atan(delta.z, delta.x);
    const int16_t dy = lara_item->rot.y - angle;
    if (ABS(dy) >= DEG_90) {
        lara_item->rot.y = angle + DEG_180;
        lara_item->speed = -75;
    } else {
        lara_item->rot.y = angle;
        lara_item->speed = 75;
    }

    lara_item->gravity = true;
    lara_item->fall_speed = -50;
    lara_item->rot.x = 0;
    lara_item->rot.z = 0;
    Item_SwitchToAnim(lara_item, LA(LA_FALL_START), 0);
    lara_item->current_anim_state = LS_JUMP_FORWARD;
    lara_item->goal_anim_state = LS_JUMP_FORWARD;

    Sparks_TriggerExplosionSparks(
        lara_item->pos, 3, -2, 2, lara_item->room_num);
    for (int32_t i = 0; i < 3; i++) {
        Sophia_TriggerPlasmaBall(
            2, lara_item->pos, lara_item->room_num, Random_GetControl() << 1);
    }

    return true;
}

static void M_AssignFuseBox(const ITEM *const item)
{
    M_PRIV *const p = item->priv;
    p->fuse_box_num = NO_ITEM;

    int32_t best_dist = INT32_MAX;
    for (int16_t i = 0; i < Item_GetLevelCount(); i++) {
        const ITEM *const other_item = Item_Get(i);
        if (other_item->object_id != O_FUSE_BOX) {
            continue;
        }

        const int32_t dist = XYZ_32_GetLength2((XYZ_32) {
            .x = other_item->pos.x - item->pos.x,
            .y = 0,
            .z = other_item->pos.z - item->pos.z,
        });
        if (dist < best_dist) {
            best_dist = dist;
            p->fuse_box_num = i;
        }
    }
}

static void M_LoadPriv(ITEM *const item, JSON_READ_IO *const io)
{
    M_PRIV *const p = item->priv;
    JSON_SHOULD(JSON_READ(io, "dropped_item", &p->dropped_item));
    JSON_SHOULD(JSON_READ(io, "ring_count", &p->ring_count));
    JSON_SHOULD(JSON_READ(io, "explode_count", &p->explode_count));
    JSON_SHOULD(JSON_READ(io, "dead", &p->dead));
    JSON_SHOULD(JSON_READ(io, "charged", &p->charged));
    JSON_SHOULD(JSON_READ(io, "death_counter", &p->death_counter));
    JSON_SHOULD(JSON_READ(io, "hp_counter", &p->hp_counter));
    JSON_SHOULD(JSON_READ(io, "fuse_box_num", &p->fuse_box_num));
}

static void M_SavePriv(const ITEM *const item, JSON_WRITE_IO *const io)
{
    const M_PRIV *const p = item->priv;
    JSONW_WRITE(io, "dropped_item", p->dropped_item);
    JSONW_WRITE(io, "ring_count", p->ring_count);
    JSONW_WRITE(io, "explode_count", p->explode_count);
    JSONW_WRITE(io, "dead", p->dead);
    JSONW_WRITE(io, "charged", p->charged);
    JSONW_WRITE(io, "death_counter", p->death_counter);
    JSONW_WRITE(io, "hp_counter", p->hp_counter);
    JSONW_WRITE(io, "fuse_box_num", p->fuse_box_num);
}

static void M_Initialise(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;
    p->dropped_item = false;
    p->dead = 0;
    p->ring_count = 0;
    p->explode_count = 0;
    M_AssignFuseBox(item);

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

static bool M_GunHit(
    ITEM *const item, const GAME_VECTOR *const start,
    const GAME_VECTOR *const hit_pos, int32_t *const damage)
{
    if (damage != nullptr) {
        *damage = 0;
    }
    return true;
}

static bool M_ShouldSpawnBlood(const ITEM *const item)
{
    return false;
}

static void M_Control(const int16_t item_num)
{
    if (!Creature_Activate(item_num)) {
        return;
    }

    ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;

    if (p->death_counter == 0 && p->fuse_box_num != NO_ITEM) {
        const ITEM *const fuse_box = Item_Get(p->fuse_box_num);
        if (!Item_TestAnimEqual(fuse_box, 0)) {
            p->death_counter = 1;
        }
    }

    ITEM *const lara_item = Lara_GetItem();

    CREATURE *const creature = item->creature_data;
    int16_t angle = 0;
    int16_t tilt = 0;
    int16_t head = 0;
    int16_t torso_x = 0;
    int16_t torso_y = 0;

    if (p->death_counter != 0) {
        if (p->death_counter == 1) {
            item->hit_points = 0;
        }

        RGB_888 color;
        int32_t falloff;
        if (p->death_counter < 12) {
            falloff = (Random_GetControl() & 1) - (p->death_counter << 1) + 25;
            color = (RGB_888) {
                .r = (Random_GetControl() & 0x3F) - (p->death_counter << 3)
                    + 128,
                .g = 256 - (p->death_counter << 3),
                .b = 255,
            };
        } else {
            falloff = (Random_GetControl() & 3) + 8;
            color = (RGB_888) {
                .r = 0,
                .g = (Random_GetControl() & 0x3F) + 64,
                .b = (Random_GetControl() & 0x3F) + 128,
            };
        }

        Output_AddDynamicLightRGB(item->pos, falloff, color);
    }

    XYZ_32 points[3];
    for (int32_t i = 0; i < 3; i++) {
        points[i] = m_WandBite[i].pos;
        Collide_GetJointAbsPosition(item, &points[i], m_WandBite[i].mesh_num);
    }

    if (item->hit_points <= 0) {
        if (item->current_anim_state != M_STATE_DEATH) {
            Item_SwitchToAnim(item, M_ANIM_DEATH, 0);
            item->current_anim_state = M_STATE_DEATH;
        }

        if (Item_TestFrameEqual(item, -2)) {
            item->mesh_bits = 0;
            item->frame_num = Item_GetAnim(item)->frame_end - 1;

            if (p->explode_count == 0) {
                p->ring_count = 0;

                for (int32_t i = 0; i < 6; i++) {
                    FX_RING *const ring =
                        FX_Ring_GetRing(FX_RING_TYPE_BLAST, i);
                    ring->on = 0;
                    ring->life = 32;
                    ring->radius = 512;
                    ring->speed = 128 + (i << 5);
                    ring->rot.x = ((Random_GetControl() & 0x1FF) - 256) << 4;
                    ring->rot.z = ((Random_GetControl() & 0x1FF) - 256) << 4;
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
                p->dead = 1;
            } else {
                M_ExplodeLondonBoss(item);
            }

            return;
        }
    } else {
        if (item->ai_bits) {
            Creature_GetAITarget(creature);
        }

        AI_INFO info;
        Creature_AIInfo(item, &info);

        AI_INFO lara_info;
        if (creature->enemy == lara_item) {
            lara_info.angle = info.angle;
            lara_info.x_angle = info.x_angle;
            lara_info.distance = info.distance;
        } else {
            int32_t x = lara_item->pos.x - item->pos.x;
            int32_t y = item->pos.y - lara_item->pos.y;
            int32_t z = lara_item->pos.z - item->pos.z;
            lara_info.angle = Math_Atan(z, x) - item->rot.y;
            lara_info.distance = SQUARE(x) + SQUARE(z);

            if (ABS(x) <= ABS(z)) {
                z = z + (x >> 1);
            } else {
                z = x + (z >> 1);
            }

            lara_info.x_angle = Math_Atan(z, y);
        }

        Creature_Mood(item, &info, true);
        angle = Creature_Turn(item, creature->maximum_turn);

        ITEM *const enemy = creature->enemy;
        creature->enemy = lara_item;

        if (item->hit_status || lara_info.distance < M_AWARE_DISTANCE
            || Creature_CanSeeEnemy(item, &lara_info)
            || lara_item->pos.y < item->pos.y) {
            Creature_AlertAllGuards(item_num);
        }

        creature->enemy = enemy;

        if (lara_item->pos.y < item->pos.y) {
            creature->hurt_by_lara = 1;
        }

        if (item->timer > 0) {
            item->timer--;
        }

        item->hit_points = 300;

        switch (item->current_anim_state) {
        case M_STATE_LAUGH:
            if (ABS(lara_info.angle) < M_WALK_TURN) {
                item->rot.y += lara_info.angle;
            } else if (lara_info.angle >= 0) {
                item->rot.y += M_WALK_TURN;
            } else {
                item->rot.y -= M_WALK_TURN;
            }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
            if (creature->alerted) {
                item->goal_anim_state = M_STATE_STAND;
                break;
            }
#pragma GCC diagnostic pop

        case M_STATE_STAND:
            creature->flags = 0;
            creature->maximum_turn = 0;

            if (creature->reached_goal) {
                creature->reached_goal = 0;
                item->ai_bits |= AI_AMBUSH;
                item->ai_tag += 0x2000;
            }

            head = lara_info.angle;

            if (item->ai_bits & AI_GUARD) {
                if ((lara_info.angle < -0x3000 || lara_info.angle > 0x3000)
                    && item->pos.y > M_FINAL_HEIGHT) {
                    item->goal_anim_state = M_STATE_WALK;
                    creature->maximum_turn = M_WALK_TURN;
                }
            } else if (
                (item->pos.y <= M_FINAL_HEIGHT
                 || item->pos.y < lara_item->pos.y)
                && !(Random_GetControl() & 0xF) && !p->charged && item->timer) {
                item->goal_anim_state = M_STATE_LAUGH;
            } else if (
                creature->reached_goal || lara_item->pos.y > item->pos.y
                || item->pos.y <= M_FINAL_HEIGHT) {
                if (p->charged) {
                    item->goal_anim_state = M_STATE_BIG_ZAP;
                } else if (item->timer) {
                    item->goal_anim_state = M_STATE_LITTLE_ZAP;
                } else {
                    item->goal_anim_state = M_STATE_SUMMON;
                }
            } else if (item->ai_bits & AI_PATROL_1) {
                item->goal_anim_state = M_STATE_WALK;
            } else if (
                creature->mood == MOOD_ESCAPE
                || item->pos.y > lara_item->pos.y) {
                item->goal_anim_state = M_STATE_RUN;
            } else if (
                creature->mood == MOOD_BORED
                || (item->ai_bits & AI_FOLLOW
                    && (creature->reached_goal
                        || lara_info.distance > 0x400000))) {
                if (item->required_anim_state) {
                    item->goal_anim_state = item->required_anim_state;
                } else if (info.ahead) {
                    item->goal_anim_state = M_STATE_STAND;
                } else {
                    item->goal_anim_state = M_STATE_RUN;
                }
            } else if (info.bite && info.distance < M_WALK_RANGE) {
                item->goal_anim_state = M_STATE_WALK;
            } else {
                item->goal_anim_state = M_STATE_RUN;
            }
            break;

        case M_STATE_WALK:
            head = lara_info.angle;
            creature->flags = 0;
            creature->maximum_turn = M_WALK_TURN;

            if (item->ai_bits & AI_GUARD
                || (creature->reached_goal && !(item->ai_bits & AI_FOLLOW))) {
                item->goal_anim_state = M_STATE_STAND;
            } else if (item->ai_bits & AI_PATROL_1) {
                item->goal_anim_state = M_STATE_WALK;
                head = 0;
            } else if (creature->mood == MOOD_ESCAPE) {
                item->goal_anim_state = M_STATE_RUN;
            } else if (creature->mood == MOOD_BORED) {
                if (Random_GetControl() < M_LAUGH_CHANCE) {
                    item->required_anim_state = M_STATE_LAUGH;
                    item->goal_anim_state = M_STATE_STAND;
                }
            } else if (info.distance > M_WALK_RANGE) {
                item->goal_anim_state = M_STATE_RUN;
            }
            break;

        case M_STATE_RUN:
            if (info.ahead) {
                head = info.angle;
            }

            creature->maximum_turn = M_RUN_TURN;
            tilt = angle >> 1;

            if (item->ai_bits & AI_GUARD
                || (creature->reached_goal && !(item->ai_bits & AI_FOLLOW))) {
                item->goal_anim_state = M_STATE_STAND;
            } else if (creature->mood != MOOD_ESCAPE) {
                if (item->ai_bits & AI_FOLLOW
                    && (creature->reached_goal
                        || lara_info.distance > 0x400000)) {
                    item->goal_anim_state = M_STATE_STAND;
                } else if (creature->mood == MOOD_BORED) {
                    item->goal_anim_state = M_STATE_WALK;
                } else if (info.ahead && info.distance < M_WALK_RANGE) {
                    item->goal_anim_state = M_STATE_WALK;
                }
            }
            break;

        case M_STATE_SUMMON:
            head = lara_info.angle;

            if (creature->reached_goal) {
                creature->reached_goal = 0;
                item->ai_bits = AI_AMBUSH;
                item->ai_tag += 0x2000;
            }

            if (Item_TestAnimEqual(item, M_ANIM_STAND_TO_SUMMON)) {
                if (Item_TestFrameEqual(item, 0)) {
                    p->hp_counter = item->hit_points;
                    item->timer = M_BIG_ZAP_TIMER;
                } else if (
                    item->hit_status
                    && item->goal_anim_state != M_STATE_STAND) {
                    Sound_StopEffect(SFX_SOPHIA_SUMMON);
                    Sound_Effect(SFX_SOPHIA_TAKE_HIT, &item->pos, SPM_NORMAL);
                    Sound_Effect(SFX_SOPHIA_SUMMON_NOT, &item->pos, SPM_NORMAL);
                    item->goal_anim_state = M_STATE_STAND;
                }
            } else if (
                Item_TestAnimEqual(item, M_ANIM_SUMMON)
                && Item_TestFrameEqual(item, -1)) {
                p->charged = true;
            }

            if (ABS(lara_info.angle) < M_WALK_TURN) {
                item->rot.y += lara_info.angle;
            } else if (lara_info.angle >= 0) {
                item->rot.y += M_WALK_TURN;
            } else {
                item->rot.y -= M_WALK_TURN;
            }

            const int32_t time4 = Output_GetTimeInGame() * 4;
            if ((time4 & 7) == 0) {
                XYZ_32 pos = {
                    .x = item->pos.x,
                    .y = (Random_GetControl() & 0x1FF) - 256,
                    .z = item->pos.z,
                };
                Sophia_TriggerLaserBolt(pos, item, 2, 0);

                for (int32_t i = 0; i < 6; i++) {
                    FX_RING *const ring =
                        FX_Ring_GetRing(FX_RING_TYPE_SUMMON, i);
                    if (ring->on == 0) {
                        const int32_t r = Random_GetControl() & 0x3FF;
                        ring->on = 3;
                        ring->life = 64;
                        ring->speed = (Random_GetControl() & 0xF) + 16;
                        ring->pos.x = item->pos.x;
                        ring->pos.y = item->pos.y - r + 128;
                        ring->pos.z = item->pos.z;
                        ring->rot.x =
                            16 * ((Random_GetControl() & 0x1FF) - 256);
                        ring->rot.z =
                            16 * ((Random_GetControl() & 0x1FF) - 256);
                        ring->radius = 2048 - ABS(r - 512);
                        FX_Ring_Sync(ring);
                        break;
                    }
                }
            }

            creature->maximum_turn = 0;
            break;

        case M_STATE_BIG_ZAP:
            if (creature->reached_goal) {
                creature->reached_goal = 0;
                item->ai_bits = AI_AMBUSH;
                item->ai_tag += 0x2000;
            }

            p->charged = false;

            if (ABS(lara_info.angle) < M_WALK_TURN) {
                item->rot.y += lara_info.angle;
            } else if (lara_info.angle >= 0) {
                item->rot.y += M_WALK_TURN;
            } else {
                item->rot.y -= M_WALK_TURN;
            }

            creature->maximum_turn = 0;
            torso_x = lara_info.x_angle;
            torso_y = lara_info.angle;

            if (Item_TestFrameEqual(item, 36)) {
                Sophia_TriggerLaserBolt(
                    points[M_RIGHT_PRONG], item, 0, item->rot.y + 512);
                Sophia_TriggerLaserBolt(
                    points[M_CENTER_PRONG], item, 1, item->rot.y);
                Sophia_TriggerLaserBolt(
                    points[M_LEFT_PRONG], item, 0, item->rot.y - 512);
            }
            break;

        case M_STATE_LITTLE_ZAP:
            if (creature->reached_goal) {
                creature->reached_goal = 0;
                item->ai_bits = AI_AMBUSH;
                item->ai_tag += 0x2000;
            }

            if (ABS(lara_info.angle) < M_WALK_TURN) {
                item->rot.y += lara_info.angle;
            } else if (lara_info.angle >= 0) {
                item->rot.y += M_WALK_TURN;
            } else {
                item->rot.y -= M_WALK_TURN;
            }

            creature->maximum_turn = 0;
            torso_x = lara_info.x_angle;
            torso_y = lara_info.angle;

            if (Item_TestFrameEqual(item, 14)) {
                Sophia_TriggerLaserBolt(
                    points[M_RIGHT_PRONG], item, 0, item->rot.y + 512);
                Sophia_TriggerLaserBolt(
                    points[M_LEFT_PRONG], item, 0, item->rot.y - 512);
            }
            break;
        }
    }

    Creature_Tilt(item, tilt);
    Creature_Joint(item, 0, torso_y);
    Creature_Joint(item, 1, torso_x);
    Creature_Joint(item, 2, head);

    if ((item->current_anim_state >= M_STATE_VAULT_2
         && item->current_anim_state <= M_STATE_GO_DOWN)
        || item->current_anim_state == M_STATE_DEATH) {
        creature->maximum_turn = 0;
        Creature_Animate(item_num, angle, 0);
    } else {
        switch (Creature_Vault(item_num, angle, 2, M_VAULT_SHIFT)) {
        case -4:
            creature->maximum_turn = 0;
            Item_SwitchToAnim(item, M_ANIM_DOWN_4, 0);
            item->current_anim_state = M_STATE_GO_DOWN;
            break;

        case 2:
            creature->maximum_turn = 0;
            Item_SwitchToAnim(item, M_ANIM_UP_2, 0);
            item->current_anim_state = M_STATE_VAULT_2;
            break;

        case 3:
            creature->maximum_turn = 0;
            Item_SwitchToAnim(item, M_ANIM_UP_3, 0);
            item->current_anim_state = M_STATE_VAULT_3;
            break;

        case 4:
            creature->maximum_turn = 0;
            Item_SwitchToAnim(item, M_ANIM_UP_4, 0);
            item->current_anim_state = M_STATE_VAULT_4;
            break;
        }
    }

    int32_t g = (Random_GetControl() & 7)
        + ABS(Math_Sin(p->wand_glow_phase << 10) >> 9);
    CLAMPG(g, 31);
    g <<= 3;
    Output_AddDynamicLightRGB(
        points[M_CENTER_PRONG], M_SMALL_FLASH, (RGB_888) { 0, g >> 1, g >> 2 });

    p->wand_glow_phase = (p->wand_glow_phase + 1) & 0x3F;

    if (item->hit_points > 0 && !p->knockback_active
        && lara_item->hit_points > 0) {
        const XYZ_32 delta = {
            .x = lara_item->pos.x - item->pos.x,
            .y = lara_item->pos.y - item->pos.y - 256,
            .z = lara_item->pos.z - item->pos.z,
        };
        if (XYZ_32_GetLength(delta) < 2816) {
            p->knockback_active = true;
            FX_Ring_SpawnKnockBack(item->pos);
        }
    } else if (p->knockback_active) {
        const FX_RING *const ring = FX_Ring_PeekRing(FX_RING_TYPE_KNOCKBACK, 1);
        if (ring != nullptr && ring->on != 0 && ring->speed >= 0
            && M_KnockBackCollision(ring)) {
            FX_Ring_BounceKnockBack();
        }
        if (!FX_Ring_IsRingActive(FX_RING_TYPE_KNOCKBACK)) {
            p->knockback_active = false;
        }
    }

    if (item->hit_points <= 0 && p->explode_count == 0) {
        Lara_Electricity_UpdatePoints();
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

            if (((s00->color.r | s00->color.g | s00->color.b | s01->color.r
                  | s01->color.g | s01->color.b | s11->color.r | s11->color.g
                  | s11->color.b | s10->color.r | s10->color.g | s10->color.b)
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
                { s00->color.r, s00->color.g, s00->color.b, 255 },
                { s01->color.r, s01->color.g, s01->color.b, 255 },
                { s11->color.r, s11->color.g, s11->color.b, 255 },
                { s10->color.r, s10->color.g, s10->color.b, 255 },
            };
            OutputSource_PolyFX_StageSpriteQuadWorld(
                sprite_idx, world_pos, color, DRAW_BLEND_ADD);
        }
    }
}

static bool M_Draw(const ITEM *const item)
{
    const bool result = Object_DrawAnimatingItem(item);

    const M_PRIV *const p = item->priv;
    if (p->explode_count != 0) {
        M_DrawShield(item);
    }

    if (item->hit_points <= 0 && p->explode_count == 0) {
        Lara_Electricity_Draw(0, item);
        Lara_Electricity_Draw(1, item);
    }

    return result;
}

static void M_Setup(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }

    obj->initialise_func = M_Initialise;
    obj->control_func = M_Control;
    obj->collision_func = Creature_Collision;
    obj->draw_func = M_Draw;
    obj->gun_hit_func = M_GunHit;
    obj->should_spawn_blood_func = M_ShouldSpawnBlood;

    obj->priv_size = sizeof(M_PRIV);
    obj->priv_load_func = M_LoadPriv;
    obj->priv_save_func = M_SavePriv;

    obj->lot_setup = LOT_Setup(LOT_SETUP_CLIMBER);
    obj->lot_setup.drop = -STEP_L * 3;
    obj->shadow_size = 0;
    obj->hit_points = 300;
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

REGISTER_OBJECT(O_SOPHIA, M_Setup)
