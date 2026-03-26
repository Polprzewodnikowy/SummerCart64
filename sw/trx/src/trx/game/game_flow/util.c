#include <trx/config.h>
#include <trx/debug.h>
#include <trx/game/game.h>
#include <trx/game/items.h>
#include <trx/game/objects.h>
#include <trx/game/savegame.h>
#include <trx/game/shell.h>

static void M_DisableObject(const OBJECT_ID object_id)
{
    OBJECT *const obj = Object_Get(object_id);
    obj->loaded = false;
    obj->collision_func = nullptr;
    obj->control_func = nullptr;
    obj->draw_func = nullptr;
    obj->floor_height_func = nullptr;
    obj->ceiling_height_func = nullptr;
}

static void M_ReplaceObject(
    const OBJECT_ID src_object_id, const OBJECT_ID dst_object_id)
{
    for (int32_t i = 0; i < Item_GetTotalCount(); i++) {
        ITEM *const item = Item_Get(i);
        if (item->object_id == src_object_id) {
            item->object_id = dst_object_id;
        }
    }
}

void GF_DisableObjectsIfNeeded(void)
{
    const GF_LEVEL *const level = Game_GetCurrentLevel();
    if (level == nullptr
        || (level->type != GFL_NORMAL && level->type != GFL_BONUS
            && level->type != GFL_GYM)) {
        return;
    }
    if (g_Config.gameplay.disable_medpacks) {
        M_DisableObject(O_SMALL_MEDIPACK_ITEM);
        M_DisableObject(O_LARGE_MEDIPACK_ITEM);
    }

    if (g_Config.gameplay.disable_extra_guns) {
        const RESUME_INFO *const resume = Savegame_GetCurrentInfo(level);
        ASSERT(resume != nullptr);
        for (int32_t i = 0; g_GunObjects[i] != NO_OBJECT; i++) {
            if (resume->flags.has_pistols) {
                M_DisableObject(g_GunObjects[i]);
            } else {
                M_ReplaceObject(g_GunObjects[i], O_PISTOL_ITEM);
            }
            M_DisableObject(
                Object_GetCognate(g_GunObjects[i], g_GunAmmoObjectMap));
        }
    }
}

int32_t GF_BadGetLevelNum(void)
{
    return GF_GetCurrentLevel()->num;
}

bool GF_BadIsMod(const char *const mod)
{
    return Shell_IsCurrentMod(mod);
}
