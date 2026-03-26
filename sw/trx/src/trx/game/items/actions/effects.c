#include <trx/config.h>
#include <trx/game/camera.h>
#include <trx/game/items.h>
#include <trx/game/lara.h>
#include <trx/game/rooms.h>
#include <trx/game/sound.h>
#include <trx/version.h>

static void M_ChainBlock(ITEM *const item)
{
    const int32_t flip_timer = Room_GetFlipTimer();
    if (g_Config.audio.fix_chainblock_secret_sound) {
        if (flip_timer == 0) {
            Sound_Effect(SFX_CHAINBLOCK_FX, nullptr, SPM_NORMAL);
            Room_SetFlipTimer(1);
            return;
        }
    }

    if (flip_timer == 0) {
        Sound_Effect(SFX_SECRET, nullptr, SPM_NORMAL);
    }

    if (flip_timer == 54) {
        Sound_Effect(SFX_LARA_SPLASH, nullptr, SPM_NORMAL);
        Room_SetFlipEffect(-1);
    }
    Room_IncrementFlipTimer(1);
}

static void M_Flood(ITEM *const item)
{
    const int32_t flip_timer = Room_GetFlipTimer();
    if (flip_timer > 4 * LOGIC_FPS) {
        Room_SetFlipEffect(-1);
        Room_IncrementFlipTimer(1);
        return;
    }

    const ITEM *const lara_item = Lara_GetItem();
    XYZ_32 pos = {
        .x = lara_item->pos.x,
        .y = g_Camera.target.pos.y,
        .z = lara_item->pos.z,
    };
    if (flip_timer >= LOGIC_FPS) {
        pos.y += 100 * (flip_timer - LOGIC_FPS);
    } else {
        pos.y += 100 * (LOGIC_FPS - flip_timer);
    }

    Sound_Effect(SFX_FLOOD, &pos, SPM_NORMAL);
    Room_IncrementFlipTimer(1);
}

static void M_Explosion(ITEM *const item)
{
    // TODO: unify
    Sound_Effect(
        g_TRVersion == 1 ? SFX_EXPLOSION_FX : SFX_EXPLOSION_1, nullptr,
        SPM_NORMAL);
    g_Camera.bounce = -75;
    Room_SetFlipEffect(-1);
}

static void M_Earthquake(ITEM *const item)
{
    const int32_t flip_timer = Room_GetFlipTimer();
    if (flip_timer == 0) {
        Sound_Effect(SFX_EXPLOSION_2, nullptr, SPM_NORMAL);
        g_Camera.bounce = -250;
    } else if (flip_timer == 3) {
        Sound_Effect(SFX_EARTHQUAKE_1, nullptr, SPM_NORMAL);
    } else if (flip_timer == 35) {
        Sound_Effect(SFX_EXPLOSION_2, nullptr, SPM_NORMAL);
    } else if (flip_timer == 20 || flip_timer == 50 || flip_timer == 70) {
        Sound_Effect(SFX_EARTHQUAKE_2, nullptr, SPM_NORMAL);
    }

    if (flip_timer == 104) {
        Room_SetFlipEffect(-1);
    }
    Room_IncrementFlipTimer(1);
}

static void M_Flicker(ITEM *const item)
{
    const int32_t flip_timer = Room_GetFlipTimer();
    if (flip_timer > 125) {
        Room_FlipMap();
        Room_SetFlipEffect(-1);
    } else if (
        flip_timer == 90 || flip_timer == 92 || flip_timer == 105
        || flip_timer == 107) {
        Room_FlipMap();
    }
    Room_IncrementFlipTimer(1);
}

static void M_FloorShake(ITEM *item)
{
    const int32_t max_dist = WALL_L * 16; // = 0x4000
    const int32_t max_bounce = 100;

    if (item == nullptr) {
        item = Lara_GetItem();
    }

    const int32_t dx = item->pos.x - g_Camera.pos.x;
    const int32_t dy = item->pos.y - g_Camera.pos.y;
    const int32_t dz = item->pos.z - g_Camera.pos.z;
    const int32_t dist = SQUARE(dz) + SQUARE(dy) + SQUARE(dx);

    if (ABS(dx) < max_dist && ABS(dy) < max_dist && ABS(dz) < max_dist) {
        g_Camera.bounce =
            max_bounce * (SQUARE(WALL_L) - dist / 256) / SQUARE(WALL_L);
    }
}

static void M_RaisingBlock(ITEM *const item)
{
    Sound_Effect(SFX_RAISINGBLOCK_FX, nullptr, SPM_NORMAL);
    Room_SetFlipEffect(-1);
}

static void M_PowerUp(ITEM *const item)
{
    const int32_t flip_timer = Room_GetFlipTimer();
    if (flip_timer > LOGIC_FPS * 4) {
        Room_SetFlipEffect(-1);
    } else {
        const XYZ_32 pos = {
            .x = g_Camera.target.x,
            .y = g_Camera.target.y + flip_timer * 100,
            .z = g_Camera.target.z,
        };
        Sound_Effect(SFX_POWERUP_FX, &pos, SPM_NORMAL);
    }
    Room_IncrementFlipTimer(1);
}

static void M_Stairs2Slope(ITEM *const item)
{
    const int32_t flip_timer = Room_GetFlipTimer();
    if (flip_timer == 5) {
        Sound_Effect(SFX_STAIRS_2_SLOPE_FX, nullptr, SPM_NORMAL);
        Room_SetFlipEffect(-1);
    }
    Room_IncrementFlipTimer(1);
}

static void M_DropSand(ITEM *const item)
{
    const int32_t flip_timer = Room_GetFlipTimer();
    if (flip_timer > LOGIC_FPS * 4) {
        Room_SetFlipEffect(-1);
    } else {
        if (flip_timer == 0) {
            Sound_Effect(SFX_TRAPDOOR_OPEN, nullptr, SPM_NORMAL);
        }
        const XYZ_32 pos = {
            .x = g_Camera.target.x,
            .y = g_Camera.target.y + flip_timer * 100,
            .z = g_Camera.target.z,
        };
        Sound_Effect(SFX_SAND_FX, &pos, SPM_NORMAL);
    }
    Room_IncrementFlipTimer(1);
}

static void M_Chandelier(ITEM *const item)
{
    const int32_t flip_timer = Room_GetFlipTimer();
    Sound_Effect(SFX_CHAIN_PULLEY, nullptr, SPM_NORMAL);
    if (flip_timer >= LOGIC_FPS) {
        Room_SetFlipEffect(-1);
    }
    Room_IncrementFlipTimer(1);
}

static void M_Rubble(ITEM *const item)
{
    Sound_Effect(SFX_MASSIVE_CRASH, nullptr, SPM_NORMAL);
    g_Camera.bounce = -350;
    Room_SetFlipEffect(-1);
}

static void M_Piston(ITEM *const item)
{
    Sound_Effect(SFX_PULLEY_CRANE, nullptr, SPM_NORMAL);
    Room_SetFlipEffect(-1);
}

static void M_Curtain(ITEM *const item)
{
    Sound_Effect(SFX_CURTAIN, nullptr, SPM_NORMAL);
    Room_SetFlipEffect(-1);
}

static void M_SetChange(ITEM *const item)
{
    Sound_Effect(SFX_STAGE_BACKDROP, nullptr, SPM_NORMAL);
    Room_SetFlipEffect(-1);
}

static void M_Statue(ITEM *const item)
{
    Sound_Effect(SFX_STONE_DOOR_SLIDE, nullptr, SPM_NORMAL);
    Room_SetFlipEffect(-1);
}

static void M_Boiler(ITEM *const item)
{
    Sound_Effect(SFX_BOILER, nullptr, SPM_NORMAL);
    Room_SetFlipEffect(-1);
}

static void M_CameraShake(ITEM *const item)
{
    g_Camera.bounce = -350;
    Room_SetFlipEffect(-1);
}

static void M_LoweringBlock(ITEM *const item)
{
    Sound_Effect(SFX_LOWERING_BLOCK, nullptr, SPM_NORMAL);
    Room_SetFlipEffect(-1);
}

REGISTER_ITEM_ACTION(ITEM_ACTION_CHAIN_BLOCK, M_ChainBlock)
REGISTER_ITEM_ACTION(ITEM_ACTION_FLOOD, M_Flood)
REGISTER_ITEM_ACTION(ITEM_ACTION_EXPLOSION, M_Explosion)
REGISTER_ITEM_ACTION(ITEM_ACTION_EARTHQUAKE, M_Earthquake)
REGISTER_ITEM_ACTION(ITEM_ACTION_FLICKER, M_Flicker)
REGISTER_ITEM_ACTION(ITEM_ACTION_FLOOR_SHAKE, M_FloorShake)
REGISTER_ITEM_ACTION(ITEM_ACTION_RAISING_BLOCK, M_RaisingBlock)
REGISTER_ITEM_ACTION(ITEM_ACTION_POWER_UP, M_PowerUp)
REGISTER_ITEM_ACTION(ITEM_ACTION_STAIRS_TO_SLOPE, M_Stairs2Slope)
REGISTER_ITEM_ACTION(ITEM_ACTION_DROP_SAND, M_DropSand)
REGISTER_ITEM_ACTION(ITEM_ACTION_CHANDELIER, M_Chandelier)
REGISTER_ITEM_ACTION(ITEM_ACTION_RUBBLE, M_Rubble)
REGISTER_ITEM_ACTION(ITEM_ACTION_PISTON, M_Piston)
REGISTER_ITEM_ACTION(ITEM_ACTION_CURTAIN, M_Curtain)
REGISTER_ITEM_ACTION(ITEM_ACTION_SET_CHANGE, M_SetChange)
REGISTER_ITEM_ACTION(ITEM_ACTION_STATUE, M_Statue)
REGISTER_ITEM_ACTION(ITEM_ACTION_BOILER, M_Boiler)
REGISTER_ITEM_ACTION(ITEM_ACTION_CAMERA_SHAKE, M_CameraShake)
REGISTER_ITEM_ACTION(ITEM_ACTION_LOWERING_BLOCK, M_LoweringBlock)
