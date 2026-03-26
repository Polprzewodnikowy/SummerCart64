#include <trx/game/cutscene.h>

#include <trx/config.h>
#include <trx/core/memory.h>
#include <trx/core/utils.h>
#include <trx/debug.h>
#include <trx/game/camera.h>
#include <trx/game/collision.h>
#include <trx/game/const.h>
#include <trx/game/effects.h>
#include <trx/game/fx.h>
#include <trx/game/gun/misc.h>
#include <trx/game/gun/smoke.h>
#include <trx/game/input.h>
#include <trx/game/interpolation.h>
#include <trx/game/lara.h>
#include <trx/game/lua.h>
#include <trx/game/music.h>
#include <trx/game/output.h>
#include <trx/game/random.h>
#include <trx/game/shell.h>
#include <trx/game/sparks.h>
#include <trx/version.h>

static CAMERA_INFO m_LocalCamera = {};
static OBJECT_MESH **m_CapturedObjectMeshes = nullptr;
static OBJECT_ID *m_CapturedObjectMeshOwners = nullptr;
static int32_t m_CapturedObjectMeshCount = 0;
static bool m_DrawLeftGunFlash = false;
static bool m_DrawRightGunFlash = false;

typedef struct {
    bool is_valid;
    LARA_SKIN_TYPE skin_type;
    LARA_GUN_TYPE hand_l_type;
    LARA_GUN_TYPE hand_r_type;
    LARA_GUN_TYPE thigh_l_type;
    LARA_GUN_TYPE thigh_r_type;
    bool holsters_visible;
} M_LARA_CUTSCENE_STATE;

static M_LARA_CUTSCENE_STATE m_LaraCutsceneState = {};

static LARA_GUN_TYPE M_GetGunEquipmentType(const LARA_MESH mesh)
{
    const LARA_SKIN_EQUIPMENT *const equipment = Lara_Skin_GetEquipment(mesh);
    if (equipment->type == EQUIPMENT_TYPE_WEAPON) {
        return (LARA_GUN_TYPE)equipment->data;
    }
    return LGT_UNARMED;
}

static void M_CaptureLaraCutsceneState(void)
{
    m_LaraCutsceneState.is_valid = true;
    m_LaraCutsceneState.skin_type = Lara_Skin_GetType();
    m_LaraCutsceneState.hand_l_type = M_GetGunEquipmentType(LM_HAND_L);
    m_LaraCutsceneState.hand_r_type = M_GetGunEquipmentType(LM_HAND_R);
    m_LaraCutsceneState.thigh_l_type = M_GetGunEquipmentType(LM_THIGH_L);
    m_LaraCutsceneState.thigh_r_type = M_GetGunEquipmentType(LM_THIGH_R);
    m_LaraCutsceneState.holsters_visible = Lara_Skin_AreHolstersVisible();
}

static void M_CaptureObjectMeshesState(void)
{
    Memory_FreePointer(&m_CapturedObjectMeshes);
    Memory_FreePointer(&m_CapturedObjectMeshOwners);
    m_CapturedObjectMeshCount = Object_GetMeshCount();
    if (m_CapturedObjectMeshCount <= 0) {
        return;
    }

    m_CapturedObjectMeshes = Memory_Alloc(
        m_CapturedObjectMeshCount * sizeof(*m_CapturedObjectMeshes));
    m_CapturedObjectMeshOwners = Memory_Alloc(
        m_CapturedObjectMeshCount * sizeof(*m_CapturedObjectMeshOwners));
    for (int32_t i = 0; i < m_CapturedObjectMeshCount; i++) {
        m_CapturedObjectMeshes[i] = Object_GetMesh(i);
        m_CapturedObjectMeshOwners[i] = NO_OBJECT;
    }

    for (OBJECT_ID obj_id = O_FIRST; obj_id < O_NUMBER_OF; obj_id++) {
        const OBJECT *const obj = Object_Get(obj_id);
        if (!obj->loaded || obj->mesh_count <= 0 || obj->mesh_idx < 0) {
            continue;
        }

        for (int32_t mesh_idx = 0; mesh_idx < obj->mesh_count; mesh_idx++) {
            const int32_t abs_idx = obj->mesh_idx + mesh_idx;
            if (abs_idx >= 0 && abs_idx < m_CapturedObjectMeshCount) {
                m_CapturedObjectMeshOwners[abs_idx] = obj_id;
            }
        }
    }
}

static void M_RestoreObjectMeshesState(void)
{
    if (m_CapturedObjectMeshes == nullptr || m_CapturedObjectMeshCount <= 0) {
        return;
    }

    const int32_t mesh_count = Object_GetMeshCount();
    if (mesh_count != m_CapturedObjectMeshCount) {
        return;
    }

    for (int32_t i = 0; i < mesh_count; i++) {
        if (Object_GetMesh(i) == m_CapturedObjectMeshes[i]) {
            continue;
        }

        int32_t j = -1;
        for (int32_t k = i + 1; k < mesh_count; k++) {
            if (Object_GetMesh(k) == m_CapturedObjectMeshes[i]) {
                j = k;
                break;
            }
        }
        if (j < 0) {
            continue;
        }

        if (m_CapturedObjectMeshOwners[i] == NO_OBJECT
            || m_CapturedObjectMeshOwners[j] == NO_OBJECT) {
            continue;
        }

        const OBJECT *const obj_1 = Object_Get(m_CapturedObjectMeshOwners[i]);
        const OBJECT *const obj_2 = Object_Get(m_CapturedObjectMeshOwners[j]);
        Object_SwapMeshEx(
            m_CapturedObjectMeshOwners[i], m_CapturedObjectMeshOwners[j],
            i - obj_1->mesh_idx, j - obj_2->mesh_idx);
    }
}

static void M_RestoreLaraCutsceneState(void)
{
    if (!m_LaraCutsceneState.is_valid) {
        return;
    }

    Lara_Skin_SetType(m_LaraCutsceneState.skin_type);
    Lara_Skin_SetGunEquipment(LM_HAND_L, m_LaraCutsceneState.hand_l_type);
    Lara_Skin_SetGunEquipment(LM_HAND_R, m_LaraCutsceneState.hand_r_type);
    Lara_Skin_SetGunEquipment(LM_THIGH_L, m_LaraCutsceneState.thigh_l_type);
    Lara_Skin_SetGunEquipment(LM_THIGH_R, m_LaraCutsceneState.thigh_r_type);
    Lara_Skin_SetHolstersVisible(m_LaraCutsceneState.holsters_visible);
}

static bool M_IsCutsceneActor(const ITEM *const item)
{
    return (item->object_id >= O_PLAYER_1 && item->object_id <= O_PLAYER_10)
        || item->object_id == O_LARA;
}

static void M_ResetActorAnimation(ITEM *const item)
{
    Item_SwitchToAnim(item, 0, 0);
    item->prev_frame_num = item->frame_num;
    item->current_anim_state = Item_GetAnim(item)->current_anim_state;
    item->goal_anim_state = item->current_anim_state;
    item->required_anim_state = 0;
}

static void M_ResetActorsToStart(void)
{
    M_RestoreObjectMeshesState();
    M_RestoreLaraCutsceneState();

    for (int32_t i = 0; i < Item_GetTotalCount(); i++) {
        ITEM *const item = Item_Get(i);
        if (M_IsCutsceneActor(item)) {
            M_ResetActorAnimation(item);
        }
    }
}

static void M_ControlGun(void)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    const ITEM *const lara_item = Lara_GetItem();

    m_DrawLeftGunFlash = false;
    m_DrawRightGunFlash = false;
    if (lara->left_arm.flash_gun > 0) {
        m_DrawLeftGunFlash = true;
        lara->left_arm.flash_gun--;
    }
    if (lara->right_arm.flash_gun > 0) {
        m_DrawRightGunFlash = true;
        lara->right_arm.flash_gun--;
    }

    Gun_Smoke_Control();

    if (g_Config.visuals.enable_gun_lighting
        && (m_DrawLeftGunFlash || m_DrawRightGunFlash)) {
        XYZ_32 pos = { .x = -12, .y = 48, .z = 40 };
        LARA_MESH mesh = LM_HAND_L;
        if (m_DrawRightGunFlash) {
            pos.x = 8;
            mesh = LM_HAND_R;
        }

        Collide_GetJointAbsPosition(lara_item, &pos, mesh);
        pos.x += (Random_GetControl() & 0xFF) - 128;
        pos.y -= (Random_GetControl() & 0x7F) - 63;
        pos.z += (Random_GetControl() & 0xFF) - 128;
        if (g_TRVersion >= 3) {
            const RGB_888 color = {
                .r = 192 + (Random_GetControl() & 0x3F),
                .g = 144 + (Random_GetControl() & 0x1F),
                .b = Random_GetControl() & 0x3F,
            };
            Output_AddDynamicLightRGB(pos, 10, color);
        } else {
            Output_AddDynamicLight(pos, 10, 11);
        }
    }
}

static void M_DrawGunFlash(const LARA_MESH hand_mesh)
{
    const LARA_INFO *const lara = Lara_GetLaraInfo();
    XYZ_32 pos = {};
    const bool has_mesh_pos = Lara_GetMeshPos(hand_mesh, &pos);
    if (!has_mesh_pos) {
        const ITEM *const lara_item = Lara_GetItem();
        Collide_GetJointAbsPosition(lara_item, &pos, hand_mesh);
    }

    Matrix_Push();
    *g_MatrixPtr = g_ViewMatrix;
    *g_WMatrixPtr = g_IDMatrix;
    Matrix_TranslateAbs32(pos);
    if (has_mesh_pos && lara->mesh_pos_matrices_valid) {
        MATRIX hand_rot = lara->mesh_pos_matrices[hand_mesh];
        hand_rot._03 = 0;
        hand_rot._13 = 0;
        hand_rot._23 = 0;
        Matrix_Mul3x3(&hand_rot);
    }
    Gun_DrawFlash(LGT_PISTOLS, CLIP_FULLY_VISIBLE, false);
    Matrix_Pop();
}

static void M_Control(void)
{
    Output_ResetDynamicLights();
    Camera_UpdateCutscene();
    M_ControlGun();
    Item_Control();
    Effect_Control();
    Sparks_Control();
    FX_Control();
    Output_AnimateTextures(1);
    Lara_Hair_Control(true);
}

static void M_ReplayActors(
    CINE_DATA *const cine_data, const int32_t start_frame,
    const int32_t end_frame)
{
    for (int32_t frame_idx = start_frame; frame_idx < end_frame; frame_idx++) {
        Lua_FireEventInt32(LUA_EVENT_BEFORE_CONTROL, 0);
        cine_data->frame_idx = frame_idx;
        M_Control();
        Lua_FireEventInt32(LUA_EVENT_AFTER_CONTROL, 0);
    }
}

static void M_PlayerControl(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);

    CAMERA_INFO *const camera = Cutscene_GetCamera();
    item->rot.y = camera->target_angle;
    item->pos = camera->pos.pos;

    XYZ_32 pos = {};
    Collide_GetJointAbsPosition(item, &pos, 0);

    int16_t room_num = Room_GetIndexFromPos(pos);
    if (room_num != NO_ROOM) {
        Item_UpdateRoom(item_num, room_num);
    }

    int16_t floor_room_num = item->room_num;
    const SECTOR *const sector = Room_GetSector(pos, &floor_room_num);
    const int32_t height = Room_GetHeight(sector, pos);
    item->floor = height == NO_HEIGHT ? pos.y : height;

    Lara_Animate(item);
}

static void M_InitialisePlayer(const int16_t item_num)
{
    OBJECT *const obj = Object_Get(O_LARA);
    obj->draw_func = Lara_Draw;
    obj->control_func = M_PlayerControl;
    obj->shadow_size = (UNIT_SHADOW * 10) / 16;

    Item_AddActive(item_num);
    ITEM *const item = Item_Get(item_num);
    CAMERA_INFO *const camera = Cutscene_GetCamera();
    Camera_GetCineData()->position.target_angle = item->rot.y;
    g_Camera.pos.room_num = item->room_num;
    g_Camera.target_angle = item->rot.y;

    CINE_DATA *const cine_data = Camera_GetCineData();
    cine_data->position.pos = item->pos;

    camera->pos.pos = item->pos;
    if (item->room_num != NO_ROOM) {
        camera->pos.room_num = item->room_num;
    }
    camera->target_angle = item->rot.y;

    item->rot.y = 0;
    item->dynamic_light = false;
    Item_SwitchToAnim(item, 0, 0);
    item->goal_anim_state = 0;
    item->current_anim_state = 0;

    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->hit_direction = DIR_UNKNOWN;
}

static void M_Skip(const int32_t frames)
{
    CINE_DATA *const cine_data = Camera_GetCineData();
    const int32_t source_frame = cine_data->frame_idx;
    int32_t target_frame = source_frame + frames;
    CLAMP(target_frame, 0, cine_data->frame_count - 1);

    if (target_frame == source_frame) {
        return;
    }

    if (target_frame > source_frame) {
        M_ReplayActors(cine_data, source_frame, target_frame);
    } else {
        Lua_ReloadLevelScript();
        M_ResetActorsToStart();
        M_ReplayActors(cine_data, 0, target_frame);
    }

    cine_data->frame_idx = target_frame;
    Camera_UpdateCutscene();
}

bool Cutscene_Start(const int32_t level_num)
{
    const GF_LEVEL *const level = GF_GetLevel(GFLT_CUTSCENES, level_num);
    ASSERT(GF_GetCurrentLevel() == level);

    m_DrawLeftGunFlash = false;
    m_DrawRightGunFlash = false;
    M_InitialisePlayer(Item_GetIndex(Lara_GetItem()));
    M_CaptureLaraCutsceneState();
    M_CaptureObjectMeshesState();
    Camera_GetCineData()->frame_idx = 0;

    if (level->music_track != MX_INACTIVE) {
        Music_Play_Direct(level->music_track, MPM_ONCE);
    }

    return true;
}

void Cutscene_End(void)
{
    m_DrawLeftGunFlash = false;
    m_DrawRightGunFlash = false;
    Memory_FreePointer(&m_CapturedObjectMeshes);
    Memory_FreePointer(&m_CapturedObjectMeshOwners);
    m_CapturedObjectMeshCount = 0;
    Music_Stop();
}

GF_COMMAND Cutscene_Control(void)
{
    Interpolation_Remember();
    Music_SyncTimestamp(Camera_GetCineData()->frame_idx / (double)LOGIC_FPS);

    Input_Update();
    Shell_ProcessInput();
    if (g_InputDB.menu_confirm || g_InputDB.menu_back) {
        return (GF_COMMAND) { .action = GF_LEVEL_COMPLETE };
    } else if (g_InputDB.pause) {
        const GF_COMMAND gf_cmd = GF_PauseGame();
        if (gf_cmd.action != GF_NOOP) {
            return gf_cmd;
        }
    } else if (g_InputDB.toggle_photo_mode) {
        const GF_COMMAND gf_cmd = GF_EnterPhotoMode();
        if (gf_cmd.action != GF_NOOP) {
            return gf_cmd;
        }
    } else if (g_InputDB.menu_right || g_InputDB.menu_left) {
        const int32_t dir = g_InputDB.menu_right ? 1 : -1;
        const int32_t speed = g_Input.draw ? 15 : (g_Input.slow ? 1 : 5);
        M_Skip(dir * LOGIC_FPS * speed);
    }

    M_Control();

    CINE_DATA *const cine_data = Camera_GetCineData();
    cine_data->frame_idx++;
    if (cine_data->frame_idx >= cine_data->frame_count) {
        // Remember the scene after the update to prevent the interpolation
        // from twitching the camera back and forth.
        Interpolation_Remember();

        return (GF_COMMAND) { .action = GF_LEVEL_COMPLETE };
    }

    return (GF_COMMAND) { .action = GF_NOOP };
}

void Cutscene_Draw(void)
{
    Interpolation_Interpolate();
    Camera_Apply();
    Room_DrawAllRooms(g_Camera.interp.room_num, g_Camera.target.room_num);
    if (m_DrawLeftGunFlash) {
        M_DrawGunFlash(LM_HAND_L);
    }
    if (m_DrawRightGunFlash) {
        M_DrawGunFlash(LM_HAND_R);
    }
    SceneCompositor_Flush();
    if (g_Config.visuals.enable_reflections) {
        Output_Textures_UpdateEnvironmentMap();
    }
}

CAMERA_INFO *Cutscene_GetCamera(void)
{
    return &m_LocalCamera;
}
