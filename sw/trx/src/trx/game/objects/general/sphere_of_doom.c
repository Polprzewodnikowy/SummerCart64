#include <trx/core/math.h>
#include <trx/core/utils.h>
#include <trx/game/lara.h>
#include <trx/game/matrix.h>
#include <trx/game/output.h>
#include <trx/game/sound.h>

#define SPHERE_OF_DOOM_RADIUS (STEP_L * 5 / 2) // = 640

static void M_Collision(
    const int16_t item_num, ITEM *const lara_item, COLL_INFO *const coll)
{
    if (Room_Get(lara_item->room_num)->flags.underwater) {
        return;
    }

    const ITEM *const item = Item_Get(item_num);
    const int32_t dx = lara_item->pos.x - item->pos.x;
    const int32_t dz = lara_item->pos.z - item->pos.z;
    const int32_t radius = (SPHERE_OF_DOOM_RADIUS * item->timer) >> 8;

    if (SQUARE(dx) + SQUARE(dz) >= SQUARE(radius)) {
        return;
    }

    const int16_t angle = Math_Atan(dz, dx);
    const int16_t diff = lara_item->rot.y - angle;
    if (ABS(diff) < DEG_90) {
        lara_item->speed = 150;
        lara_item->rot.y = angle;
    } else {
        lara_item->speed = -150;
        lara_item->rot.y = angle + DEG_180;
    }

    lara_item->gravity = true;
    lara_item->fall_speed = -50;
    lara_item->pos.x =
        item->pos.x + (((radius + 50) * Math_Sin(angle)) >> W2V_SHIFT);
    lara_item->pos.z =
        item->pos.z + (((radius + 50) * Math_Cos(angle)) >> W2V_SHIFT);
    lara_item->rot.x = 0;
    lara_item->rot.z = 0;
    Item_SwitchToAnim(lara_item, LA(LA_FALL_START), 0);
    lara_item->current_anim_state = LS(LS_JUMP_FORWARD);
    lara_item->goal_anim_state = LS(LS_JUMP_FORWARD);
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    item->timer += 64;
    item->rot.y +=
        item->object_id == O_SPHERE_OF_DOOM_2 ? DEG_1 * 10 : -DEG_1 * 10;
    const ITEM *const lara_item = Lara_GetItem();
    const int32_t dx = item->pos.x - lara_item->pos.x;
    const int32_t dy = item->pos.y - lara_item->pos.y;
    const int32_t dz = item->pos.z - lara_item->pos.z;
    const int32_t radius = (SPHERE_OF_DOOM_RADIUS * item->timer) >> 8;
    const int32_t dist = Math_Sqrt(SQUARE(dx) + SQUARE(dy) + SQUARE(dz));
    XYZ_32 pos = lara_item->pos;
    pos.x += ((dist - radius) * dx) / radius;
    pos.y += ((dist - radius) * dy) / radius;
    pos.z += ((dist - radius) * dz) / radius;
    Sound_Effect(SFX_MARCO_BARTOLLI_TRANSFORM, &pos, SPM_NORMAL);
    if (item->timer > 60 * 64) {
        Item_Kill(item_num);
    }
}

static bool M_Draw(const ITEM *const item)
{
    const int32_t radius = item->timer << 6;
    Matrix_Push();
    Matrix_TranslateAbs32(item->interp.result.pos);
    Matrix_RotY(item->interp.result.rot.y);

    MATRIX *mptr = g_WMatrixPtr;
    mptr->_00 = ((int64_t)mptr->_00 * radius) >> W2V_SHIFT;
    mptr->_01 = ((int64_t)mptr->_01 * radius) >> W2V_SHIFT;
    mptr->_02 = ((int64_t)mptr->_02 * radius) >> W2V_SHIFT;
    mptr->_10 = ((int64_t)mptr->_10 * radius) >> W2V_SHIFT;
    mptr->_11 = ((int64_t)mptr->_11 * radius) >> W2V_SHIFT;
    mptr->_12 = ((int64_t)mptr->_12 * radius) >> W2V_SHIFT;
    mptr->_20 = ((int64_t)mptr->_20 * radius) >> W2V_SHIFT;
    mptr->_21 = ((int64_t)mptr->_21 * radius) >> W2V_SHIFT;
    mptr->_22 = ((int64_t)mptr->_22 * radius) >> W2V_SHIFT;

    mptr = g_MatrixPtr;
    mptr->_00 = ((int64_t)mptr->_00 * radius) >> W2V_SHIFT;
    mptr->_01 = ((int64_t)mptr->_01 * radius) >> W2V_SHIFT;
    mptr->_02 = ((int64_t)mptr->_02 * radius) >> W2V_SHIFT;
    mptr->_10 = ((int64_t)mptr->_10 * radius) >> W2V_SHIFT;
    mptr->_11 = ((int64_t)mptr->_11 * radius) >> W2V_SHIFT;
    mptr->_12 = ((int64_t)mptr->_12 * radius) >> W2V_SHIFT;
    mptr->_20 = ((int64_t)mptr->_20 * radius) >> W2V_SHIFT;
    mptr->_21 = ((int64_t)mptr->_21 * radius) >> W2V_SHIFT;
    mptr->_22 = ((int64_t)mptr->_22 * radius) >> W2V_SHIFT;

    const ANIM_FRAME *const frame_ptr = Item_GetAnim(item)->frame_ptr;
    const CLIP clip = Output_CheckBoundsClip(&frame_ptr->bounds);
    if (clip != CLIP_NOT_VISIBLE) {
        Output_CalculateObjectLighting(item, &frame_ptr->bounds);
        Object_DrawMesh(Object_Get(item->object_id)->mesh_idx, clip, false);
    }

    Matrix_Pop();
    return true;
}

static void M_SetupBase(OBJECT *const obj, const bool transparent)
{
    obj->collision_func = M_Collision;
    obj->control_func = M_Control;
    obj->draw_func = M_Draw;
    obj->save_position = true;
    obj->save_flags = true;
    obj->semi_transparent = transparent;
}

static void M_SetupTransparent(OBJECT *const obj)
{
    M_SetupBase(obj, true);
}

static void M_SetupOpaque(OBJECT *const obj)
{
    M_SetupBase(obj, false);
}

REGISTER_OBJECT(O_SPHERE_OF_DOOM_1, M_SetupTransparent)
REGISTER_OBJECT(O_SPHERE_OF_DOOM_2, M_SetupTransparent)
REGISTER_OBJECT(O_SPHERE_OF_DOOM_3, M_SetupOpaque)
