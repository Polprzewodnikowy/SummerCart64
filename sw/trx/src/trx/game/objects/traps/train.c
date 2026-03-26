#include <trx/game/camera.h>
#include <trx/game/lara.h>
#include <trx/game/objects.h>
#include <trx/game/output.h>
#include <trx/game/rooms.h>
#include <trx/game/sound.h>
#include <trx/game/spawn.h>

// clang-format off
#define M_DEFAULT_SPEED 260
#define M_HIT_SPEED     160
#define M_BRAKE_SPEED   48
#define M_FRONT_DIST    (WALL_L * 5) // = 5120
#define M_LIGHT_DIST    (WALL_L * 3) // = 3072
#define M_CAM_DIST      (WALL_L * 8) // = 8192
// clang-format on

typedef struct {
    int32_t max_speed;
} M_PRIV;

static void M_Initialise(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;
    const ANIM *const anim = Item_GetAnim(item);
    p->max_speed = anim->velocity != 0 ? anim->velocity : M_DEFAULT_SPEED;
    item->speed = p->max_speed;
}

static int32_t M_GetHeight(
    const ITEM *const item, const int32_t x, const int32_t z,
    int16_t *const room_num)
{
    XYZ_32 pos = item->pos;
    const int32_t sy = Math_Sin(item->rot.y);
    const int32_t cy = Math_Cos(item->rot.y);
    const int32_t sx = Math_Sin(item->rot.x);
    const int32_t sz = Math_Sin(item->rot.z);

    pos.x += (z * sy + x * cy) >> W2V_SHIFT;
    pos.z += (z * cy - x * sy) >> W2V_SHIFT;
    pos.y = ((x * sz) >> W2V_SHIFT) + (item->pos.y - ((z * sx) >> W2V_SHIFT));

    *room_num = item->room_num;
    const SECTOR *const sector = Room_GetSector(pos, room_num);
    return Room_GetHeight(sector, pos);
}

static void M_Collision(
    const int16_t item_num, ITEM *const lara_item, COLL_INFO *const coll)
{
    ITEM *const train_item = Item_Get(item_num);
    if (train_item->status != IS_ACTIVE) {
        Object_Collision(item_num, lara_item, coll);
        return;
    }

    if (!Item_TestBoundsCollide(train_item, lara_item, coll->radius)) {
        return;
    }

    if (!Collide_TestCollision(train_item, lara_item)) {
        return;
    }

    Sound_Effect(SFX_LARA_GENERAL_DEATH, &lara_item->pos, SPM_ALWAYS);
    Sound_Effect(SFX_LARA_FALL_DEATH, &lara_item->pos, SPM_ALWAYS);
    Sound_StopEffect(SFX_TRAIN_LOOP);

    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara_item->hit_points = 0;
    lara_item->rot.y = train_item->rot.y;
    lara->move_angle = lara_item->rot.y;
    lara_item->gravity = false;
    lara_item->fall_speed = 0;
    lara_item->speed = 0;
    lara->gun_type = LGT_UNARMED;
    Lara_SwitchToExtraState(LS_EXTRA_TRAIN_KILL);

    if (train_item->speed != 0) {
        train_item->speed = M_HIT_SPEED;
    }
    XYZ_32 pos = XYZ_32_OffsetYaw(lara_item->pos, lara_item->rot.y, STEP_L);
    pos.y -= STEP_L * 2;
    Spawn_BloodBath(
        pos.x, pos.y, pos.z, WALL_L, lara_item->rot.y, lara_item->room_num, 15);
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    if (!Item_IsTriggerActive(item)) {
        return;
    }

    item->pos = XYZ_32_OffsetYaw(item->pos, item->rot.y, item->speed);

    int16_t room_num = NO_ROOM;
    const int32_t front_height = M_GetHeight(item, 0, M_FRONT_DIST, &room_num);
    const int32_t mid_height = M_GetHeight(item, 0, 0, &room_num);
    item->pos.y = mid_height;

    if (item->pos.y == NO_HEIGHT) {
        Item_Kill(item_num);
        return;
    }

    item->pos.y -= 32;
    room_num = item->room_num;
    Room_GetSector(item->pos, &room_num);
    Item_UpdateRoom(item_num, room_num);

    item->rot.x = (mid_height - front_height) << 1;

    const XYZ_32 light_pos =
        XYZ_32_OffsetYaw(item->pos, item->rot.y, M_LIGHT_DIST);
    Output_AddDynamicLightRGB(light_pos, 14, (RGB_888) { 0xFF, 0xFF, 0xFF });

    const M_PRIV *const p = item->priv;
    if (item->speed == p->max_speed) {
        Sound_Effect(SFX_TRAIN_LOOP, &item->pos, SPM_ALWAYS);
        return;
    }

    if (item->speed == M_HIT_SPEED) {
        XYZ_32 cam_pos = XYZ_32_OffsetYaw(item->pos, item->rot.y, M_CAM_DIST);
        cam_pos.y -= STEP_L * 2;
        const SECTOR *const sector = Room_GetSector(cam_pos, &room_num);
        cam_pos.y = Room_GetHeight(sector, cam_pos);
        Camera_UpdateDynamicFixedObject(cam_pos, item->room_num);
    }

    item->speed -= M_BRAKE_SPEED;
    CLAMPL(item->speed, 0);
}

static void M_Setup(OBJECT *const obj)
{
    if (!obj->loaded) {
        return;
    }

    obj->initialise_func = M_Initialise;
    obj->collision_func = M_Collision;
    obj->control_func = M_Control;
    obj->priv_size = sizeof(M_PRIV);
    obj->save_position = true;
    obj->save_flags = true;
    obj->save_anim = true;
}

REGISTER_OBJECT(O_TRAIN, M_Setup)
