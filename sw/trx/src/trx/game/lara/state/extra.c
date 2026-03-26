#include <trx/game/camera.h>
#include <trx/game/collision.h>
#include <trx/game/game.h>
#include <trx/game/inventory.h>
#include <trx/game/lara.h>
#include <trx/game/lara/util.h>
#include <trx/game/music.h>
#include <trx/game/objects/effects/twinkle.h>
#include <trx/game/output/state.h>
#include <trx/game/overlay.h>
#include <trx/game/random.h>
#include <trx/game/rooms.h>
#include <trx/game/sparks/spawners.h>
#include <trx/game/stats.h>
#include <trx/game/viewport.h>

// clang-format off
#define M_LF_PICKUP_SCION              44
#define M_LF_PICKUP_GOLD_BAR           113
#define M_LF_SHARK_DEATH_END           56
#define M_LF_SHARK_DEATH_TIMER_DELAY   25
#define M_LF_TREX_DEATH_TIMER_DELAY    45
#define M_LF_YETI_DEATH_TIMER_DELAY    70
#define M_LF_DRAGON_DAGGER_PULLED      1
#define M_LF_DRAGON_DAGGER_STORED      180
#define M_LF_DRAGON_DAGGER_DISPLAY     210
#define M_LF_DRAGON_DAGGER_ANIM_END    239
#define M_LF_START_HOUSE_BEGIN         1
#define M_LF_START_HOUSE_DAGGER_STORED 401
#define M_LF_START_HOUSE_END           427
#define M_LF_SHOWER_START              1
#define M_LF_SHOWER_SHOTGUN_PICKUP     316
#define M_CAM_YETI_KILL_ANGLE         (160 * DEG_1) // = 29120
#define M_CAM_YETI_KILL_DISTANCE      (3 * WALL_L)  // = 3072
#define M_CAM_SHARK_KILL_ANGLE        (160 * DEG_1) // = 29120
#define M_CAM_SHARK_KILL_DISTANCE     (3 * WALL_L)  // = 3072
#define M_CAM_AIRLOCK_ANGLE           (80 * DEG_1)  // = 14560
#define M_CAM_AIRLOCK_ELEVATION       (-25 * DEG_1) // = -4550
#define M_CAM_GONG_BONG_ANGLE         (-25 * DEG_1) // = -4550
#define M_CAM_GONG_BONG_ELEVATION     (-20 * DEG_1) // = -3640
#define M_CAM_GONG_BONG_DISTANCE      (3 * WALL_L)  // = 3072
#define M_CAM_BEAST_KILL_ANGLE        (170 * DEG_1) // = 30940
#define M_CAM_BEAST_KILL_ELEVATION    (-25 * DEG_1) // = -4550
#define M_CAM_TORSO_KILL_DISTANCE     (2 * WALL_L)  // = 2048
// clang-format on

typedef struct {
    int16_t frame_idx;
    LARA_MESH mesh;
} M_MIDAS_STEP;

static const M_MIDAS_STEP m_MidasSteps[] = {
    { .frame_idx = 5, .mesh = LM_FOOT_L },
    { .frame_idx = 5, .mesh = LM_FOOT_R },
    { .frame_idx = 70, .mesh = LM_CALF_L },
    { .frame_idx = 90, .mesh = LM_THIGH_L },
    { .frame_idx = 100, .mesh = LM_CALF_R },
    { .frame_idx = 120, .mesh = LM_HIPS },
    { .frame_idx = 120, .mesh = LM_THIGH_R },
    { .frame_idx = 135, .mesh = LM_TORSO },
    { .frame_idx = 150, .mesh = LM_UARM_L },
    { .frame_idx = 163, .mesh = LM_LARM_L },
    { .frame_idx = 174, .mesh = LM_HAND_L },
    { .frame_idx = 186, .mesh = LM_UARM_R },
    { .frame_idx = 195, .mesh = LM_LARM_R },
    { .frame_idx = 218, .mesh = LM_HAND_R },
    { .frame_idx = 225, .mesh = LM_HEAD },
};

static void M_ScionPedestal(ITEM *const item, COLL_INFO *const coll)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    if (!Item_TestFrameEqual(item, M_LF_PICKUP_SCION)
        || lara->interact_target.item_num == NO_ITEM) {
        return;
    }

    ITEM *const scion = Item_Get(lara->interact_target.item_num);
    const ITEM_ACTION action = ItemAction_ToGameID(ITEM_ACTION_FINISH_LEVEL);
    if (!Anim_HasFXCommand(Item_GetAnim(item), action)) {
        Overlay_AddDisplayPickup(scion->object_id);
    }

    Inv_AddItem(scion->object_id);
    scion->status = IS_INVISIBLE;
    Item_RemoveDrawn(lara->interact_target.item_num);
    Stats_AddPickup();
    lara->interact_target.item_num = NO_ITEM;
}

static void M_UseMidas(ITEM *const item, COLL_INFO *const coll)
{
    coll->enable_hit = 0;
    coll->enable_baddie_push = 0;
    Twinkle_SparkleItem(item, (1 << LM_HAND_L) | (1 << LM_HAND_R));

    if (Item_TestFrameEqual(item, M_LF_PICKUP_GOLD_BAR)) {
        Overlay_AddDisplayPickup(O_PUZZLE_ITEM_1);
        Inv_RemoveItem(O_LEADBAR_ITEM);
        Inv_AddItem(O_PUZZLE_ITEM_1);
        LARA_INFO *const lara = Lara_GetLaraInfo();
        lara->interact_target.item_num = NO_ITEM;
    }
}

static void M_MidasKill(ITEM *const item, COLL_INFO *const coll)
{
    item->gravity = false;
    coll->enable_hit = 0;
    coll->enable_baddie_push = 0;

    LARA_INFO *const lara = Lara_GetLaraInfo();
    const int32_t frame_num = Item_GetRelativeFrame(item);

    for (size_t i = 0; i < ARRAY_SIZE(m_MidasSteps); i++) {
        const M_MIDAS_STEP *const step = &m_MidasSteps[i];
        if (step->frame_idx > frame_num) {
            continue;
        }

        lara->mesh_effects |= (1 << step->mesh);
        Lara_Skin_SwapSingleExtra(step->mesh, LS_EXTRA_MIDAS_KILL);
        switch (step->mesh) {
        case LM_TORSO:
        case LM_HAND_L:
        case LM_HAND_R:
            Lara_Skin_ClearEquipment(step->mesh);
            break;
        default:
            break;
        }
    }

    Twinkle_SparkleItem(item, lara->mesh_effects);
}

static void M_Breath(ITEM *const item, COLL_INFO *const coll)
{
    Item_SwitchToAnim(item, LA(LA_STAND_IDLE), 0);
    item->goal_anim_state = LS(LS_STOP);
    item->current_anim_state = LS(LS_STOP);
    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->extra_anim = false;
    lara->gun_status = LGS_ARMLESS;
    if (g_Camera.type != CAM_HEAVY) {
        g_Camera.type = CAM_CHASE;
    }
    Viewport_AlterFOV(-1, FOV_MODE_GAME);
}

static void M_YetiKill(ITEM *const item, COLL_INFO *const coll)
{
    g_Camera.target_angle = M_CAM_YETI_KILL_ANGLE;
    g_Camera.target_distance = M_CAM_YETI_KILL_DISTANCE;
    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->hit_direction = -1;
    if (Item_TestFrameRange(item, 0, M_LF_YETI_DEATH_TIMER_DELAY)) {
        lara->death_timer = 1;
    }
}

static void M_SharkKill(ITEM *const item, COLL_INFO *const coll)
{
    g_Camera.target_angle = M_CAM_SHARK_KILL_ANGLE;
    g_Camera.target_distance = M_CAM_SHARK_KILL_DISTANCE;
    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->hit_direction = -1;

    if (Item_TestFrameEqual(item, M_LF_SHARK_DEATH_END)) {
        const int32_t water_height =
            Room_GetWaterHeight(item->pos, item->room_num);
        if (water_height != NO_HEIGHT && water_height < item->pos.y - 100) {
            item->pos.y -= 5;
        }
    }

    if (Item_TestFrameRange(item, 0, M_LF_SHARK_DEATH_TIMER_DELAY)) {
        lara->death_timer = 1;
    }
}

static void M_Airlock(ITEM *const item, COLL_INFO *const coll)
{
    g_Camera.target_angle = M_CAM_AIRLOCK_ANGLE;
    g_Camera.target_elevation = M_CAM_AIRLOCK_ELEVATION;
}

static void M_GongBong(ITEM *const item, COLL_INFO *const coll)
{
    g_Camera.target_angle = M_CAM_GONG_BONG_ANGLE;
    g_Camera.target_elevation = M_CAM_GONG_BONG_ELEVATION;
    g_Camera.target_distance = M_CAM_GONG_BONG_DISTANCE;
}

static void M_BeastKill(ITEM *const item, COLL_INFO *const coll)
{
    g_Camera.flags = CF_FOLLOW_CENTRE;
    g_Camera.target_angle = M_CAM_BEAST_KILL_ANGLE;
    g_Camera.target_elevation = M_CAM_BEAST_KILL_ELEVATION;
    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->hit_direction = DIR_UNKNOWN;

    if (item->current_anim_state == LS_EXTRA_TREX_KILL) {
        if (Item_TestFrameRange(item, 0, M_LF_TREX_DEATH_TIMER_DELAY)) {
            lara->death_timer = 1;
        }
    } else if (item->current_anim_state == LS_EXTRA_TORSO_KILL) {
        g_Camera.target_distance = M_CAM_TORSO_KILL_DISTANCE;
    }
}

static void M_RapidsDrown(ITEM *const item, COLL_INFO *const coll)
{
    Collide_GetCollisionInfo(
        coll, item->pos.x, item->pos.y, item->pos.z, item->room_num,
        LARA_HEIGHT);

    int16_t room_num = item->room_num;
    const SECTOR *const sector = Room_GetSector(item->pos, &room_num);
    item->pos.y = Room_GetHeight(sector, item->pos) + 384;

    item->rot.y += 1024;

    const int32_t time4 = (int32_t)Output_GetTimeInGame() * 4;
    if ((time4 & 3) == 0) {
        Sparks_TriggerWaterfallMist(
            item->pos.x, item->pos.y, item->pos.z,
            Random_GetControl() & 0x0FFF);
    }
}

static void M_PullDagger(ITEM *const item, COLL_INFO *const coll)
{
    if (Item_TestFrameEqual(item, M_LF_DRAGON_DAGGER_PULLED)) {
        Lara_Skin_SetExtraEquipment(LM_HAND_R, EXTRA_MESH_DAGGER_HAND);
        Music_Play(MX_DAGGER_PULL, MPM_ONCE);
    } else if (Item_TestFrameEqual(item, M_LF_DRAGON_DAGGER_STORED)) {
        Lara_Skin_ClearEquipment(LM_HAND_R);
        Inv_AddItem(O_PUZZLE_ITEM_2);
        Stats_AddPickup();
    } else if (Item_TestFrameEqual(item, M_LF_DRAGON_DAGGER_DISPLAY)) {
        Overlay_AddDisplayPickup(O_PUZZLE_ITEM_2);
    } else if (Item_TestFrameEqual(item, M_LF_DRAGON_DAGGER_ANIM_END)) {
        item->rot.y += DEG_90;
    }
}

static void M_StartAnim(ITEM *const item, COLL_INFO *const coll)
{
    Room_TestTriggers(item);
}

static void M_StartHouse(ITEM *const item, COLL_INFO *const coll)
{
    if (Item_TestFrameEqual(item, M_LF_START_HOUSE_BEGIN)) {
        Music_Play(MX_REVEAL_2, MPM_ONCE);
        Lara_Skin_SetExtraEquipment(LM_HAND_R, EXTRA_MESH_DAGGER_HAND);
    } else if (Item_TestFrameEqual(item, M_LF_START_HOUSE_DAGGER_STORED)) {
        Lara_Skin_ClearEquipment(LM_HAND_R);
        Lara_Skin_SetExtraEquipment(LM_HIPS, EXTRA_MESH_DAGGER_HIPS);
        Inv_AddItem(O_PUZZLE_ITEM_1);
    } else if (Item_TestFrameEqual(item, M_LF_START_HOUSE_END)) {
        g_Camera.type = CAM_CHASE;
        Viewport_AlterFOV(-1, FOV_MODE_GAME);
    }
}

static void M_EndHouse(ITEM *const item, COLL_INFO *const coll)
{
    item->hit_points = LARA_MAX_HITPOINTS;
    Lara_SetControllable(false);

    if (Item_TestFrameEqual(item, M_LF_SHOWER_START)) {
        LARA_INFO *const lara = Lara_GetLaraInfo();
        Lara_Skin_ClearEquipment(LM_TORSO);
        Lara_Skin_SetCombatFace(false);
        Lara_Skin_ClearEquipment(LM_HAND_R);
        Lara_Skin_ClearEquipment(LM_HIPS);
        Music_Play(MX_CUTSCENE_BATH, MPM_ONCE);
    } else if (Item_TestFrameEqual(item, M_LF_SHOWER_SHOTGUN_PICKUP)) {
        Lara_Skin_SetGunEquipment(LM_HAND_R, LGT_SHOTGUN);
    } else if (Item_TestFrameEqual(item, -1)) {
        Game_SetIsLevelComplete(true);
    }

    if (Music_GetCurrentPlayingTrack() == Music_ToGameID(MX_CUTSCENE_BATH)) {
        const int32_t frame_num = Item_GetRelativeFrame(item);
        const double ts = (frame_num - M_LF_SHOWER_START) / (double)LOGIC_FPS;
        Music_SyncTimestamp(ts);
    }
}

static void M_TrainKill(ITEM *const item, COLL_INFO *const coll)
{
    g_Camera.num = Camera_GetDynamicFixedObjectIdx();
    g_Camera.type = CAM_FIXED;
    g_Camera.speed = 1;

    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->hit_direction = DIR_UNKNOWN;
    item->gravity = false;
    item->hit_points = -1;

    int16_t room_num = item->room_num;
    const SECTOR *const sector = Room_GetSector(item->pos, &room_num);
    item->pos.y = Room_GetHeight(sector, item->pos);

    if (Item_TestFrameEqual(item, -30)) {
        lara->death_timer = 1;
    }
}

static void M_JailWakeUp(ITEM *const item, COLL_INFO *const coll)
{
    if (!Item_TestFrameEqual(item, -2)) {
        return;
    }

    Item_Animate(item);
    XYZ_32 pos = {};
    Lara_GetMeshPos(LM_HIPS, &pos);
    item->pos.x = pos.x;
    item->pos.z = pos.z;
    item->interp.prev.pos = item->pos;
    item->interp.prev.rot = item->rot;
}

// clang-format off
REGISTER_LARA_EXTRA(LS_EXTRA_BREATH,         M_Breath)
REGISTER_LARA_EXTRA(LS_EXTRA_SCION_PICKUP_1, M_ScionPedestal)
REGISTER_LARA_EXTRA(LS_EXTRA_USE_MIDAS,      M_UseMidas)
REGISTER_LARA_EXTRA(LS_EXTRA_MIDAS_KILL,     M_MidasKill)
REGISTER_LARA_EXTRA(LS_EXTRA_YETI_KILL,      M_YetiKill)
REGISTER_LARA_EXTRA(LS_EXTRA_GUARD_KILL,     M_YetiKill)
REGISTER_LARA_EXTRA(LS_EXTRA_SHARK_KILL,     M_SharkKill)
REGISTER_LARA_EXTRA(LS_EXTRA_AIRLOCK,        M_Airlock)
REGISTER_LARA_EXTRA(LS_EXTRA_GONG_BONG,      M_GongBong)
REGISTER_LARA_EXTRA(LS_EXTRA_TREX_KILL,      M_BeastKill)
REGISTER_LARA_EXTRA(LS_EXTRA_TORSO_KILL,     M_BeastKill)
REGISTER_LARA_EXTRA(LS_EXTRA_PULL_DAGGER,    M_PullDagger)
REGISTER_LARA_EXTRA(LS_EXTRA_START_ANIM,     M_StartAnim)
REGISTER_LARA_EXTRA(LS_EXTRA_START_HOUSE,    M_StartHouse)
REGISTER_LARA_EXTRA(LS_EXTRA_END_HOUSE,      M_EndHouse)
REGISTER_LARA_EXTRA(LS_EXTRA_SHIVA_KILL,     M_BeastKill)
REGISTER_LARA_EXTRA(LS_EXTRA_RAPIDS_DROWN,   M_RapidsDrown)
REGISTER_LARA_EXTRA(LS_EXTRA_TRAIN_KILL,     M_TrainKill)
REGISTER_LARA_EXTRA(LS_EXTRA_JAIL_WAKE_UP,   M_JailWakeUp)
// clang-format on
