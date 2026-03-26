#include <trx/game/fx/gun_flash.h>

#include <trx/config.h>
#include <trx/core/colors.h>
#include <trx/core/math.h>
#include <trx/game/collision.h>
#include <trx/game/items.h>
#include <trx/game/matrix.h>
#include <trx/game/objects.h>
#include <trx/game/output.h>
#include <trx/game/random.h>
#include <trx/version.h>

#define M_MAX_FLASHES 32
#define M_LIFETIME 3
#define M_AXIS_UNIT 1024

typedef struct {
    bool active;
    int16_t owner_item_num;
    int16_t room_num;
    int16_t lifetime;
    XZ_16 rot;
    BITE bite;
    OBJECT_ID flash_object_id;
    XYZ_32 light_pos;
} M_GUN_FLASH;

typedef struct {
    M_GUN_FLASH flashes[M_MAX_FLASHES];
    int32_t next_idx;
} M_PRIV;

static M_PRIV m_Priv = {};

static int16_t M_GetRandomRoll(void)
{
    const int16_t rnd = (int16_t)Random_GetControl();
    return (int16_t)((rnd << 14) + (rnd >> 2) - 4096);
}

static bool M_GetOwnerItem(
    const M_GUN_FLASH *const flash, ITEM **const out_item)
{
    if (flash->owner_item_num < 0
        || flash->owner_item_num >= Item_GetTotalCount()) {
        return false;
    }
    *out_item = Item_Get(flash->owner_item_num);
    return *out_item != nullptr;
}

static void M_GetJointPose(
    const ITEM *const item, const BITE bite, XYZ_32 *const out_pos,
    MATRIX *const out_rot)
{
    XYZ_32 pos = bite.pos;
    Collide_GetJointAbsPosition(item, &pos, bite.mesh_num);

    XYZ_32 axis_x = bite.pos;
    axis_x.x += M_AXIS_UNIT;
    Collide_GetJointAbsPosition(item, &axis_x, bite.mesh_num);

    XYZ_32 axis_y = bite.pos;
    axis_y.y += M_AXIS_UNIT;
    Collide_GetJointAbsPosition(item, &axis_y, bite.mesh_num);

    XYZ_32 axis_z = bite.pos;
    axis_z.z += M_AXIS_UNIT;
    Collide_GetJointAbsPosition(item, &axis_z, bite.mesh_num);

    *out_pos = pos;
    *out_rot = g_IDMatrix;

    out_rot->_00 = ((int64_t)(axis_x.x - pos.x) << W2V_SHIFT) / M_AXIS_UNIT;
    out_rot->_10 = ((int64_t)(axis_x.y - pos.y) << W2V_SHIFT) / M_AXIS_UNIT;
    out_rot->_20 = ((int64_t)(axis_x.z - pos.z) << W2V_SHIFT) / M_AXIS_UNIT;

    out_rot->_01 = ((int64_t)(axis_y.x - pos.x) << W2V_SHIFT) / M_AXIS_UNIT;
    out_rot->_11 = ((int64_t)(axis_y.y - pos.y) << W2V_SHIFT) / M_AXIS_UNIT;
    out_rot->_21 = ((int64_t)(axis_y.z - pos.z) << W2V_SHIFT) / M_AXIS_UNIT;

    out_rot->_02 = ((int64_t)(axis_z.x - pos.x) << W2V_SHIFT) / M_AXIS_UNIT;
    out_rot->_12 = ((int64_t)(axis_z.y - pos.y) << W2V_SHIFT) / M_AXIS_UNIT;
    out_rot->_22 = ((int64_t)(axis_z.z - pos.z) << W2V_SHIFT) / M_AXIS_UNIT;

    out_rot->_03 = 0;
    out_rot->_13 = 0;
    out_rot->_23 = 0;
}

bool FX_GunFlash_Spawn(
    const ITEM *const owner_item, const CREATURE_GUN *const gun)
{
    if (owner_item == nullptr || gun == nullptr || !gun->tr3_enemy_flash) {
        return false;
    }

    M_GUN_FLASH *const flash = &m_Priv.flashes[m_Priv.next_idx];
    flash->active = true;
    flash->owner_item_num = Item_GetIndex(owner_item);
    flash->room_num = owner_item->room_num;
    flash->lifetime = M_LIFETIME;
    flash->rot = (XZ_16) { .x = gun->tr3_flash_rot_x, .z = M_GetRandomRoll() };
    flash->bite = gun->tr3_flash;
    flash->flash_object_id =
        (gun->tr3_enemy_weapon_flags & 1) != 0 ? O_M16_FLASH : O_GUN_FLASH;
    flash->light_pos = owner_item->pos;

    m_Priv.next_idx = (m_Priv.next_idx + 1) % M_MAX_FLASHES;
    return true;
}

void FX_GunFlash_Control(void)
{
    for (int32_t i = 0; i < M_MAX_FLASHES; i++) {
        M_GUN_FLASH *const flash = &m_Priv.flashes[i];
        if (!flash->active) {
            continue;
        }

        ITEM *owner_item = nullptr;
        if (!M_GetOwnerItem(flash, &owner_item)) {
            flash->active = false;
            continue;
        }

        flash->room_num = owner_item->room_num;
        flash->rot.z = M_GetRandomRoll();

        XYZ_32 light_pos = flash->bite.pos;
        Collide_GetJointAbsPosition(
            owner_item, &light_pos, flash->bite.mesh_num);
        flash->light_pos = light_pos;

        if (g_Config.visuals.enable_gun_lighting) {
            const int32_t falloff = (flash->lifetime << 1) + 8;
            if (g_TRVersion >= 3) {
                Output_AddDynamicLightRGB(
                    flash->light_pos, falloff, (RGB_888) { 192, 128, 32 });
            } else {
                Output_AddDynamicLight(flash->light_pos, falloff, 11);
            }
        }

        flash->lifetime--;
        if (flash->lifetime <= 0) {
            flash->active = false;
        }
    }
}

void FX_GunFlash_Draw(void)
{
    const OBJECT *const glow_obj = Object_Get(O_GLOW);

    for (int32_t i = 0; i < M_MAX_FLASHES; i++) {
        const M_GUN_FLASH *const flash = &m_Priv.flashes[i];
        if (!flash->active) {
            continue;
        }

        ITEM *owner_item = nullptr;
        if (!M_GetOwnerItem(flash, &owner_item)) {
            continue;
        }

        const OBJECT *const flash_obj = Object_Get(flash->flash_object_id);
        if (!flash_obj->loaded) {
            continue;
        }

        XYZ_32 flash_pos = {};
        MATRIX flash_rot = {};
        M_GetJointPose(owner_item, flash->bite, &flash_pos, &flash_rot);

        if (glow_obj->loaded) {
            Output_DrawSprite(
                flash_pos.x, flash_pos.y, flash_pos.z, glow_obj->mesh_idx,
                SHADE_NEUTRAL, (RGB_F) { 1.0f, 0.89f, 0.13f }, DRAW_BLEND_ADD);
        }

        Matrix_Push();
        *g_MatrixPtr = g_ViewMatrix;
        *g_WMatrixPtr = g_IDMatrix;
        Matrix_TranslateAbs32(flash_pos);
        Matrix_Mul3x3(&flash_rot);
        Matrix_RotX(flash->rot.x);
        Matrix_RotZ(flash->rot.z);
        Output_CalculateStaticLightRGB_F((RGB_F) { 1.0f, 0.89f, 0.13f });
        Object_DrawMesh(flash_obj->mesh_idx, -1, false);
        Matrix_Pop();
    }
}
