#include <trx/core/memory.h>
#include <trx/game/lara/enum.h>

#include <ctype.h>
#include <lauxlib.h>
#include <stdint.h>
#include <string.h>

static void M_PushCatalogKey(
    lua_State *const L, const char *const name, const char *const prefix,
    const int32_t value)
{
    const char *key = name;
    if (prefix != nullptr) {
        const size_t prefix_len = strlen(prefix);
        if (strncmp(name, prefix, prefix_len) == 0) {
            key = name + prefix_len;
        }
    }

    const size_t key_len = strlen(key);
    char *const lower_key = Memory_Alloc(key_len + 1);
    for (size_t i = 0; i < key_len; i++) {
        lower_key[i] = (char)tolower((unsigned char)key[i]);
    }
    lower_key[key_len] = '\0';

    lua_pushinteger(L, value);
    lua_setfield(L, -2, lower_key);
    Memory_Free(lower_key);
}

static void M_PushObjects(lua_State *const L)
{
    lua_newtable(L);

    int32_t id = 0;
#define X_CATALOG_ID(enum_value) M_PushCatalogKey(L, #enum_value, "O_", id++);
#include "trx/game/catalog/objects.def"
#undef X_CATALOG_ID

    lua_setfield(L, -2, "objects");
}

static void M_PushFlipEffects(lua_State *const L)
{
    lua_newtable(L);

    int32_t id = 0;
#define X_CATALOG_ID(enum_value)                                               \
    M_PushCatalogKey(L, #enum_value, "ITEM_ACTION_", id++);
#include "trx/game/catalog/item_actions.def"
#undef X_CATALOG_ID

    lua_setfield(L, -2, "flip_effects");
}

static void M_PushLaraStates(lua_State *const L)
{
    lua_newtable(L);

    int32_t id = 0;
#define X_CATALOG_ID(enum_value) M_PushCatalogKey(L, #enum_value, "LS_", id++);
#include "trx/game/catalog/lara_states.def"
#undef X_CATALOG_ID

    lua_setfield(L, -2, "lara_states");
}

static void M_PushWeapons(lua_State *const L)
{
    lua_newtable(L);

#define X_LUA_WEAPON(enum_value)                                               \
    M_PushCatalogKey(L, #enum_value, "LGT_", enum_value);
    X_LUA_WEAPON(LGT_UNARMED);
    X_LUA_WEAPON(LGT_PISTOLS);
    X_LUA_WEAPON(LGT_MAGNUMS);
    X_LUA_WEAPON(LGT_UZIS);
    X_LUA_WEAPON(LGT_SHOTGUN);
    X_LUA_WEAPON(LGT_M16);
    X_LUA_WEAPON(LGT_GRENADE);
    X_LUA_WEAPON(LGT_HARPOON);
    X_LUA_WEAPON(LGT_FLARE);
    X_LUA_WEAPON(LGT_SKIDOO);
    X_LUA_WEAPON(LGT_AUTOS);
    X_LUA_WEAPON(LGT_DESERT_EAGLE);
    X_LUA_WEAPON(LGT_MP5);
    X_LUA_WEAPON(LGT_ROCKET);
#undef X_LUA_WEAPON

    lua_setfield(L, -2, "weapons");
}

static void M_PushLaraAnims(lua_State *const L)
{
    lua_newtable(L);

    int32_t id = 0;
#define X_CATALOG_ID(enum_value) M_PushCatalogKey(L, #enum_value, "LA_", id++);
#include "trx/game/catalog/lara_anims.def"
#undef X_CATALOG_ID

    lua_setfield(L, -2, "lara_anims");
}

static void M_PushMusic(lua_State *const L)
{
    lua_newtable(L);

    int32_t id = 0;
#define X_CATALOG_ID(enum_value) M_PushCatalogKey(L, #enum_value, "MX_", id++);
#include "trx/game/catalog/music.def"
#undef X_CATALOG_ID

    lua_setfield(L, -2, "music");
}

static void M_PushSamples(lua_State *const L)
{
    lua_newtable(L);

    int32_t id = 0;
#define X_CATALOG_ID(enum_value) M_PushCatalogKey(L, #enum_value, "SFX_", id++);
#include "trx/game/catalog/samples.def"
#undef X_CATALOG_ID

    lua_setfield(L, -2, "samples");
}

void LUA_CreateCatalog(lua_State *const L)
{
    lua_getglobal(L, "trxc");
    lua_newtable(L);

    M_PushObjects(L);
    M_PushFlipEffects(L);
    M_PushLaraStates(L);
    M_PushWeapons(L);
    M_PushLaraAnims(L);
    M_PushMusic(L);
    M_PushSamples(L);

    lua_setfield(L, -2, "catalog");
    lua_pop(L, 1);
}
