#include <trx/game/lara.h>
#include <trx/game/objects.h>
#include <trx/game/output.h>
#include <trx/game/random.h>
#include <trx/game/sparks.h>

#define M_DISTANCE (16 * WALL_L) // = 16384

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);

    const int32_t time = Output_GetTimeInGame();
    if (!Item_IsTriggerActive(item) || (time % 4) != 0) {
        return;
    }

    const ITEM *const lara_item = Lara_GetItem();
    const int32_t dx = lara_item->pos.x - item->pos.x;
    const int32_t dz = lara_item->pos.z - item->pos.z;
    if (ABS(dx) > M_DISTANCE || ABS(dz) > M_DISTANCE) {
        return;
    }

    SPARK *const spark = Sparks_GetFreeSpark();
    spark->on = true;
    spark->src_color.r = 0;
    spark->src_color.g = 0;
    spark->src_color.b = 0;
    spark->dst_color.r = 12;
    spark->dst_color.g = 32;
    spark->dst_color.b = 0;

    spark->fade_to_black = 32;
    spark->col_fade_speed = (Random_GetControl() & 7) + 24;
    spark->draw_type = DRAW_BLEND_ADD;
    spark->extras = 0;
    spark->life = (Random_GetControl() & 7) + 64;
    spark->s_life = spark->life;
    spark->dynamic = -1;
    spark->pos.x = (Random_GetControl() & 0x1FF) + item->pos.x - 256;
    spark->pos.y = item->pos.y - (Random_GetControl() & 0xF) - 264;
    spark->pos.z = (Random_GetControl() & 0x3FF) + item->pos.z - 512;
    spark->vel.x = (Random_GetControl() & 0xFF) - 128;
    spark->vel.y = -1 - (Random_GetControl() & 1);
    spark->vel.z = (Random_GetControl() & 0xFF) - 128;
    spark->friction = 4;

    spark->flags = SPARK_F_ALT_SPRITE | SPARK_F_SPRITE | SPARK_F_SCALE;
    if ((Random_GetControl() & 1) != 0) {
        spark->flags |= SPARK_F_ROTATE;
        spark->rot_angle = Random_GetControl() & 0xFFF;
        if ((Random_GetControl() & 1) != 0) {
            spark->rot_add = -4 - (Random_GetControl() & 7);
        } else {
            spark->rot_add = (Random_GetControl() & 7) + 4;
        }
    }

    spark->scalar = 3;
    spark->sprite_idx = Object_Get(O_EXPLOSION_1)->mesh_idx;
    spark->max_y_vel = 0;
    spark->gravity = 0;

    const uint8_t size = (Random_GetControl() & 0x1F) + 96;
    spark->size.width = size >> 1;
    spark->src_size.width = spark->size.width;
    spark->dst_size.width = spark->size.width;
    spark->size.height = (size + (Random_GetControl() & 0x1F) + 32) >> 1;
    spark->src_size.height = spark->size.height;
    spark->dst_size.height = spark->size.height;
    Sparks_FinishSetup(spark);
}

static void M_Setup(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }

    obj->control_func = M_Control;
    obj->draw_func = nullptr;
    obj->save_flags = true;
}

REGISTER_OBJECT(O_GAS_EMITTER_GREEN, M_Setup)
