#include <trx/core/log.h>
#include <trx/game/effects.h>
#include <trx/game/lara.h>
#include <trx/game/objects.h>
#include <trx/game/random.h>
#include <trx/game/rooms.h>
#include <trx/game/sound.h>
#include <trx/game/sparks.h>

#define M_EPSILON 32

typedef struct {
    bool is_initialised;
    bool is_flat;
} M_PRIV;

static bool M_IsFenceOnDeathSector(const ITEM *const item)
{
    int16_t room_num = item->room_num;
    const SECTOR *sector = Room_GetSector(item->pos, &room_num);
    return sector->is_death_sector;
}

static void M_TriggerFenceSparks(const XYZ_32 pos, const bool kill)
{
    SPARK *const spark = Sparks_GetFreeSpark();
    spark->on = true;
    spark->src_color.b = (Random_GetControl() & 63) + 192;
    spark->src_color.r = spark->src_color.b;
    spark->src_color.g = spark->src_color.b;
    spark->dst_color.b = (Random_GetControl() & 0x3F) + 192;
    spark->dst_color.r = spark->dst_color.b >> 2;
    spark->dst_color.g = spark->dst_color.b >> 1; // OG: 1

    spark->col_fade_speed = 8;
    spark->fade_to_black = 16;
    spark->draw_type = DRAW_BLEND_ADD;
    spark->extras = 0;
    spark->life = 32 + (Random_GetControl() & 7);
    spark->s_life = spark->life;
    spark->dynamic = -1;

    spark->pos = pos;
    spark->vel.x = ((Random_GetControl() & 0xFF) << 1) - 256;
    spark->vel.y = (Random_GetControl() & 0xF) - ((int32_t)kill << 5) - 8;
    spark->vel.z = ((Random_GetControl() & 0xFF) << 1) - 256;

    spark->friction = 4;
    spark->flags = SPARK_F_SCALE;
    spark->scalar = 1 + kill;
    spark->max_y_vel = 0;
    spark->gravity = 16 + (Random_GetControl() & 0xF);

    spark->size.width = 4 + (Random_GetControl() & 3);
    spark->src_size.width = spark->size.width;
    spark->dst_size.width = 1;

    spark->size.height = spark->size.width * 2;
    spark->src_size.height = spark->src_size.width * 2;
    spark->dst_size.height = spark->dst_size.width * 2;
    Sparks_FinishSetup(spark);
}

static void M_TouchFence(const XZ_32 spark_axis, XYZ_32 spark_pos)
{
    ITEM *const lara_item = Lara_GetItem();
    const XYZ_32 old_spark_pos = spark_pos;
    const int32_t iterations = (Random_GetControl() & 0xF) + 3;

    for (int32_t i = 0; i < iterations; i++) {
        if (spark_axis.x != 0) {
            spark_pos.x =
                (Random_GetControl() & 0x1FF) + lara_item->pos.x - 256;
        } else {
            spark_pos.z =
                (Random_GetControl() & 0x1FF) + lara_item->pos.z - 256;
        }

        spark_pos.y = lara_item->pos.y - Random_GetControl() % 768;
        const int32_t spark_count = (Random_GetControl() & 3) + 6;

        for (int32_t j = 0; j < spark_count; j++) {
            M_TriggerFenceSparks(spark_pos, true);
            if (spark_axis.x != 0) {
                spark_pos.x += (spark_axis.x & Random_GetControl() & 7) - 4;
            } else {
                spark_pos.z += (spark_axis.z & Random_GetControl() & 7) - 4;
            }
            spark_pos.y += (Random_GetControl() & 7) - 4;
        }

        spark_pos = old_spark_pos;
    }

    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->electric = 1;
    lara_item->hit_points = 0;
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    if (!Item_IsTriggerActive(item)) {
        return;
    }

    ITEM *const lara_item = Lara_GetItem();

    const int32_t dx = lara_item->pos.x - item->pos.x;
    const int32_t dz = lara_item->pos.z - item->pos.z;
    if (dx < -0x5000 || dx > 0x5000 || dz < -0x5000 || dz > 0x5000) {
        return;
    }

    XYZ_32 fence_center = {};
    XYZ_32 spark_pos = {};
    XZ_32 spark_axis = {};
    XZ_32 fence_size = {};

    M_PRIV *const p = item->priv;
    if (!p->is_initialised) {
        p->is_initialised = true;
        p->is_flat = M_IsFenceOnDeathSector(item);
    }

    switch (item->rot.y) {
    case 0:
        fence_center.x = item->pos.x + WALL_L / 2;
        fence_center.z = item->pos.z + WALL_L / 2;
        fence_size.x = WALL_L + M_EPSILON;
        fence_size.z = 128;
        spark_pos.x = fence_center.x - (WALL_L - M_EPSILON);
        spark_pos.z = fence_center.z;
        if (p->is_flat) {
            spark_axis.x = Random_GetControl() & 0x3FF;
            spark_pos.z += spark_axis.x * -(Random_GetControl() & 1);
        }
        spark_axis.x = WALL_L;
        spark_axis.z = 0;
        break;

    case -DEG_90:
        fence_center.x = item->pos.x - WALL_L / 2;
        fence_center.z = item->pos.z + WALL_L / 2;
        fence_size.x = 128;
        fence_size.z = WALL_L + M_EPSILON;
        spark_pos.x = fence_center.x;
        spark_pos.z = fence_center.z - (WALL_L - M_EPSILON);
        if (p->is_flat) {
            spark_axis.x = Random_GetControl() & 0x3FF;
            spark_pos.x += spark_axis.x * -(Random_GetControl() & 1);
        }
        spark_axis.x = 0;
        spark_axis.z = WALL_L;
        break;

    case -DEG_180:
        fence_center.x = item->pos.x - WALL_L / 2;
        fence_center.z = item->pos.z - WALL_L / 2;
        fence_size.x = WALL_L + M_EPSILON;
        fence_size.z = 128;
        spark_pos.x = fence_center.x - (WALL_L - M_EPSILON);
        spark_pos.z = fence_center.z;
        if (p->is_flat) {
            spark_axis.x = Random_GetControl() & 0x3FF;
            spark_pos.z += spark_axis.x * -(Random_GetControl() & 1);
        }
        spark_axis.x = WALL_L;
        spark_axis.z = 0;
        break;

    case DEG_90:
        fence_center.x = item->pos.x + WALL_L / 2;
        fence_center.z = item->pos.z - WALL_L / 2;
        fence_size.x = 128;
        fence_size.z = WALL_L + M_EPSILON;
        spark_pos.x = fence_center.x;
        spark_pos.z = fence_center.z - (WALL_L - M_EPSILON);
        if (p->is_flat) {
            spark_axis.x = Random_GetControl() & 0x3FF;
            spark_pos.x += spark_axis.x * -(Random_GetControl() & 1);
        }
        spark_axis.x = 0;
        spark_axis.z = WALL_L;
        break;

    default:
        break;
    }

    if ((Random_GetControl() & 0x1F) == 0) {
        if (spark_axis.x != 0) {
            spark_pos.x += Random_GetControl() & spark_axis.x;
        } else {
            spark_pos.z += Random_GetControl() & spark_axis.z;
        }

        if (p->is_flat) {
            spark_pos.y = item->pos.y - (Random_GetControl() & 0x1F);
        } else {
            spark_pos.y = item->pos.y - (Random_GetControl() & 0x7FF)
                - (Random_GetControl() & 0x3FF);
        }

        const int32_t spark_count = (Random_GetControl() & 3) + 3;
        for (int32_t i = 0; i < spark_count; i++) {
            M_TriggerFenceSparks(spark_pos, false);

            if (spark_axis.x != 0) {
                spark_pos.x += (spark_axis.x & Random_GetControl() & 7) - 4;
            } else {
                spark_pos.z += (spark_axis.z & Random_GetControl() & 7) - 4;
            }

            spark_pos.y += (Random_GetControl() & 7) - 4;
        }
    }

    LARA_INFO *const lara = Lara_GetLaraInfo();
    if (lara->electric != 0 || p->is_flat
        || lara_item->pos.x < fence_center.x - fence_size.x
        || lara_item->pos.x > fence_center.x + fence_size.x
        || lara_item->pos.z < fence_center.z - fence_size.z
        || lara_item->pos.z > fence_center.z + fence_size.z
        || lara_item->pos.y > item->pos.y + M_EPSILON
        || lara_item->pos.y < item->pos.y - 3 * WALL_L) {
        return;
    }

    M_TouchFence(spark_axis, spark_pos);
}

static void M_Setup(OBJECT *const obj)
{
    obj->control_func = M_Control;
    obj->draw_func = nullptr;
    obj->priv_size = sizeof(M_PRIV);
    obj->save_flags = true;
}

REGISTER_OBJECT(O_ELECTRIC_FENCE, M_Setup)
