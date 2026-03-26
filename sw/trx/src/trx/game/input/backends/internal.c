#include <trx/game/input/backends/internal.h>

#include <trx/config.h>

static bool M_IsManualCameraInput(const INPUT_ROLE role)
{
    return role == INPUT_ROLE_CAMERA_FORWARD || role == INPUT_ROLE_CAMERA_BACK
        || role == INPUT_ROLE_CAMERA_LEFT || role == INPUT_ROLE_CAMERA_RIGHT;
}

void Input_ConflictHelper(
    const INPUT_LAYOUT layout,
    bool (*check_conflict_func)(
        INPUT_LAYOUT layout, INPUT_ROLE role1, INPUT_ROLE role2),
    void (*assign_conflict_func)(
        INPUT_LAYOUT layout, INPUT_ROLE role, bool conflict))
{
    for (INPUT_ROLE role1 = 0; role1 < INPUT_ROLE_NUMBER_OF; role1++) {
        if (!Input_IsRoleRebindable(role1)) {
            continue;
        }

        bool conflict = false;
        for (INPUT_ROLE role2 = 0; role2 < INPUT_ROLE_NUMBER_OF; role2++) {
            if (!Input_IsRoleRebindable(role2)) {
                continue;
            }

            if (role1 == role2) {
                continue;
            }

            if (!g_Config.gameplay.enable_manual_camera
                && (M_IsManualCameraInput(role1)
                    || M_IsManualCameraInput(role2))) {
                continue;
            }

            if (check_conflict_func(layout, role1, role2)) {
                conflict = true;
            }
        }

        assign_conflict_func(layout, role1, conflict);
    }
}
