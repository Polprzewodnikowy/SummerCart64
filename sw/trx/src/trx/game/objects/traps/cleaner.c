#include <trx/config.h>
#include <trx/core/json/util/read_io.h>
#include <trx/core/json/util/write_io.h>
#include <trx/game/lara.h>
#include <trx/game/objects.h>
#include <trx/game/output.h>
#include <trx/game/random.h>
#include <trx/game/sound.h>
#include <trx/game/sparks.h>

// clang-format off
#define M_NODE_COUNT 3
#define M_TOUCH_BITS 0b11111111'11111100
#define M_RADIUS     (STEP_L * 2)  // = 512
#define M_TURN       (DEG_90 / 16) // = 1024
#define M_VELOCITY   (STEP_L / 4)  // = 64
#define M_MAX_DIST   (WALL_L * 20) // = 20480
// clang-format on

typedef struct {
    bool resume;
    int16_t turn;
    int16_t velocity;
    uint8_t sparks[M_NODE_COUNT];
} M_PRIV;

typedef struct {
    int16_t joint_idx;
    int16_t node_idx;
} M_SPARK_NODE;

static const M_SPARK_NODE m_Nodes[M_NODE_COUNT] = {
    { .joint_idx = 5, .node_idx = 9 },
    { .joint_idx = 9, .node_idx = 10 },
    { .joint_idx = 13, .node_idx = 11 },
};

static void M_LoadPriv(ITEM *const item, JSON_READ_IO *const io)
{
    M_PRIV *const p = item->priv;
    JSON_SHOULD(JSON_READ(io, "resume", &p->resume));
    JSON_SHOULD(JSON_READ(io, "turn", &p->turn));
    JSON_SHOULD(JSON_READ(io, "velocity", &p->velocity));
}

static void M_SavePriv(const ITEM *const item, JSON_WRITE_IO *const io)
{
    const M_PRIV *const p = item->priv;
    JSONW_WRITE(io, "resume", p->resume);
    JSONW_WRITE(io, "turn", p->turn);
    JSONW_WRITE(io, "velocity", p->velocity);
}

static void M_Initialise(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;

    item->pos.x = ROUND_TO_SECTOR(item->pos.x) + STEP_L * 2;
    item->pos.z = ROUND_TO_SECTOR(item->pos.z) + STEP_L * 2;
    p->resume = false;
    p->turn = M_TURN;
    p->velocity = M_VELOCITY;
}

static void M_TriggerSparks(
    const XYZ_32 pos, const int16_t item_num, const int16_t node)
{
    const ITEM *const lara_item = Lara_GetItem();
    const XZ_32 delta = {
        .x = lara_item->pos.x - pos.x,
        .z = lara_item->pos.z - pos.z,
    };
    if (ABS(delta.x) > M_MAX_DIST || ABS(delta.z) > M_MAX_DIST) {
        return;
    }

    SPARK *const spark = Sparks_GetFreeSpark();
    spark->on = true;
    spark->src_color.r = (Random_GetControl() & 0x3F) + 192;
    spark->src_color.g = spark->src_color.r;
    spark->src_color.b = spark->src_color.r;
    spark->dst_color.r = spark->src_color.b >> 2;
    spark->dst_color.g = spark->src_color.b >> 1;
    spark->dst_color.b = (Random_GetControl() & 0x3F) + 192;

    spark->col_fade_speed = 8;
    spark->fade_to_black = 8;
    spark->draw_type = DRAW_BLEND_ADD;
    spark->extras = 0;
    spark->dynamic = -1;
    spark->life = (Random_GetControl() & 7) + 20;
    spark->s_life = spark->life;
    spark->pos.x = (Random_GetControl() & 0x1F) - 16;
    spark->pos.y = (Random_GetControl() & 0x1F) - 16;
    spark->pos.z = (Random_GetControl() & 0x1F) - 16;
    spark->vel.x = ((Random_GetControl() & 0xFF) << 2) - 512;
    spark->vel.y = (Random_GetControl() & 7) - 4;
    spark->vel.z = ((Random_GetControl() & 0xFF) << 2) - 512;
    spark->friction = 4;
    spark->flags = SPARK_F_ATTACHED_NODE | SPARK_F_ITEM | SPARK_F_SCALE;
    spark->item_num = item_num;
    spark->node_num = node;
    spark->scalar = 1;
    spark->size.width = (Random_GetControl() & 3) + 4;
    spark->src_size.width = spark->size.width;
    spark->dst_size.width = spark->size.width >> 1;
    spark->size.height = spark->size.width;
    spark->src_size.height = spark->size.height;
    spark->dst_size.height = spark->size.height >> 1;
    spark->max_y_vel = 0;
    spark->gravity = (Random_GetControl() & 3) + 4;
    Sparks_FinishSetup(spark);
}

static XZ_32 M_GetDirection(const int16_t yaw)
{
    // clang-format off
    switch (yaw) {
    case 0:         return (XZ_32) { +0, +1 };
    case DEG_90:    return (XZ_32) { +1, +0 };
    case -DEG_90:   return (XZ_32) { -1, +0 };
    case -DEG_180:  return (XZ_32) { +0, -1 };
    default:        return (XZ_32) { +0, +0 };
    }
    // clang-format on
}

static bool M_IsMidSector(const ITEM *const item)
{
    const XZ_32 dir = M_GetDirection(item->rot.y);
    if (dir.x != 0) {
        return (item->pos.x & (WALL_L - 1)) == STEP_L * 2;
    }
    if (dir.z != 0) {
        return (item->pos.z & (WALL_L - 1)) == STEP_L * 2;
    }
    return false;
}

static bool M_TestSector(
    const XYZ_32 pos, const XZ_32 dir, int16_t *const room_num)
{
    const XYZ_32 test_pos = {
        .x = pos.x + dir.x * WALL_L,
        .y = pos.y,
        .z = pos.z + dir.z * WALL_L,
    };
    const SECTOR *sector = Room_GetSector(test_pos, room_num);
    const int32_t height = Room_GetHeight(sector, test_pos);
    const ROOM *const room = Room_Get(*room_num);
    sector = Room_GetWorldSector(room, test_pos.x, test_pos.z);
    return height == test_pos.y && !sector->stopper;
}

static void M_DecideMove(ITEM *const item)
{
    M_PRIV *const p = item->priv;
    const XZ_32 dir_ahead = M_GetDirection(item->rot.y);
    const XZ_32 dir_left = M_GetDirection(item->rot.y - DEG_90);

    int16_t room_num = item->room_num;
    const bool move_left = M_TestSector(item->pos, dir_left, &room_num);

    room_num = item->room_num;
    const bool move_ahead = M_TestSector(item->pos, dir_ahead, &room_num);

    if (!move_ahead && !move_left && p->turn > 0) {
        item->rot.y += M_TURN;
        p->turn = M_TURN;
    } else if (!move_ahead && !move_left && p->turn < 0) {
        item->rot.y -= M_TURN;
        p->turn = -M_TURN;
    } else if (move_left && p->turn > 0) {
        item->rot.y -= M_TURN;
        p->turn = -M_TURN;
    } else {
        p->turn = M_TURN;
        p->resume = true;
        item->pos.x += dir_ahead.x * p->velocity;
        item->pos.z += dir_ahead.z * p->velocity;

        XYZ_32 pos = item->pos;
        pos.x += dir_ahead.x * WALL_L;
        pos.z += dir_ahead.z * WALL_L;
        const ROOM *const room = Room_Get(room_num);
        SECTOR *const sector = Room_GetWorldSector(room, pos.x, pos.z);
        sector->stopper = true;
    }
}

static void M_HitLara(ITEM *const item)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    if (g_Config.debug.enable_invulnerability || lara->electric != 0) {
        return;
    }

    lara->electric = 1;
    Lara_GetItem()->hit_points = 0;

    M_PRIV *const p = item->priv;
    p->velocity = 0;
    Sound_Effect(SFX_CLEANER_FUSEBOX, &item->pos, SPM_NORMAL);
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;
    if (p->velocity == 0) {
        return;
    }

    if ((item->rot.y & 0x3FFF) != 0) {
        item->rot.y += p->turn;
    } else if (M_IsMidSector(item)) {
        if (p->resume) {
            const XYZ_32 pos =
                XYZ_32_OffsetYaw(item->pos, item->rot.y + DEG_180, WALL_L);
            int16_t room_num = item->room_num;
            Room_GetSector(pos, &room_num);
            const ROOM *const room = Room_Get(room_num);
            SECTOR *const sector = Room_GetWorldSector(room, pos.x, pos.z);
            sector->stopper = false;
            p->resume = false;
        }

        M_DecideMove(item);

        if (Room_TestTriggers(item)) {
            p->velocity = 0;
            Sound_Effect(SFX_CLEANER_FUSEBOX, &item->pos, SPM_NORMAL);
        }
    } else {
        const XZ_32 dir = M_GetDirection(item->rot.y);
        item->pos.x += dir.x * p->velocity;
        item->pos.z += dir.z * p->velocity;
    }

    if ((item->touch_bits & M_TOUCH_BITS) != 0) {
        M_HitLara(item);
    }

    Item_Animate(item);
    int16_t room_num = item->room_num;
    Room_GetSector(item->pos, &room_num);
    Item_UpdateRoom(item_num, room_num);

    Sound_Effect(SFX_CLEANER_LOOP, &item->pos, SPM_NORMAL);

    for (int32_t i = 0; i < M_NODE_COUNT; i++) {
        const M_SPARK_NODE *const node = &m_Nodes[i];
        const int32_t rnd = Random_GetControl();
        if (p->sparks[i] == 0 && (rnd & 7) != 0) {
            continue;
        }

        if (p->sparks[i] == 0) {
            p->sparks[i] = (Random_GetControl() & 7) + 4;
        } else {
            p->sparks[i]--;
        }

        XYZ_32 pos = {
            .x = -160,
            .y = -8,
            .z = 16,
        };
        Collide_GetJointAbsPosition(item, &pos, node->joint_idx);
        M_TriggerSparks(pos, item_num, node->node_idx);
        pos.x += (Random_GetControl() & 0x1F) - 16;
        pos.y += (Random_GetControl() & 0x1F) - 16;
        pos.z += (Random_GetControl() & 0x1F) - 16;

        const int32_t shade = (Random_GetControl() & 0x7F) + 128;
        const RGB_888 color = { shade >> 2, shade >> 1, shade };
        Output_AddDynamicLightRGB(pos, 10, color);
    }
}

static void M_Setup(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }

    obj->initialise_func = M_Initialise;
    obj->control_func = M_Control;
    obj->collision_func = Object_Collision;
    obj->priv_size = sizeof(M_PRIV);
    obj->priv_load_func = M_LoadPriv;
    obj->priv_save_func = M_SavePriv;
    obj->radius = M_RADIUS;
    obj->save_position = true;
    obj->save_hitpoints = true;
    obj->save_flags = true;
    obj->save_anim = true;
}

REGISTER_OBJECT(O_ELECTRIC_CLEANER, M_Setup)
