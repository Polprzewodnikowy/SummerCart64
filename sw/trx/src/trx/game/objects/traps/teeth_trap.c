#include <trx/game/lara.h>
#include <trx/game/objects.h>
#include <trx/game/spawn.h>

#define M_DAMAGE 400
#define M_NUM_TEETH 6

typedef enum {
    TEETH_TRAP_STATE_NICE = 0,
    TEETH_TRAP_STATE_NASTY = 1,
} TEETH_TRAP_STATE;

static const BITE m_Teeth[M_NUM_TEETH] = {
    // clang-format off
    { .pos = { .x = -23, .y = 0,   .z = -1718 }, .mesh_num = 0 },
    { .pos = { .x = 71,  .y = 0,   .z = -1718 }, .mesh_num = 1 },
    { .pos = { .x = -23, .y = 10,  .z = -1718 }, .mesh_num = 0 },
    { .pos = { .x = 71,  .y = 10,  .z = -1718 }, .mesh_num = 1 },
    { .pos = { .x = -23, .y = -10, .z = -1718 }, .mesh_num = 0 },
    { .pos = { .x = 71,  .y = -10, .z = -1718 }, .mesh_num = 1 },
    // clang-format on
};

static void M_Bite(ITEM *const item, const BITE *const bite)
{
    XYZ_32 pos = bite->pos;
    Collide_GetJointAbsPosition(item, &pos, bite->mesh_num);
    Spawn_Blood(pos.x, pos.y, pos.z, item->speed, item->rot.y, item->room_num);
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);

    if (Item_IsTriggerActive(item)) {
        item->goal_anim_state = TEETH_TRAP_STATE_NASTY;
        if (item->touch_bits != 0
            && item->current_anim_state == TEETH_TRAP_STATE_NASTY) {
            Lara_TakeDamage(M_DAMAGE, true);
            for (int32_t i = 0; i < M_NUM_TEETH; i++) {
                M_Bite(item, &m_Teeth[i]);
            }
        }
    } else {
        item->goal_anim_state = TEETH_TRAP_STATE_NICE;
    }

    Item_Animate(item);
}

static void M_Setup(OBJECT *const obj)
{
    obj->control_func = M_Control;
    obj->collision_func = Object_Collision_Trap;
    obj->save_flags = true;
    obj->save_anim = true;
}

REGISTER_OBJECT(O_TEETH_TRAP, M_Setup)
