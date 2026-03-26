#include <trx/core/json/util/read_io.h>
#include <trx/core/json/util/write_io.h>
#include <trx/core/utils.h>
#include <trx/game/creature.h>
#include <trx/game/lara.h>
#include <trx/game/objects/common.h>
#include <trx/game/objects/draw.h>
#include <trx/game/pathing.h>
#include <trx/game/random.h>
#include <trx/game/rooms.h>
#include <trx/game/shell.h>
#include <trx/game/sound.h>
#include <trx/game/sparks.h>
#include <trx/game/spawn.h>

// clang-format off
#define M_WALK_TURN (4 * DEG_1)
#define M_PINCER_RANGE SQUARE(WALL_L * 5 / 4)
#define M_CHOPPER_RANGE SQUARE(WALL_L * 4 / 3)
// clang-format on

static BITE m_RightBlade = { .pos = { 0, 0, 920 }, .mesh_num = 22 };
static BITE m_LeftBlade = { .pos = { 0, 0, 920 }, .mesh_num = 13 };

typedef enum {
    M_STATE_WAIT,
    M_STATE_WALK,
    M_STATE_WAIT_DEF,
    M_STATE_WALK_DEF,
    M_STATE_START,
    M_STATE_PINCER,
    M_STATE_KILL,
    M_STATE_CHOPPER,
    M_STATE_WALK_BACK,
    M_STATE_DEATH
} M_STATE;

typedef enum {
    M_ANIM_DEATH = 22,
    M_ANIM_START_ANIM = 14,
    M_ANIM_KILL_ANIM = 18,
} M_ANIM;

typedef struct {
    int32_t effect_mesh;
} M_PRIV;

static bool M_ShouldSpawnBlood(const ITEM *const item)
{
    if (item->current_anim_state != M_STATE_WAIT_DEF
        && item->current_anim_state != M_STATE_WALK_DEF
        && item->current_anim_state != M_STATE_START) {
        return true;
    }

    const ITEM *const lara_item = Lara_GetItem();
    const int32_t dx = item->pos.x - lara_item->pos.x;
    const int32_t dz = item->pos.z - lara_item->pos.z;
    const int16_t angle = DEG_180 - item->rot.y + Math_Atan(dz, dx);
    if (angle <= -DEG_90 || angle >= DEG_90) {
        return true;
    }

    const LARA_INFO *const lara = Lara_GetLaraInfo();
    // XXX: This uses Lara's currently equipped gun. If she swaps weapons before
    // a projectile impact resolves, this can differ from the projectile weapon.
    if (lara->gun_type == LGT_ROCKET || lara->gun_type == LGT_GRENADE
        || lara->gun_type == LGT_HARPOON) {
        return true;
    }

    return false;
}

static void M_TriggerSmoke(const XYZ_32 pos, const bool uw)
{
    const ITEM *const lara_item = Lara_GetItem();
    const int32_t dx = lara_item->pos.x - pos.x;
    const int32_t dz = lara_item->pos.z - pos.z;

    if (dx < -0x4000 || dx > 0x4000 || dz < -0x4000 || dz > 0x4000) {
        return;
    }

    SPARK *const spark = Sparks_GetFreeSpark();
    spark->on = true;

    if (uw) {
        spark->src_color.r = 0;
        spark->src_color.g = 0;
        spark->src_color.b = 0;
        spark->dst_color.r = 192;
        spark->dst_color.g = 192;
        spark->dst_color.b = 208;
    } else {
        spark->src_color.r = 144;
        spark->src_color.g = 144;
        spark->src_color.b = 144;
        spark->dst_color.r = 64;
        spark->dst_color.g = 64;
        spark->dst_color.b = 64;
    }

    spark->col_fade_speed = 8;
    spark->fade_to_black = 64;
    spark->life = (Random_GetControl() & 0x1F) + 96;
    spark->s_life = spark->life;

    if (uw) {
        spark->draw_type = DRAW_BLEND_ADD;
    } else {
        spark->draw_type = DRAW_BLEND_SUB;
    }

    spark->extras = 0;
    spark->dynamic = -1;
    spark->pos.x = pos.x + (Random_GetControl() & 0x1F) - 16;
    spark->pos.y = pos.y + (Random_GetControl() & 0x1F) - 16;
    spark->pos.z = pos.z + (Random_GetControl() & 0x1F) - 16;
    spark->vel.x = ((Random_GetControl() & 0xFFF) - 2048) >> 2;
    spark->vel.y = (Random_GetControl() & 0xFF) - 128;
    spark->vel.z = ((Random_GetControl() & 0xFFF) - 2048) >> 2;

    if (uw) {
        spark->friction = 20;
        spark->pos.y += 32;
        spark->vel.y >>= 4;
    } else {
        spark->friction = 6;
    }

    spark->flags =
        SPARK_F_ALT_SPRITE | SPARK_F_ROTATE | SPARK_F_SPRITE | SPARK_F_SCALE;
    spark->rot_angle = Random_GetControl() & 0xFFF;

    if (Random_GetControl() & 1) {
        spark->rot_add = -16 - (Random_GetControl() & 0xF);
    } else {
        spark->rot_add = (Random_GetControl() & 0xF) + 16;
    }

    spark->scalar = 3;
    spark->sprite_idx = Object_Get(O_EXPLOSION_1)->mesh_idx;

    if (uw) {
        spark->max_y_vel = 0;
        spark->gravity = 0;
    } else {
        spark->gravity = -3 - (Random_GetControl() & 3);
        spark->max_y_vel = -4 - (Random_GetControl() & 3);
    }

    spark->dst_size.width = (Random_GetControl() & 0x1F) + 128;
    spark->size.width = spark->dst_size.width >> 2;
    spark->src_size.width = spark->size.width >> 2;

    spark->dst_size.height =
        spark->dst_size.width + (Random_GetControl() & 0x1F) + 32;
    spark->size.height = spark->dst_size.height >> 3;
    spark->src_size.height = spark->size.height;
    Sparks_FinishSetup(spark);
}

static void M_Damage(
    ITEM *const item, CREATURE *const creature, const int32_t damage)
{
    if (creature->flags == 0 && item->touch_bits & 0x2400000) {
        Lara_TakeDamage(damage, true);
        Creature_Effect(item, &m_RightBlade, Spawn_Blood);
        creature->flags = 1;
        Sound_Effect(SFX_MACAQUE_ROLL, &item->pos, SPM_NORMAL);
    }

    if (creature->flags == 0 && item->touch_bits & 0x2400) {
        Lara_TakeDamage(damage, true);
        Creature_Effect(item, &m_LeftBlade, Spawn_Blood);
        creature->flags = 1;
        Sound_Effect(SFX_MACAQUE_ROLL, &item->pos, SPM_NORMAL);
    }
}

static void M_LoadPriv(ITEM *const item, JSON_READ_IO *const io)
{
    M_PRIV *const p = item->priv;
    JSON_SHOULD(JSON_READ(io, "effect_mesh", &p->effect_mesh));
}

static void M_SavePriv(const ITEM *const item, JSON_WRITE_IO *const io)
{
    const M_PRIV *const p = item->priv;
    JSONW_WRITE(io, "effect_mesh", p->effect_mesh);
}

static void M_Initialise(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;

    if (item->object_id == O_SHIVA) {
        Item_SwitchToAnim(item, M_ANIM_START_ANIM, 0);
        item->current_anim_state = M_STATE_START;
        item->goal_anim_state = M_STATE_START;
    }

    item->status = IS_INACTIVE;
    item->mesh_bits = 0;
    p->effect_mesh = 0;
}

static void M_Control(const int16_t item_num)
{
    if (!Creature_Activate(item_num)) {
        return;
    }

    const ITEM *const lara_item = Lara_GetItem();
    ITEM *const item = Item_Get(item_num);
    CREATURE *const creature = item->creature_data;
    M_PRIV *const p = item->priv;

    const bool lara_alive = lara_item->hit_points > 0;
    int16_t torso_x = 0;
    int16_t torso_y = 0;
    int16_t head_y = 0;
    int16_t angle = 0;

    if (item->hit_points <= 0) {
        if (item->current_anim_state != M_STATE_DEATH) {
            Item_SwitchToAnim(item, M_ANIM_DEATH, 0);
            item->current_anim_state = M_STATE_DEATH;
        }
    } else {
        AI_INFO info;
        Creature_AIInfo(item, &info);
        Creature_Mood(item, &info, true);

        if (creature->mood == MOOD_ESCAPE) {
            creature->target.x = lara_item->pos.x;
            creature->target.z = lara_item->pos.z;
        }

        angle = Creature_Turn(item, creature->maximum_turn);

        if (item->current_anim_state != M_STATE_START) {
            item->mesh_bits = INT32_MAX;
        }

        switch (item->current_anim_state) {
        case M_STATE_WAIT:
            if (info.ahead) {
                head_y = info.angle;
            }

            if (creature->flags < 0) {
                creature->flags++;
                const XYZ_32 smoke_pos = {
                    .x = item->pos.x + (Random_GetControl() & 0x5FF) - 768,
                    .y = item->pos.y - (Random_GetControl() & 0x5FF),
                    .z = item->pos.z + (Random_GetControl() & 0x5FF) - 768,
                };
                M_TriggerSmoke(smoke_pos, true);
            } else {
                if (creature->flags == 1) {
                    creature->flags = 0;
                }

                creature->maximum_turn = 0;

                if (creature->mood == MOOD_ESCAPE) {
                    int16_t room_num = item->room_num;
                    const XYZ_32 offset = XYZ_32_OffsetYaw(
                        item->pos, item->rot.y + DEG_180, WALL_L);
                    const SECTOR *const sector =
                        Room_GetSector(offset, &room_num);

                    if (creature->flags != 0 || sector->box == 0x7FF
                        || Box_GetBox(sector->box)->overlap_index
                            & BOX_BLOCKABLE) {
                        item->goal_anim_state = M_STATE_WAIT_DEF;
                    } else {
                        item->goal_anim_state = M_STATE_WALK_BACK;
                    }
                } else if (creature->mood == MOOD_BORED) {
                    if (Random_GetControl() < 1024) {
                        item->goal_anim_state = M_STATE_WALK;
                    }
                } else if (info.bite && info.distance < M_PINCER_RANGE) {
                    item->goal_anim_state = M_STATE_PINCER;
                    creature->flags = 0;
                } else if (info.bite && info.distance < M_CHOPPER_RANGE) {
                    item->goal_anim_state = M_STATE_CHOPPER;
                    creature->flags = 0;
                } else if (item->hit_status && info.ahead) {
                    item->goal_anim_state = M_STATE_WAIT_DEF;
                    creature->flags = 4;
                } else {
                    item->goal_anim_state = M_STATE_WALK;
                }
            }
            break;

        case M_STATE_WALK:
            if (info.ahead) {
                head_y = info.angle;
            }

            creature->maximum_turn = M_WALK_TURN;

            if (creature->mood == MOOD_ESCAPE) {
                item->goal_anim_state = M_STATE_WAIT;
            } else if (creature->mood == MOOD_BORED) {
                item->goal_anim_state = M_STATE_WAIT;
            } else if (info.bite && info.distance < M_CHOPPER_RANGE) {
                item->goal_anim_state = M_STATE_WAIT;
                creature->flags = 0;
            } else if (item->hit_status) {
                item->goal_anim_state = M_STATE_WALK_DEF;
                creature->flags = 4;
            }
            break;

        case M_STATE_WAIT_DEF:
            if (info.ahead) {
                head_y = info.angle;
            }

            creature->maximum_turn = 0;
            if (item->hit_status || creature->mood == MOOD_ESCAPE) {
                creature->flags = 4;
            }

            if ((info.bite && info.distance < M_CHOPPER_RANGE)
                || (Item_GetRelativeFrame(item) == 0 && creature->flags == 0)
                || !info.ahead) {
                item->goal_anim_state = M_STATE_WAIT;
                creature->flags = 0;
            } else if (creature->flags != 0) {
                item->goal_anim_state = M_STATE_WAIT_DEF;
            }

            if (Item_GetRelativeFrame(item) == 0 && creature->flags > 1) {
                creature->flags -= 2;
            }
            break;

        case M_STATE_WALK_DEF:
            if (info.ahead) {
                head_y = info.angle;
            }

            creature->maximum_turn = M_WALK_TURN;
            if (item->hit_status) {
                creature->flags = 4;
            }

            if ((info.bite && info.distance < M_PINCER_RANGE)
                || (Item_GetRelativeFrame(item) == 0 && creature->flags == 0)) {
                item->goal_anim_state = M_STATE_WALK;
                creature->flags = 0;
            } else if (creature->flags != 0) {
                item->goal_anim_state = M_STATE_WALK_DEF;
            }

            if (Item_GetRelativeFrame(item) == 0) {
                creature->flags = 0;
            }
            break;

        case M_STATE_START:
            creature->maximum_turn = 0;

            if (creature->flags != 0) {
                creature->flags--;
            } else {
                if (item->mesh_bits == 0) {
                    p->effect_mesh = 0;
                }

                item->mesh_bits <<= 1;
                item->mesh_bits |= 1;
                creature->flags = 1;
                XYZ_32 smoke_pos = { 0, 0, 256 };
                Collide_GetJointAbsPosition(item, &smoke_pos, p->effect_mesh++);
                Sparks_TriggerExplosionSparks(
                    smoke_pos, 2, 0, 0, item->room_num);
                M_TriggerSmoke(smoke_pos, true);
            }

            if (item->mesh_bits == INT32_MAX) {
                item->goal_anim_state = M_STATE_WAIT;
                p->effect_mesh = 0;
                creature->flags = -45;
            }
            break;

        case M_STATE_PINCER:
            if (info.ahead) {
                torso_y = info.angle;
                torso_x = info.x_angle;
                head_y = info.angle;
            }
            creature->maximum_turn = M_WALK_TURN;
            M_Damage(item, creature, 150);
            break;

        case M_STATE_KILL: {
            creature->maximum_turn = 0;
            head_y = 0;
            torso_x = 0;
            torso_y = 0;
            const int32_t frame = Item_GetRelativeFrame(item);
            if (frame == 10 || frame == 21 || frame == 33) {
                Creature_Effect(item, &m_RightBlade, Spawn_Blood);
                Creature_Effect(item, &m_LeftBlade, Spawn_Blood);
            }
            break;
        }

        case M_STATE_CHOPPER:
            head_y = info.angle;
            torso_y = info.angle;
            if (info.x_angle > 0) {
                torso_x = info.x_angle;
            }
            creature->maximum_turn = M_WALK_TURN;
            M_Damage(item, creature, 180);
            break;

        case M_STATE_WALK_BACK:
            if (info.ahead) {
                head_y = info.angle;
            }
            creature->maximum_turn = M_WALK_TURN;

            if ((info.ahead && info.distance < M_CHOPPER_RANGE)
                || (Item_GetRelativeFrame(item) == 0 && creature->flags == 0)) {
                item->goal_anim_state = M_STATE_WAIT;
            } else if (item->hit_status) {
                creature->flags = 4;
                item->goal_anim_state = M_STATE_WAIT;
            }
            break;
        }
    }

    if (lara_alive && lara_item->hit_points <= 0) {
        Creature_SpecialKill(
            item, M_ANIM_KILL_ANIM, M_STATE_KILL, LS_EXTRA_SHIVA_KILL);
        return;
    }

    Creature_Tilt(item, 0);
    Creature_Joint(item, 0, torso_y);
    Creature_Joint(item, 1, torso_x);
    Creature_Joint(item, 2, head_y - torso_y);
    Creature_Joint(item, 3, 0);
    Creature_Animate(item_num, angle, 0);
}

bool M_Draw(const ITEM *const item)
{
    M_PRIV *const p = item->priv;

    if (item->hit_points <= 0 && item->status != IS_ACTIVE
        && item->mesh_bits != 0) {
        ITEM *const mutable_item = (ITEM *)item;
        mutable_item->mesh_bits >>= 1;
        XYZ_32 smoke_pos = { 0, 0, 256 };
        Collide_GetJointAbsPosition(item, &smoke_pos, p->effect_mesh++);
        M_TriggerSmoke(smoke_pos, true);
    }

    const OBJECT *const swap = Object_Get(O_MESH_SWAP_1);
    return Object_DrawAnimatingItemWithSwap(item, swap);
}

static void M_Setup(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }

    if (!Object_Get(O_MESH_SWAP_1)->loaded) {
        Shell_ExitSystem("Shiva requires O_MESH_SWAP_1 (statue)");
    }

    obj->initialise_func = M_Initialise;
    obj->control_func = M_Control;
    obj->collision_func = Creature_Collision;
    obj->should_spawn_blood_func = M_ShouldSpawnBlood;
    obj->draw_func = M_Draw;
    obj->priv_size = sizeof(M_PRIV);
    obj->priv_load_func = M_LoadPriv;
    obj->priv_save_func = M_SavePriv;

    obj->shadow_size = UNIT_SHADOW / 2;
    obj->hit_points = 100;
    obj->radius = 341;
    obj->pivot_length = 0;

    obj->intelligent = true;
    obj->save_position = true;
    obj->save_hitpoints = true;
    obj->save_flags = true;
    obj->save_anim = true;

    Object_GetBone(obj, 6)->rot.x = true;
    Object_GetBone(obj, 6)->rot.y = true;
    Object_GetBone(obj, 25)->rot.x = true;
    Object_GetBone(obj, 25)->rot.y = true;
}

REGISTER_OBJECT(O_SHIVA, M_Setup)
