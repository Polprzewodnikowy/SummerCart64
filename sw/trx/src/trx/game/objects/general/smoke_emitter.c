#include <trx/game/lara.h>
#include <trx/game/objects.h>
#include <trx/game/output.h>
#include <trx/game/random.h>
#include <trx/game/sparks.h>

#define M_DISTANCE (16 * WALL_L)

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    ITEM *const lara_item = Lara_GetItem();

    int32_t time = Output_GetTimeInGame();
    if (!Item_IsTriggerActive(item)) {
        return;
    }

    if ((time % 4) || (item->object_id == O_STEAM_EMITTER && (time % 8))) {
        return;
    }

    const int32_t dx = lara_item->pos.x - item->pos.x;
    const int32_t dz = lara_item->pos.z - item->pos.z;
    if (dx < -M_DISTANCE || dx > M_DISTANCE || dz < -M_DISTANCE
        || dz > M_DISTANCE) {
        return;
    }

    SPARK *const spark = Sparks_GetFreeSpark();
    spark->on = true;
    spark->src_color.r = 0;
    spark->src_color.g = 0;
    spark->src_color.b = 0;
    spark->dst_color.r = 32;
    spark->dst_color.g = 32;
    spark->dst_color.b = 32;

    spark->fade_to_black = 64;
    spark->col_fade_speed = (Random_GetControl() & 7) + 16;
    spark->life = (Random_GetControl() & 0xF) + 96;
    spark->s_life = spark->life;

    if (item->object_id == O_SMOKE_EMITTER_BLACK) {
        spark->draw_type = DRAW_BLEND_SUB;
    } else {
        spark->draw_type = DRAW_BLEND_ADD;
    }

    spark->extras = 0;
    spark->dynamic = -1;
    spark->pos.x = item->pos.x + (Random_GetControl() & 0xF) - 8;
    spark->pos.y = item->pos.y + (Random_GetControl() & 0xF) - 8;
    spark->pos.z = item->pos.z + (Random_GetControl() & 0xF) - 8;
    spark->vel.x = (Random_GetControl() & 0xFF) - 128;
    spark->vel.y = -16 - (Random_GetControl() & 0xF);
    spark->vel.z = (Random_GetControl() & 0xFF) - 128;
    spark->friction = 4;

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
    spark->sprite_idx = Object_Get(O_EXPLOSION_1)->mesh_idx;
    spark->gravity = -8 - (Random_GetControl() & 7);
    spark->max_y_vel = -4 - (Random_GetControl() & 7);
    spark->dst_size.width = (Random_GetControl() & 0x1F) + 128;
    spark->dst_size.height =
        spark->dst_size.width + (Random_GetControl() & 0x1F) + 32;
    spark->size.width = spark->dst_size.width >> 2;
    spark->size.height = spark->dst_size.height >> 2;
    spark->src_size.width = spark->size.width;
    spark->src_size.height = spark->size.height >> 2;

    if (item->object_id == O_STEAM_EMITTER) {
        spark->gravity >>= 1;
        spark->vel.y >>= 1;
        spark->max_y_vel >>= 1;
        spark->dst_color.r = 24;
        spark->dst_color.g = 24;
        spark->dst_color.b = 24;
    }
    Sparks_FinishSetup(spark);
}

static void M_Setup(OBJECT *const obj)
{
    obj->control_func = M_Control;
    obj->draw_func = nullptr;
    obj->save_flags = true;
}

REGISTER_OBJECT(O_SMOKE_EMITTER_WHITE, M_Setup)
REGISTER_OBJECT(O_SMOKE_EMITTER_BLACK, M_Setup)
REGISTER_OBJECT(O_STEAM_EMITTER, M_Setup)
