#include <trx/core/json/util/read_io.h>
#include <trx/core/json/util/write_io.h>
#include <trx/game/creature.h>
#include <trx/game/lara.h>
#include <trx/game/objects.h>
#include <trx/game/output.h>
#include <trx/game/pathing.h>
#include <trx/game/random.h>
#include <trx/game/rooms.h>
#include <trx/game/sound.h>
#include <trx/game/sparks.h>
#include <trx/game/spawn.h>

// clang-format off
#define M_RADIUS         (WALL_L / 10)          // = 102
#define M_HITPOINTS      20
#define M_TOUCH_BITS     0b0100100'00000000
#define M_IGNITE_COUNT   3
#define M_SWIPE_DAMAGE   100
#define M_HIT_DAMAGE     80
#define M_MAX_FIRE_DIST  (WALL_L * 16)          // = 16384
#define M_ALERT_DIST     SQUARE(WALL_L)         // = 1048576
#define M_ALERT_HEIGHT   (STEP_L * 5)           // = 1280
#define M_WALK_DIST      SQUARE(WALL_L)         // = 1048576
#define M_RUN_DIST       SQUARE(WALL_L * 2)     // = 4194304
#define M_ATTACK_RANGE_1 SQUARE(WALL_L / 2)     // = 262144
#define M_ATTACK_RANGE_2 SQUARE(WALL_L)         // = 1048576
#define M_ATTACK_RANGE_3 SQUARE(WALL_L * 5 / 4) // = 1638400
#define M_WALK_TURN      (DEG_1 * 5)            // = 910
#define M_RUN_TURN       (DEG_1 * 6)            // = 1092
#define M_WAIT_CHANCE    0x100
// clang-format on

typedef enum {
    M_STATE_NULL,
    M_STATE_STOP,
    M_STATE_WALK,
    M_STATE_PUNCH_3,
    M_STATE_AIM_3,
    M_STATE_WAIT,
    M_STATE_AIM_2,
    M_STATE_AIM_1,
    M_STATE_PUNCH_2,
    M_STATE_PUNCH_1,
    M_STATE_RUN,
    M_STATE_DEATH,
    M_STATE_UP_2,
    M_STATE_UP_3,
    M_STATE_UP_4,
    M_STATE_DOWN_4,
} M_STATE;

typedef enum {
    // clang-format off
    M_ANIM_STAND  = 6,
    M_ANIM_DEATH  = 26,
    M_ANIM_UP_4   = 27,
    M_ANIM_UP_2   = 28,
    M_ANIM_UP_3   = 29,
    M_ANIM_DOWN_4 = 30,
    // clang-format on
} M_ANIM;

typedef struct {
    struct {
        bool initialised;
        bool on_fire;
        uint8_t hit_count;
    } stick;
} M_PRIV;

static const BITE m_Bite = {
    .pos = { 16, 48, 320 },
    .mesh_num = 13,
};

static void M_LoadPriv(ITEM *const item, JSON_READ_IO *const io)
{
    M_PRIV *const p = item->priv;
    JSON_SHOULD(JSON_READ(io, "stick_initialised", &p->stick.initialised));
    JSON_SHOULD(JSON_READ(io, "stick_on_fire", &p->stick.on_fire));
    JSON_SHOULD(JSON_READ(io, "stick_hit_count", &p->stick.hit_count));
}

static void M_SavePriv(const ITEM *const item, JSON_WRITE_IO *const io)
{
    const M_PRIV *const p = item->priv;
    JSONW_WRITE(io, "stick_initialised", p->stick.initialised);
    JSONW_WRITE(io, "stick_on_fire", p->stick.on_fire);
    JSONW_WRITE(io, "stick_hit_count", p->stick.hit_count);
}

static void M_Initialise(const int16_t item_num)
{
    Creature_Initialise(item_num);
    ITEM *const item = Item_Get(item_num);
    Item_SwitchToAnim(item, M_ANIM_STAND, 0);
    item->current_anim_state = M_STATE_STOP;
    item->goal_anim_state = M_STATE_STOP;
}

static void M_InitialiseStick(ITEM *const punk_item)
{
    M_PRIV *const p = punk_item->priv;
    const int16_t fire_item_idx = Item_FindTypeAtPos(
        punk_item->room_num, punk_item->pos, O_FLAME_EMITTER_BIG);
    if (fire_item_idx != NO_ITEM) {
        ITEM *const fire_item = Item_Get(fire_item_idx);
        Item_Kill(fire_item_idx);
        fire_item->room_num = NO_ROOM;
        p->stick.on_fire = true;
    }

    p->stick.initialised = true;
}

static void M_TriggerFireSparks(const ITEM *const item)
{
    const ITEM *const lara_item = Lara_GetItem();
    const XZ_32 delta = {
        .x = lara_item->pos.x - item->pos.x,
        .z = lara_item->pos.z - item->pos.z,
    };
    if (ABS(delta.x) > M_MAX_FIRE_DIST || ABS(delta.z) > M_MAX_FIRE_DIST) {
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

    if ((Random_GetControl() & 1) != 0) {
        spark->gravity = -16 - (Random_GetControl() & 0x1F);
        spark->flags = SPARK_F_ATTACHED_NODE | SPARK_F_ALT_SPRITE | SPARK_F_ITEM
            | SPARK_F_ROTATE | SPARK_F_SPRITE | SPARK_F_SCALE;
        spark->max_y_vel = -16 - (Random_GetControl() & 7);
        spark->rot_angle = Random_GetControl() & 0xFFF;

        if (Random_GetControl() & 1) {
            spark->rot_add = -16 - (Random_GetControl() & 0xF);
        } else {
            spark->rot_add = (Random_GetControl() & 0xF) + 16;
        }
    } else {
        spark->flags = SPARK_F_ATTACHED_NODE | SPARK_F_ALT_SPRITE | SPARK_F_ITEM
            | SPARK_F_SPRITE | SPARK_F_SCALE;
        spark->gravity = -16 - (Random_GetControl() & 0x1F);
        spark->max_y_vel = -16 - (Random_GetControl() & 7);
    }

    spark->node_num = 2;
    spark->item_num = Item_GetIndex(item);
    spark->sprite_idx = Object_Get(O_EXPLOSION_1)->mesh_idx;
    spark->scalar = 1;
    uint8_t size = (Random_GetControl() & 0x1F) + 64;
    spark->src_size.width = size;
    spark->size.width = size;
    spark->src_size.height = size;
    spark->size.height = size;
    size >>= 2;
    spark->dst_size.width = size;
    spark->dst_size.height = size;
    Sparks_FinishSetup(spark);
}

static void M_TriggerFireLight(const ITEM *const item)
{
    const int32_t rnd = Random_GetControl();
    XYZ_32 pos = {
        .x = (rnd & 0xF) + m_Bite.pos.x - 8,
        .y = ((rnd >> 4) & 0xF) + m_Bite.pos.y - 8,
        .z = ((rnd >> 8) & 0xF) + m_Bite.pos.z - 8,
    };
    Collide_GetJointAbsPosition(item, &pos, m_Bite.mesh_num);
    const RGB_888 color = {
        .r = 255 - ((rnd >> 4) & 0x1F),
        .g = 192 - ((rnd >> 6) & 0x1F),
        .b = rnd & 0x3F,
    };
    Output_AddDynamicLightRGB(pos, 13, color);
}

static void M_HitLara(ITEM *const item, const int16_t damage)
{
    Lara_TakeDamage(damage, true);
    Creature_Effect(item, &m_Bite, Spawn_Blood);
    Sound_Effect(SFX_LARA_THUD, &item->pos, SPM_NORMAL);

    M_PRIV *const p = item->priv;
    p->stick.hit_count++;
    CLAMPG(p->stick.hit_count, M_IGNITE_COUNT);
    if (p->stick.on_fire && p->stick.hit_count == M_IGNITE_COUNT) {
        Lara_CatchFire();
    }
}

static bool M_Vault(ITEM *const item, const int16_t angle)
{
    const int32_t vault_result =
        Creature_Vault(Item_GetIndex(item), angle, 2, 260);
    switch (vault_result) {
    case -4:
        Item_SwitchToAnim(item, M_ANIM_DOWN_4, 0);
        item->current_anim_state = M_STATE_DOWN_4;
        return true;
    case 2:
        Item_SwitchToAnim(item, M_ANIM_UP_2, 0);
        item->current_anim_state = M_STATE_UP_2;
        return true;
    case 3:
        Item_SwitchToAnim(item, M_ANIM_UP_3, 0);
        item->current_anim_state = M_STATE_UP_3;
        return true;
    case 4:
        Item_SwitchToAnim(item, M_ANIM_UP_4, 0);
        item->current_anim_state = M_STATE_UP_4;
        return true;
    default:
        return false;
    }
}

static void M_Control(const int16_t item_num)
{
    if (!Creature_Activate(item_num)) {
        return;
    }

    ITEM *const item = Item_Get(item_num);
    CREATURE *const creature = item->creature_data;
    M_PRIV *const p = item->priv;

    if (!p->stick.initialised) {
        M_InitialiseStick(item);
    }

    if (p->stick.on_fire) {
        M_TriggerFireSparks(item);
        M_TriggerFireLight(item);
    }

    int16_t angle = 0;
    int16_t head = 0;
    int16_t tilt = 0;
    int16_t torso_x = 0;
    int16_t torso_y = 0;

    if (item->hit_points <= 0) {
        if (item->current_anim_state != M_STATE_DEATH) {
            Item_SwitchToAnim(item, M_ANIM_DEATH, 0);
            item->current_anim_state = M_STATE_DEATH;
            item->goal_anim_state = M_STATE_DEATH;
            creature->lot.setup.step = STEP_L;
        }
        goto finish;
    }

    ITEM *const lara_item = Lara_GetItem();
    const LARA_INFO *const lara = Lara_GetLaraInfo();
    if (item->ai_bits != 0) {
        Creature_GetAITarget(creature);
    } else {
        creature->enemy = lara_item;
    }

    AI_INFO info = {};
    Creature_AIInfo(item, &info);

    AI_INFO lara_info = {};
    if (creature->enemy == lara_item) {
        lara_info.distance = info.distance;
        lara_info.angle = info.angle;
    } else {
        const int32_t dx = lara_item->pos.x - item->pos.x;
        const int32_t dz = lara_item->pos.z - item->pos.z;
        lara_info.angle = Math_Atan(dz, dx) - item->rot.y;
        lara_info.distance = XYZ_32_GetLength2((XYZ_32) { dx, 0, dz });
    }

    if (!creature->alerted && creature->enemy == lara_item) {
        creature->enemy = nullptr;
    }

    Creature_Mood(item, &info, true);
    angle = Creature_Turn(item, creature->maximum_turn);

    ITEM *const enemy = creature->enemy;
    creature->enemy = lara_item;
    if (item->hit_status
        || ((lara_info.distance < M_ALERT_DIST
             || Creature_CanSeeEnemy(item, &lara_info))
            && ABS(lara_item->pos.y - item->pos.y) < M_ALERT_HEIGHT
            && Creature_IsHostile(item) && (item->ai_bits & AI_FOLLOW) == 0)) {
        if (!creature->alerted) {
            Sound_Effect(SFX_ENGLISH_HOY, &item->pos, SPM_NORMAL);
        }
        Creature_AlertAllGuards(item_num);
    }
    creature->enemy = enemy;

    switch (item->current_anim_state) {
    case M_STATE_STOP:
    case M_STATE_WAIT:
        if (item->current_anim_state == M_STATE_WAIT
            && (creature->alerted || item->goal_anim_state == M_STATE_RUN)) {
            item->goal_anim_state = M_STATE_STOP;
            break;
        }

        creature->flags = 0;
        creature->maximum_turn = 0;
        head = lara_info.angle;

        if ((item->ai_bits & AI_GUARD) != 0) {
            head = Creature_AIGuard(creature);
            if ((Random_GetControl() & 0xFF) == 0) {
                if (item->current_anim_state == M_STATE_STOP) {
                    item->goal_anim_state = M_STATE_WAIT;
                } else {
                    item->goal_anim_state = M_STATE_STOP;
                }
            }
        } else if ((item->ai_bits & AI_PATROL_1) != 0) {
            item->goal_anim_state = M_STATE_WALK;
        } else if (creature->mood == MOOD_ESCAPE) {
            if (lara->target != item && info.ahead && !item->hit_status) {
                item->goal_anim_state = M_STATE_STOP;
            } else {
                item->goal_anim_state = M_STATE_RUN;
            }
        } else if (
            creature->mood == MOOD_BORED
            || ((item->ai_bits & AI_FOLLOW) != 0
                && (creature->reached_goal
                    || lara_info.distance > M_RUN_DIST))) {
            if (item->required_anim_state != M_STATE_NULL) {
                item->goal_anim_state = item->required_anim_state;
            } else if (info.ahead) {
                item->goal_anim_state = M_STATE_STOP;
            } else {
                item->goal_anim_state = M_STATE_RUN;
            }
        } else if (info.bite && info.distance < M_ATTACK_RANGE_1) {
            item->goal_anim_state = M_STATE_AIM_1;
        } else if (info.bite && info.distance < M_ATTACK_RANGE_2) {
            item->goal_anim_state = M_STATE_AIM_2;
        } else if (info.bite && info.distance < M_WALK_DIST) {
            item->goal_anim_state = M_STATE_WALK;
        } else {
            item->goal_anim_state = M_STATE_RUN;
        }
        break;

    case M_STATE_WALK:
        creature->maximum_turn = M_WALK_TURN;
        head = lara_info.angle;

        if ((item->ai_bits & AI_PATROL_1) != 0) {
            item->goal_anim_state = M_STATE_WALK;
            head = 0;
        } else if (creature->mood == MOOD_ESCAPE) {
            item->goal_anim_state = M_STATE_RUN;
        } else if (creature->mood == MOOD_BORED) {
            if (Random_GetControl() < M_WAIT_CHANCE) {
                item->required_anim_state = M_STATE_WAIT;
                item->goal_anim_state = M_STATE_STOP;
            }
        } else if (info.bite && info.distance < M_ATTACK_RANGE_1) {
            item->goal_anim_state = M_STATE_STOP;
        } else if (info.bite && info.distance < M_ATTACK_RANGE_3) {
            item->goal_anim_state = M_STATE_AIM_3;
        } else {
            item->goal_anim_state = M_STATE_RUN;
        }
        break;

    case M_STATE_RUN:
        if (info.ahead) {
            head = info.angle;
        }

        creature->maximum_turn = M_RUN_TURN;
        tilt = angle / 2;

        if ((item->ai_bits & AI_GUARD) != 0) {
            item->goal_anim_state = M_STATE_WAIT;
        } else if (creature->mood == MOOD_ESCAPE) {
            if (lara->target != item && info.ahead) {
                item->goal_anim_state = M_STATE_STOP;
            }
        } else if (
            (item->ai_bits & AI_FOLLOW) != 0
            && (creature->reached_goal || lara_info.distance > M_RUN_DIST)) {
            item->goal_anim_state = M_STATE_STOP;
        } else if (creature->mood == MOOD_BORED) {
            item->goal_anim_state = M_STATE_WALK;
        } else if (info.ahead && info.distance < M_WALK_DIST) {
            item->goal_anim_state = M_STATE_WALK;
        }
        break;

    case M_STATE_AIM_1:
        if (info.ahead) {
            torso_y = info.angle;
            torso_x = info.x_angle;
        }

        creature->maximum_turn = M_WALK_TURN;
        creature->flags = 0;
        if (info.bite && info.distance < M_ATTACK_RANGE_1) {
            item->goal_anim_state = M_STATE_PUNCH_1;
        } else {
            item->goal_anim_state = M_STATE_STOP;
        }
        break;

    case M_STATE_AIM_2:
        if (info.ahead) {
            torso_y = info.angle;
            torso_x = info.x_angle;
        }

        creature->maximum_turn = M_WALK_TURN;
        creature->flags = 0;
        if (info.ahead && info.distance < M_ATTACK_RANGE_2) {
            item->goal_anim_state = M_STATE_PUNCH_2;
        } else {
            item->goal_anim_state = M_STATE_STOP;
        }
        break;

    case M_STATE_AIM_3:
        if (info.ahead) {
            torso_y = info.angle;
            torso_x = info.x_angle;
        }

        creature->maximum_turn = M_WALK_TURN;
        creature->flags = 0;
        if (info.bite && info.distance < M_ATTACK_RANGE_3) {
            item->goal_anim_state = M_STATE_PUNCH_3;
        } else {
            item->goal_anim_state = M_STATE_WALK;
        }
        break;

    case M_STATE_PUNCH_1:
    case M_STATE_PUNCH_2:
        if (info.ahead) {
            torso_y = info.angle;
            torso_x = info.x_angle;
        }

        creature->maximum_turn = M_WALK_TURN;
        if (creature->flags == 0 && (item->touch_bits & M_TOUCH_BITS) != 0) {
            M_HitLara(item, M_HIT_DAMAGE);
            creature->flags = 1;
        }

        if (item->current_anim_state == M_STATE_PUNCH_2 && info.ahead
            && info.distance > M_ATTACK_RANGE_2
            && info.distance < M_ATTACK_RANGE_3) {
            item->goal_anim_state = M_STATE_PUNCH_3;
        }
        break;

    case M_STATE_PUNCH_3:
        if (info.ahead) {
            torso_y = info.angle;
            torso_x = info.x_angle;
        }

        creature->maximum_turn = M_WALK_TURN;
        if (creature->flags != 2 && (item->touch_bits & M_TOUCH_BITS) != 0) {
            M_HitLara(item, M_SWIPE_DAMAGE);
            creature->flags = 2;
        }
        break;
    }

finish:
    Creature_Tilt(item, tilt);
    Creature_Joint(item, 0, torso_y);
    Creature_Joint(item, 1, torso_x);
    Creature_Joint(item, 2, head);

    if (item->current_anim_state >= M_STATE_DEATH) {
        creature->maximum_turn = 0;
        Creature_Animate(item_num, angle, 0);
    } else if (M_Vault(item, angle)) {
        creature->maximum_turn = 0;
    }
}

static void M_Setup(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }

    obj->initialise_func = M_Initialise;
    obj->collision_func = Creature_Collision;
    obj->control_func = M_Control;

    obj->priv_size = sizeof(M_PRIV);
    obj->priv_load_func = M_LoadPriv;
    obj->priv_save_func = M_SavePriv;

    obj->lot_setup = LOT_Setup(LOT_SETUP_CLIMBER);
    obj->radius = M_RADIUS;
    obj->shadow_size = UNIT_SHADOW / 2;
    obj->hit_points = M_HITPOINTS;

    obj->intelligent = true;
    obj->save_anim = true;
    obj->save_flags = true;
    obj->save_hitpoints = true;
    obj->save_position = true;

    Object_GetBone(obj, 6)->rot.x = true;
    Object_GetBone(obj, 6)->rot.y = true;
    Object_GetBone(obj, 13)->rot.y = true;
}

REGISTER_OBJECT(O_PUNK_1, M_Setup)
REGISTER_OBJECT(O_PUNK_2, M_Setup)
