#include <trx/config.h>
#include <trx/game/lara.h>
#include <trx/game/objects/common.h>
#include <trx/game/pathing.h>

#define M_DEFAULT_RADIUS 10

static void M_SetupLara(void)
{
    OBJECT *const obj = Object_Get(O_LARA);
    obj->initialise_func = Lara_InitialiseLoad;
    obj->can_interpolate_func = Lara_CanInterpolate;
    obj->draw_func = nullptr;
    obj->get_mesh_index_func = Lara_GetMeshIndex;

    obj->shadow_size = (UNIT_SHADOW * 10) / 16;
    obj->hit_points = g_Config.gameplay.start_lara_hitpoints;

    obj->save_position = true;
    obj->save_hitpoints = true;
    obj->save_flags = true;
    obj->save_anim = true;
}

void Object_SetupAllObjects(void)
{
    for (int32_t i = O_FIRST; i < O_NUMBER_OF; i++) {
        OBJECT *const obj = Object_Get(i);
        obj->initialise_func = nullptr;
        obj->control_func = nullptr;
        obj->floor_height_func = nullptr;
        obj->ceiling_height_func = nullptr;
        obj->draw_func = Object_DrawAnimatingItem;
        obj->collision_func = nullptr;
        obj->add_walkable_func = nullptr;
        obj->is_usable_func = nullptr;
        obj->can_drop_items_func = nullptr;
        obj->can_interpolate_func = Object_CanInterpolate;
        obj->should_spawn_blood_func = nullptr;
        obj->is_alive_func = nullptr;
        obj->is_targetable_func = nullptr;
        obj->can_take_damage_func = nullptr;
        obj->can_be_projectile_target_func = nullptr;
        obj->can_be_exploded_func = nullptr;
        obj->get_mesh_index_func = nullptr;
        obj->hit_points = 0;
        obj->pivot_length = 0;
        obj->radius = M_DEFAULT_RADIUS;
        obj->shadow_size = 0;
        obj->enable_interpolation = true;
        obj->lot_setup = LOT_Setup(LOT_SETUP_DEFAULT);

        obj->save_position = false;
        obj->save_hitpoints = false;
        obj->save_flags = false;
        obj->save_anim = false;
        obj->load_floor = false;
        obj->intelligent = false;
        obj->smartness = -1;

        if (obj->setup_func != nullptr) {
            obj->setup_func(obj);
        }
    }

    M_SetupLara();
    Lara_Hair_Initialise();
}
