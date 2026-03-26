#include <trx/core/utils.h>
#include <trx/game/effects.h>
#include <trx/game/lara.h>
#include <trx/game/objects.h>
#include <trx/game/objects/creatures/tribe_boss.h>
#include <trx/game/pathing.h>
#include <trx/game/random.h>
#include <trx/game/sparks.h>
#include <trx/game/spawn.h>

// clang-format off
#define M_SWIPE_DAMAGE     120
#define M_BITE_DAMAGE      100
#define M_RUN_TURN         (DEG_1 * 4) // = 728
#define M_WALK_TURN        (DEG_1 * 10) // = 1820
#define M_ATTACK_0_RANGE   SQUARE(WALL_L * 5 / 2) // = 0x640000
#define M_ATTACK_1_RANGE   SQUARE(WALL_L * 3 / 4) // = 0x90000
#define M_ATTACK_2_RANGE   SQUARE(WALL_L * 3 / 2) // = 0x240000
#define M_WALK_RANGE       SQUARE(WALL_L * 2)
#define M_WALK_CHANCE      0x100
#define M_WAIT_CHANCE      0x100
#define M_BITE_TOUCH_BITS  0xC00
#define M_SWIPE_TOUCH_BITS 0x20
#define M_VAULT_SHIFT      260
// clang-format on

typedef enum {
    M_STATE_NULL,
    M_STATE_STOP,
    M_STATE_WALK,
    M_STATE_PUNCH_2,
    M_STATE_AIM_2,
    M_STATE_WAIT,
    M_STATE_AIM_1,
    M_STATE_AIM_0,
    M_STATE_PUNCH_1,
    M_STATE_PUNCH_B,
    M_STATE_RUN,
    M_STATE_DEATH,
    M_STATE_CLIMB_3,
    M_STATE_CLIMB_1,
    M_STATE_CLIMB_2,
    M_STATE_FALL_3
} M_STATE;

typedef enum {
    M_ANIM_SLIDE_1 = 23,
    M_ANIM_DEATH = 26,
    M_ANIM_CLIMB_3 = 27,
    M_ANIM_CLIMB_1 = 28,
    M_ANIM_CLIMB_2 = 29,
    M_ANIM_FALL_3 = 30,
    M_ANIM_SLIDE_2 = 31,
} M_ANIM;

static BITE m_BiteHit = {
    .pos = { .x = 0, .y = -120, .z = 120 },
    .mesh_num = 10,
};
static BITE m_SwipeHit = {
    .pos = { .x = 0, .y = 0, .z = 0 },
    .mesh_num = 5,
};
static BITE m_GasHit = {
    .pos = { .x = 0, .y = -64, .z = 56 },
    .mesh_num = 9,
};

static void M_TriggerGas(
    const XYZ_32 pos, const XYZ_32 vel, const int32_t effect_num)
{
    SPARK *const spark = Sparks_GetFreeSpark();
    spark->on = true;
    spark->src_color.r = 0;
    spark->src_color.g = (Random_GetControl() & 0x3F) + 128;
    spark->src_color.b = 32;
    spark->dst_color.r = 0;
    spark->dst_color.g = (Random_GetControl() & 0xF) + 32;
    spark->dst_color.b = 0;

    if (vel.x != 0 || vel.y != 0 || vel.z != 0) {
        spark->col_fade_speed = 6;
        spark->fade_to_black = 2;
        spark->life = (Random_GetControl() & 1) + 12;
    } else {
        spark->col_fade_speed = 8;
        spark->fade_to_black = 16;
        spark->life = (Random_GetControl() & 3) + 20;
    }

    spark->s_life = spark->life;
    spark->draw_type = DRAW_BLEND_ADD;
    spark->extras = 0;
    spark->dynamic = -1;
    spark->pos.x = pos.x + (Random_GetControl() & 0x1F) - 16;
    spark->pos.y = pos.y;
    spark->pos.z = pos.z + (Random_GetControl() & 0x1F) - 16;
    spark->vel.x = vel.x + (Random_GetControl() & 0xF) - 16;
    spark->vel.y = vel.y;
    spark->vel.z = vel.z + (Random_GetControl() & 0xF) - 16;
    spark->friction = 0;

    if (Random_GetControl() & 1) {
        if (effect_num < 0) {
            spark->flags = SPARK_F_ALT_SPRITE | SPARK_F_ROTATE | SPARK_F_SPRITE
                | SPARK_F_SCALE;
        } else {
            spark->flags = SPARK_F_ALT_SPRITE | SPARK_F_FX | SPARK_F_ROTATE
                | SPARK_F_SPRITE | SPARK_F_SCALE;
        }

        spark->rot_angle = Random_GetControl() & 0xFFF;

        if (Random_GetControl() & 1) {
            spark->rot_add = -16 - (Random_GetControl() & 0xF);
        } else {
            spark->rot_add = (Random_GetControl() & 0xF) + 16;
        }
    } else if (effect_num < 0) {
        spark->flags = SPARK_F_ALT_SPRITE | SPARK_F_SPRITE | SPARK_F_SCALE;
    } else {
        spark->flags =
            SPARK_F_ALT_SPRITE | SPARK_F_FX | SPARK_F_SPRITE | SPARK_F_SCALE;
    }

    spark->max_y_vel = 0;
    spark->effect_num = (uint8_t)effect_num;
    spark->gravity = 0;
    spark->sprite_idx = Object_Get(O_EXPLOSION_1)->mesh_idx;

    const int32_t size = (Random_GetControl() & 0x1F) + 48;
    if (vel.x != 0 || vel.y != 0 || vel.z != 0) {
        spark->size.width = size >> 5;
        spark->src_size.width = spark->size.width;
        spark->dst_size.width = size >> 1;
        spark->size.height = spark->size.width;
        spark->src_size.height = spark->size.height;
        spark->dst_size.height = spark->dst_size.width;

        if (effect_num == -2) {
            spark->scalar = 2;
        } else {
            spark->scalar = 3;
        }
    } else {
        spark->scalar = 4;
        spark->size.width = size >> 4;
        spark->src_size.width = spark->size.width;
        spark->dst_size.width = size >> 1;
        spark->size.height = spark->size.width;
        spark->src_size.height = spark->size.height;
        spark->dst_size.height = spark->dst_size.width;
    }
    Sparks_FinishSetup(spark);
}

static int16_t M_TriggerGasThrower(
    const ITEM *const item, const BITE *const bite, const int16_t speed)
{
    const int16_t effect_num = Effect_Create(item->room_num);
    if (effect_num == NO_ITEM) {
        return NO_ITEM;
    }

    XYZ_32 pos = bite->pos;
    Collide_GetJointAbsPosition(item, &pos, bite->mesh_num);

    XYZ_32 pos1 = {
        .x = bite->pos.x,
        .y = bite->pos.y << 3,
        .z = bite->pos.z << 2,
    };
    Collide_GetJointAbsPosition(item, &pos1, bite->mesh_num);

    int16_t angles[2];
    Math_GetVectorAngles(
        pos1.x - pos.x, pos1.y - pos.y, pos1.z - pos.z, angles);

    EFFECT *const effect = Effect_Get(effect_num);
    effect->pos = pos;
    effect->room_num = item->room_num;
    effect->rot.x = angles[1];
    effect->rot.z = 0;
    effect->rot.y = angles[0];
    effect->speed = speed << 2;
    effect->object_id = O_MISSILE_POISON;
    effect->counter = 20;
    M_TriggerGas((XYZ_32) {}, (XYZ_32) {}, effect_num);

    for (int32_t i = 0; i < 2; i++) {
        const int32_t s = Random_GetControl() % (speed << 2) + 32;
        const int32_t r = (s * Math_Cos(effect->rot.x)) >> W2V_SHIFT;
        XYZ_32 vel = {
            .x = (r * Math_Sin(effect->rot.y)) >> W2V_SHIFT,
            .y = -((s * Math_Sin(effect->rot.x)) >> W2V_SHIFT),
            .z = (r * Math_Cos(effect->rot.y)) >> W2V_SHIFT,
        };
        M_TriggerGas(
            effect->pos, (XYZ_32) { vel.x << 5, vel.y << 5, vel.z << 5 }, -1);
    }

    {
        const int32_t r = ((speed << 1) * Math_Cos(effect->rot.x)) >> W2V_SHIFT;
        const XYZ_32 vel = {
            .x = (r * Math_Sin(effect->rot.y)) >> W2V_SHIFT,
            .y = -(((speed << 1) * Math_Sin(effect->rot.x)) >> W2V_SHIFT),
            .z = (r * Math_Cos(effect->rot.y)) >> W2V_SHIFT,
        };
        M_TriggerGas(
            effect->pos, (XYZ_32) { vel.x << 5, vel.y << 5, vel.z << 5 }, -2);
    }

    return effect_num;
}

static void M_Control(const int16_t item_num)
{
    if (!Creature_Activate(item_num)) {
        return;
    }

    ITEM *const item = Item_Get(item_num);
    CREATURE *const creature = item->creature_data;
    int16_t tilt = 0;
    int16_t angle = 0;
    int16_t neck = 0;

    if (item->hit_points <= 0) {
        if (item->current_anim_state != M_STATE_DEATH) {
            Item_SwitchToAnim(item, M_ANIM_DEATH, 0);
            item->current_anim_state = M_STATE_DEATH;
        } else if (
            TribeBoss_IsLizardActive() && Item_GetRelativeFrame(item) == 50) {
            Creature_Die(item_num, true);
        }
    } else {
        AI_INFO info;
        Creature_AIInfo(item, &info);
        Creature_Mood(item, &info, true);

        if (Box_GetBox(creature->enemy->box_num)->overlap_index
            & BOX_BLOCKED_SEARCH) {
            creature->mood = MOOD_ATTACK;
        }

        LARA_INFO *const lara_info = Lara_GetLaraInfo();
        angle = Creature_Turn(item, creature->maximum_turn);

        switch (item->current_anim_state) {
        case M_STATE_STOP:
            creature->flags = 0;

            if (info.ahead) {
                neck = info.angle;
            }

            creature->maximum_turn = 0;
            if (creature->mood == MOOD_ESCAPE) {
                item->goal_anim_state = M_STATE_RUN;
            } else if (creature->mood == MOOD_BORED) {
                if (item->required_anim_state) {
                    item->goal_anim_state = item->required_anim_state;
                } else if (Random_GetControl() < 0x4000) {
                    item->goal_anim_state = M_STATE_WALK;
                } else {
                    item->goal_anim_state = M_STATE_WAIT;
                }
            } else if (info.bite && info.distance < M_ATTACK_1_RANGE) {
                item->goal_anim_state = M_STATE_AIM_1;
            } else if (
                Creature_CanTargetEnemy(item, &info) && info.bite
                && info.distance < M_ATTACK_0_RANGE
                && (lara_info->poison_timer < 256
                    || (Box_GetBox(creature->enemy->box_num)->overlap_index
                        & BOX_BLOCKED_SEARCH))) {
                item->goal_anim_state = M_STATE_AIM_0;
            } else {
                item->goal_anim_state = M_STATE_RUN;
            }
            break;

        case M_STATE_WALK:
            if (info.ahead) {
                neck = info.angle;
            }

            if (Item_GetRelativeAnim(item) == M_ANIM_SLIDE_1
                || Item_GetRelativeAnim(item) == M_ANIM_SLIDE_2) {
                creature->maximum_turn = 0;
            } else {
                creature->maximum_turn = M_WALK_TURN;
            }

            if (creature->mood == MOOD_ESCAPE) {
                item->goal_anim_state = M_STATE_RUN;
            } else if (creature->mood == MOOD_BORED) {
                if (Random_GetControl() < M_WAIT_CHANCE) {
                    item->required_anim_state = M_STATE_WAIT;
                    item->goal_anim_state = M_STATE_STOP;
                }
            } else if (info.bite && info.distance < M_ATTACK_1_RANGE) {
                item->goal_anim_state = M_STATE_STOP;
            } else if (info.bite && info.distance < M_ATTACK_2_RANGE) {
                item->goal_anim_state = M_STATE_AIM_2;
            } else if (
                Creature_CanTargetEnemy(item, &info)
                && info.distance < M_ATTACK_0_RANGE
                && (lara_info->poison_timer < 256
                    || (Box_GetBox(creature->enemy->box_num)->overlap_index
                        & BOX_BLOCKED_SEARCH))) {
                item->goal_anim_state = M_STATE_STOP;
            } else if (info.distance > M_WALK_RANGE) {
                item->goal_anim_state = M_STATE_RUN;
            }
            break;

        case M_STATE_PUNCH_2:
            if (info.ahead) {
                neck = info.angle;
            }
            if (creature->flags != 2 && item->touch_bits & M_BITE_TOUCH_BITS) {
                Lara_TakeDamage(M_BITE_DAMAGE, true);
                Creature_Effect(item, &m_BiteHit, Spawn_Blood);
                creature->flags = 2;
            }
            break;

        case M_STATE_AIM_2:
            if (info.ahead) {
                neck = info.angle;
            }
            creature->maximum_turn = M_WALK_TURN;
            creature->flags = 0;
            if (info.bite && info.distance < M_ATTACK_2_RANGE) {
                item->goal_anim_state = M_STATE_PUNCH_2;
            } else {
                item->goal_anim_state = M_STATE_WALK;
            }
            break;

        case M_STATE_WAIT:
            if (info.ahead) {
                neck = info.angle;
            }
            creature->maximum_turn = 0;
            if (creature->mood != MOOD_BORED) {
                item->goal_anim_state = M_STATE_STOP;
            } else if (Random_GetControl() < M_WALK_CHANCE) {
                item->required_anim_state = M_STATE_WALK;
                item->goal_anim_state = M_STATE_STOP;
            }
            break;

        case M_STATE_AIM_1:
            if (info.ahead) {
                neck = info.angle;
            }
            creature->maximum_turn = M_WALK_TURN;
            creature->flags = 0;
            if (info.ahead && info.distance < M_ATTACK_1_RANGE) {
                item->goal_anim_state = M_STATE_PUNCH_1;
            } else {
                item->goal_anim_state = M_STATE_STOP;
            }
            break;

        case M_STATE_AIM_0:
            if (info.ahead) {
                neck = info.angle;
            }
            creature->maximum_turn = 0;
            if (ABS(info.angle) < M_RUN_TURN) {
                item->rot.y += info.angle;
            } else if (info.angle < 0) {
                item->rot.y -= M_RUN_TURN;
            } else {
                item->rot.y += M_RUN_TURN;
            }

            if (info.bite && info.distance < M_ATTACK_0_RANGE
                && (lara_info->poison_timer < 256
                    || (Box_GetBox(creature->enemy->box_num)->overlap_index
                        & BOX_BLOCKED_SEARCH))) {
                item->goal_anim_state = M_STATE_PUNCH_B;
            } else {
                item->goal_anim_state = M_STATE_STOP;
            }
            break;

        case M_STATE_PUNCH_1:
            if (info.ahead) {
                neck = info.angle;
            }

            if (!creature->flags && item->touch_bits & M_SWIPE_TOUCH_BITS) {
                Lara_TakeDamage(M_SWIPE_DAMAGE, true);
                Creature_Effect(item, &m_SwipeHit, Spawn_Blood);
                creature->flags = 1;
            }

            if (info.distance < M_ATTACK_2_RANGE) {
                item->goal_anim_state = M_STATE_PUNCH_2;
            }
            break;

        case M_STATE_PUNCH_B:
            if (info.ahead) {
                neck = info.angle;
            }

            if (ABS(info.angle) < M_RUN_TURN) {
                item->rot.y += info.angle;
            } else if (info.angle < 0) {
                item->rot.y -= M_RUN_TURN;
            } else {
                item->rot.y += M_RUN_TURN;
            }

            if (Item_GetRelativeFrame(item) >= 7
                && Item_GetRelativeFrame(item) <= 28) {
                if (creature->flags < 24) {
                    creature->flags += 2;
                }

                int32_t f;
                if (creature->flags < 24) {
                    f = creature->flags;
                } else {
                    f = (Random_GetControl() & 0xF) + 8;
                }

                M_TriggerGasThrower(item, &m_GasHit, f);
            }

            if (Item_GetRelativeFrame(item) > 28) {
                creature->flags = 0;
            }
            break;

        case M_STATE_RUN:
            if (info.ahead) {
                neck = info.angle;
            }

            creature->maximum_turn = M_RUN_TURN;
            tilt = angle / 2;

            if (creature->mood != MOOD_ESCAPE) {
                if (creature->mood == MOOD_BORED) {
                    item->goal_anim_state = M_STATE_WALK;
                } else if (info.bite && info.distance < M_ATTACK_1_RANGE) {
                    item->goal_anim_state = M_STATE_STOP;
                } else if (
                    Creature_CanTargetEnemy(item, &info)
                    && info.distance < M_ATTACK_0_RANGE
                    && (lara_info->poison_timer < 256
                        || (Box_GetBox(creature->enemy->box_num)->overlap_index
                            & BOX_BLOCKED_SEARCH))) {
                    item->goal_anim_state = M_STATE_STOP;
                } else if (info.ahead && info.distance < M_WALK_RANGE) {
                    item->goal_anim_state = M_STATE_WALK;
                }
            }
            break;
        }
    }

    Creature_Tilt(item, tilt);
    Creature_Joint(item, 0, 0);
    Creature_Joint(item, 1, neck);

    if (item->current_anim_state >= M_STATE_DEATH) {
        Creature_Animate(item_num, angle, 0);
    } else {
        switch (Creature_Vault(item_num, angle, 2, M_VAULT_SHIFT)) {
        case -4:
            creature->maximum_turn = 0;
            Item_SwitchToAnim(item, M_ANIM_FALL_3, 0);
            item->current_anim_state = M_STATE_FALL_3;
            break;

        case 2:
            creature->maximum_turn = 0;
            Item_SwitchToAnim(item, M_ANIM_CLIMB_1, 0);
            item->current_anim_state = M_STATE_CLIMB_1;
            break;

        case 3:
            creature->maximum_turn = 0;
            Item_SwitchToAnim(item, M_ANIM_CLIMB_2, 0);
            item->current_anim_state = M_STATE_CLIMB_2;
            break;

        case 4:
            creature->maximum_turn = 0;
            Item_SwitchToAnim(item, M_ANIM_CLIMB_3, 0);
            item->current_anim_state = M_STATE_CLIMB_3;
            break;
        }
    }
}

static void M_Setup(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }

    obj->control_func = M_Control;
    obj->collision_func = Creature_Collision;

    obj->shadow_size = UNIT_SHADOW / 2;
    obj->hit_points = 36;
    obj->radius = 204;
    obj->pivot_length = 0;
    obj->lot_setup = LOT_Setup(LOT_SETUP_CLIMBER);

    obj->intelligent = true;
    obj->save_position = true;
    obj->save_hitpoints = true;
    obj->save_flags = true;
    obj->save_anim = true;

    Object_GetBone(obj, 1)->rot.z = true;
    Object_GetBone(obj, 9)->rot.z = true;
}

REGISTER_OBJECT(O_LIZARD, M_Setup)
