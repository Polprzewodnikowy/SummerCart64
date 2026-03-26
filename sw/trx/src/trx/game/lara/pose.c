#include <trx/game/lara/pose.h>

#include <trx/core/filesystem.h>
#include <trx/core/json/util/file.h>
#include <trx/core/json/util/read_io.h>
#include <trx/core/memory.h>
#include <trx/core/strings.h>
#include <trx/core/vector.h>
#include <trx/debug.h>
#include <trx/game/game_flow.h>
#include <trx/game/interpolation.h>
#include <trx/game/items.h>
#include <trx/game/lara/hair.h>
#include <trx/game/objects.h>
#include <trx/game/shell.h>

#define M_NO_POSE (-1)

static VECTOR *m_Poses = nullptr;
static int32_t m_ActivePose = M_NO_POSE;

static void M_WarnWithJSONError(const JSON_READ_IO *const io)
{
    char warning_message[1024];
    JSON_ReadIO_FormatError(
        io, false, warning_message, sizeof(warning_message));
    LOG_WARNING("%s", warning_message);
}

static bool M_LoadPose(JSON_READ_IO *const io, LARA_POSE *const pose)
{
    JSON_MUST(JSON_READ(io, "offset", &pose->offset));
    JSON_MUST(JSON_PUSH(io, "rots"));
    const int32_t rot_count = JSON_ARRAY_LEN(io);
    if (rot_count < 0) {
        JSON_MUST(JSON_POP(io));
        JSON_FAIL();
    }
    if (rot_count != LM_NUMBER_OF) {
        JSON_ReadIO_SetError(
            io, "expected exactly %d rotations, got %d", LM_NUMBER_OF,
            rot_count);
        JSON_MUST(JSON_POP(io));
        JSON_FAIL();
    }

    for (int32_t i = 0; i < LM_NUMBER_OF; i++) {
        JSON_MUST(JSON_READ_A(io, i, &pose->rots[i]));
    }

    JSON_MUST(JSON_POP(io));
    JSON_FINISH();
}

static bool M_LoadPosesArray(JSON_READ_IO *const io, VECTOR *const poses)
{
    const int32_t pose_count = JSON_ARRAY_LEN(io);
    if (pose_count < 0) {
        JSON_FAIL();
    }

    for (int32_t i = 0; i < pose_count; i++) {
        JSON_MUST(JSON_PUSH_INDEX(io, i));

        LARA_POSE pose = {};
        if (JSON_SHOULD(M_LoadPose(io, &pose))) {
            Vector_Add(poses, &pose);
        }
        JSON_MUST(JSON_POP(io));
    }

    JSON_FINISH();
}

static void M_LoadPoses(void)
{
    m_Poses = Vector_Create(sizeof(LARA_POSE));
    ASSERT(m_Poses != nullptr);

    const char *const poses_path =
        TRXPath_TryResolve(TRX_DYNAMIC_PATH_COMMON_CONFIG, "poses.json5");
    if (poses_path == nullptr) {
        return;
    }
    JSON_VALUE *const doc = JSONFile_Read(poses_path);
    if (doc == nullptr) {
        return;
    }

    JSON_READ_IO *const io = JSON_ReadIO_Create(doc, 0, poses_path);
    if (!M_LoadPosesArray(io, m_Poses)) {
        M_WarnWithJSONError(io);
    }
    JSON_ReadIO_Destroy(io);
    JSON_ValueFree(doc);
}

void Lara_Pose_Init(void)
{
    if (m_Poses == nullptr) {
        M_LoadPoses();
    }
}

void Lara_Pose_Shutdown(void)
{
    if (m_Poses != nullptr) {
        Vector_Free(m_Poses);
        m_Poses = nullptr;
    }
}

bool Lara_Pose_IsAvailable(void)
{
    return m_Poses->count > 0 && Object_Get(O_LARA)->loaded
        && GF_GetCurrentLevel()->type != GFL_CUTSCENE;
}

void Lara_Pose_Clear(void)
{
    if (m_ActivePose != M_NO_POSE) {
        LOG_DEBUG("Clearing Lara's pose");
    }
    m_ActivePose = M_NO_POSE;
}

void Lara_Pose_Cycle(const int32_t dir)
{
    if (!Lara_Pose_IsAvailable()) {
        return;
    }
    if (m_ActivePose == M_NO_POSE) {
        m_ActivePose = (dir > 0) ? 0 : m_Poses->count - 1;
    } else {
        m_ActivePose += dir;
        m_ActivePose += m_Poses->count;
        m_ActivePose %= m_Poses->count;
    }

    LOG_DEBUG("Active Lara pose: %d", m_ActivePose);
    Lara_Hair_Control(true);
    Interpolation_CommitBraid();
}

const LARA_POSE *Lara_Pose_Get(void)
{
    if (m_ActivePose == M_NO_POSE) {
        return nullptr;
    }
    return Vector_Get(m_Poses, m_ActivePose);
}
