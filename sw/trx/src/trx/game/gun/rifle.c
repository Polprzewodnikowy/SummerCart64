#include <trx/game/gun/rifle.h>

#include <trx/config.h>
#include <trx/core/math.h>
#include <trx/game/camera.h>
#include <trx/game/game.h>
#include <trx/game/gun/common.h>
#include <trx/game/gun/control.h>
#include <trx/game/gun/misc.h>
#include <trx/game/gun/smashing.h>
#include <trx/game/gun/smoke.h>
#include <trx/game/gun/vars.h>
#include <trx/game/input.h>
#include <trx/game/lara.h>
#include <trx/game/random.h>
#include <trx/game/rooms.h>
#include <trx/game/sound.h>
#include <trx/game/sparks.h>
#include <trx/game/spawn.h>
#include <trx/game/stats.h>
#include <trx/version.h>

#define M_SHOTGUN_PELLET_SCATTER (DEG_1 * 20) // = 3640
#define M_HARPOON_BOLT_SPEED_TR12 150
#define M_HARPOON_BOLT_SPEED_TR3 256
#define M_GRENADE_SPEED 200

typedef enum {
    LA_G_AIM = 0,
    LA_G_DRAW = 1,
    LA_G_RECOIL = 2,
    LA_G_UNDRAW = 3,
    LA_G_UNAIM = 4,
    LA_G_RELOAD = 5,
    LA_G_UAIM = 6,
    LA_G_UUNAIM = 7,
    LA_G_URECOIL = 8,
    LA_G_SURF_UNDRAW = 9,
} M_ANIM;

static bool m_M16Firing = false;
static bool m_ReloadHarpoon = false;

static void M_SetTR3ProjectileShade(ITEM *const item)
{
    if (item == nullptr) {
        return;
    }

    // OG TR3 uses `item->shade = -0x3DF0` on projectiles; in TRX any negative
    // shade forces the dynamic/smoothed lighting path.
    item->shade.value_1 = -1;
    item->shade.value_2 = -1;
}

static M_ANIM M_GetReadyAnim(const LARA_GUN_TYPE weapon_type)
{
    const LARA_INFO *const lara = Lara_GetLaraInfo();
    switch (weapon_type) {
    case LGT_HARPOON:
        return lara->water_status == LWS_UNDERWATER ? LA_G_UAIM : LA_G_AIM;
    case LGT_GRENADE:
        return LA_G_DRAW;
    default:
        return LA_G_AIM;
    }
}

static void M_AnimateGun(ITEM *const item)
{
    // While the item is drawn in Lara_Draw, it needs a world position for
    // sound effect commands in Item_Animate.
    const ITEM *const lara_item = Lara_GetItem();
    item->pos.x = lara_item->pos.x;
    item->pos.y = lara_item->pos.y - LARA_HEIGHT;
    item->pos.z = lara_item->pos.z;
    Item_Animate(item);
}

static void M_Ready(const LARA_GUN_TYPE weapon_type)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    lara->gun_status = LGS_READY;
    lara->target = nullptr;

    const OBJECT *const obj = Object_Get(Gun_GetWeaponAnim(weapon_type));
    lara->left_arm.frame_base = obj->frame_base;
    lara->left_arm.frame_num = LF_G_AIM_START;
    lara->left_arm.lock = 0;
    lara->left_arm.rot.x = 0;
    lara->left_arm.rot.y = 0;
    lara->left_arm.rot.z = 0;

    lara->right_arm.frame_base = obj->frame_base;
    lara->right_arm.frame_num = LF_G_AIM_START;
    lara->right_arm.lock = 0;
    lara->right_arm.rot.x = 0;
    lara->right_arm.rot.y = 0;
    lara->right_arm.rot.z = 0;

    if (g_Config.gameplay.look_mode == LOOK_MODE_RESTRICTED) {
        lara->head_rot.x = 0;
        lara->head_rot.y = 0;
        lara->torso_rot.x = 0;
        lara->torso_rot.y = 0;
    }
}

static void M_FireGeneric(const LARA_GUN_TYPE weapon_type)
{
    const ITEM *const lara_item = Lara_GetItem();
    LARA_INFO *const lara = Lara_GetLaraInfo();
    bool fired = false;
    int16_t angles[2] = {
        lara->left_arm.rot.y + lara_item->rot.y,
        lara->left_arm.rot.x,
    };
    if (g_TRVersion == 3 && !lara->left_arm.lock) {
        angles[0] += lara->torso_rot.y;
        angles[1] += lara->torso_rot.x;
    }

    const int32_t clip = Gun_GetAmmoClipCount(weapon_type);
    for (int32_t i = 0; i < clip; i++) {
        int16_t dangles[2] = {
            angles[0]
                + M_SHOTGUN_PELLET_SCATTER * (Random_GetControl() - 0x4000)
                    / 0x10000,
            angles[1]
                + M_SHOTGUN_PELLET_SCATTER * (Random_GetControl() - 0x4000)
                    / 0x10000,
        };
        if (Gun_FireWeapon(weapon_type, lara->target, lara_item, dangles)) {
            fired = true;
        }
    }

    if (fired) {
        lara->right_arm.flash_gun = g_Weapons[weapon_type].flash_time;
        Gun_Smoke_OnFire(weapon_type, true);
        Sound_Effect(
            g_Weapons[weapon_type].sample_num, &lara_item->pos, SPM_NORMAL);
    }
}

static void M_FireM16(const bool running, const LARA_GUN_TYPE weapon_type)
{
    const ITEM *const lara_item = Lara_GetItem();
    LARA_INFO *const lara = Lara_GetLaraInfo();
    int16_t angles[2] = {
        lara->left_arm.rot.y + lara_item->rot.y,
        lara->left_arm.rot.x,
    };
    if (g_TRVersion == 3 && !lara->left_arm.lock) {
        angles[0] += lara->torso_rot.y;
        angles[1] += lara->torso_rot.x;
    }

    if (g_Config.gameplay.fix_m16_accuracy && running) {
        g_Weapons[weapon_type].shot_accuracy = DEG_1 * 12;
        g_Weapons[weapon_type].damage = 1;
    } else {
        g_Weapons[weapon_type].shot_accuracy = DEG_1 * 4;
        g_Weapons[weapon_type].damage = 3;
    }

    if (Gun_FireWeapon(weapon_type, lara->target, lara_item, angles)) {
        lara->right_arm.flash_gun = g_Weapons[weapon_type].flash_time;
        Spawn_GunShell(weapon_type, true);
        Gun_Smoke_OnFire(weapon_type, true);
    }
}

static void M_FireHarpoon(void)
{
    const ITEM *const lara_item = Lara_GetItem();
    LARA_INFO *const lara = Lara_GetLaraInfo();
    if (lara->harpoon_ammo.ammo <= 0) {
        goto finish;
    }

    const int16_t item_num = Item_Create();
    if (item_num == NO_ITEM) {
        goto finish;
    }

    const WEAPON_INFO *const weapon = &g_Weapons[LGT_HARPOON];
    const GAME_VECTOR origin = {
        .pos = {
            .x = lara_item->pos.x,
            .y = lara_item->pos.y - weapon->gun_height,
            .z = lara_item->pos.z,
        },
        .room_num = lara_item->room_num,
    };

    ITEM *const projectile_item = Item_Get(item_num);
    projectile_item->object_id = O_HARPOON_BOLT;
    projectile_item->room_num = lara_item->room_num;

    XYZ_32 offset = {
        .x = -2,
        .y = 373,
        .z = 77,
    };

    Lara_GetJointAbsPosition(&offset, LM_HAND_R);
    projectile_item->pos = offset;
    projectile_item->interp.prev.pos = projectile_item->pos;
    Item_Initialise(item_num);

    if (lara->target != nullptr) {
        GAME_VECTOR lara_vec;
        Gun_FindTargetPoint(lara->target, &lara_vec);
        const int32_t dx = lara_vec.pos.x - projectile_item->pos.x;
        const int32_t dz = lara_vec.pos.z - projectile_item->pos.z;
        const int32_t dy = lara_vec.pos.y - projectile_item->pos.y;
        const int32_t dxz = Math_Sqrt(SQUARE(dx) + SQUARE(dz));
        projectile_item->rot.y = Math_Atan(dz, dx);
        projectile_item->rot.x = -Math_Atan(dxz, dy);
        projectile_item->rot.z = 0;
    } else {
        if (g_TRVersion == 3) {
            projectile_item->rot.x = lara->torso_rot.x + lara_item->rot.x;
            projectile_item->rot.y = lara->torso_rot.y + lara_item->rot.y;
        } else {
            projectile_item->rot.x = lara->left_arm.rot.x + lara_item->rot.x;
            projectile_item->rot.y = lara->left_arm.rot.y + lara_item->rot.y;
        }
        projectile_item->rot.z = 0;
    }

    const int32_t bolt_speed =
        g_TRVersion == 3 ? M_HARPOON_BOLT_SPEED_TR3 : M_HARPOON_BOLT_SPEED_TR12;
    projectile_item->fall_speed =
        (-bolt_speed * Math_Sin(projectile_item->rot.x)) >> W2V_SHIFT;
    projectile_item->speed =
        (bolt_speed * Math_Cos(projectile_item->rot.x)) >> W2V_SHIFT;

    if (g_TRVersion == 3) {
        M_SetTR3ProjectileShade(projectile_item);
        projectile_item->hit_points = 256;
    }

    Item_AddActive(item_num);
    projectile_item->status = IS_ACTIVE;

    Gun_SmashItems(
        origin,
        (GAME_VECTOR) {
            .pos = projectile_item->pos,
            .room_num = projectile_item->room_num,
        },
        nullptr, projectile_item->object_id);

    lara->harpoon_ammo.ammo--;
    Stats_AddAmmoUsed();

finish:
    const int32_t recoil = g_Config.gameplay.harpoon_recoil;
    const bool is_ngplus = Game_IsBonusFlagSet(GBF_NGPLUS);
    if (recoil <= 0) {
        if (is_ngplus) {
            lara->harpoon_ammo.ammo++;
        }
    } else if ((lara->harpoon_ammo.ammo % recoil) == 0) {
        if (is_ngplus) {
            lara->harpoon_ammo.ammo += recoil;
        }
        m_ReloadHarpoon = lara->harpoon_ammo.ammo > 0;
    }
}

static void M_FireGrenade(void)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    const ITEM *const lara_item = Lara_GetItem();
    if (lara->grenade_ammo.ammo <= 0) {
        return;
    }
    const WEAPON_INFO *const weapon = &g_Weapons[LGT_GRENADE];
    const GAME_VECTOR origin = {
        .pos = {
            .x = lara_item->pos.x,
            .y = lara_item->pos.y - weapon->gun_height,
            .z = lara_item->pos.z,
        },
        .room_num = lara_item->room_num,
    };

    const int16_t item_num = Item_Create();
    if (item_num == NO_ITEM) {
        return;
    }

    ITEM *const projectile_item = Item_Get(item_num);
    projectile_item->object_id = O_GRENADE;
    projectile_item->room_num = lara_item->room_num;

    XYZ_32 offset =
        g_TRVersion == 3 ? (XYZ_32) { 0, 276, 80 } : (XYZ_32) { -2, 373, 77 };
    Lara_GetJointAbsPosition(&offset, LM_HAND_R);
    projectile_item->pos = offset;
    projectile_item->interp.prev.pos = projectile_item->pos;
    Item_Initialise(item_num);

    int16_t room_num = projectile_item->room_num;
    const SECTOR *const sector = Room_GetSector(origin.pos, &room_num);
    const int32_t height = Room_GetHeight(sector, origin.pos);
    if (height < origin.pos.y) {
        projectile_item->pos = (XYZ_32) {
            .x = lara_item->pos.x,
            .y = origin.pos.y,
            .z = lara_item->pos.z,
        };
    }

    Room_GetSector(projectile_item->pos, &room_num);
    Item_UpdateRoom(item_num, room_num);

    projectile_item->rot.x = lara->left_arm.rot.x + lara_item->rot.x;
    projectile_item->rot.y = lara->left_arm.rot.y + lara_item->rot.y;
    projectile_item->rot.z = 0;
    if (g_TRVersion == 3 && !lara->left_arm.lock) {
        projectile_item->rot.x += lara->torso_rot.x;
        projectile_item->rot.y += lara->torso_rot.y;
    }

    if (g_Config.gameplay.enable_bouncy_grenades) {
        // TR3 grenades use a timed fuse and bounce/roll physics, so use speed
        // as horizontal velocity magnitude and fall_speed as vertical velocity.
        projectile_item->speed = 128;
        projectile_item->fall_speed =
            -(projectile_item->speed * Math_Sin(projectile_item->rot.x))
            >> W2V_SHIFT;
        projectile_item->current_anim_state = projectile_item->rot.x;
        projectile_item->goal_anim_state = projectile_item->rot.y;
        projectile_item->required_anim_state = 0;
        projectile_item->hit_points = 120;
    } else {
        projectile_item->speed = M_GRENADE_SPEED;
        projectile_item->fall_speed = 0;
    }

    Item_AddActive(item_num);
    projectile_item->status = IS_ACTIVE;

    Gun_SmashItems(
        origin,
        (GAME_VECTOR) {
            .pos = projectile_item->pos,
            .room_num = projectile_item->room_num,
        },
        nullptr, projectile_item->object_id);

    if (!Game_IsBonusFlagSet(GBF_NGPLUS)) {
        lara->grenade_ammo.ammo--;
    }
    Stats_AddAmmoUsed();

    Gun_Smoke_OnFire(LGT_GRENADE, true);
}

static void M_FireRocket(void)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();
    const ITEM *const lara_item = Lara_GetItem();
    if (lara->rocket_ammo.ammo <= 0) {
        return;
    }
    const WEAPON_INFO *const weapon = &g_Weapons[LGT_ROCKET];
    const GAME_VECTOR origin = {
        .pos = {
        .x = lara_item->pos.x,
        .y = lara_item->pos.y - weapon->gun_height,
        .z = lara_item->pos.z,
        },
        .room_num = lara_item->room_num,
    };

    const int16_t item_num = Item_Create();
    if (item_num == NO_ITEM) {
        return;
    }

    ITEM *const projectile_item = Item_Get(item_num);
    projectile_item->object_id = O_ROCKET;
    projectile_item->room_num = lara_item->room_num;

    XYZ_32 offset = {
        .x = 0,
        .y = 180,
        .z = 72,
    };
    Lara_GetJointAbsPosition(&offset, LM_HAND_R);
    projectile_item->pos = offset;
    projectile_item->interp.prev.pos = projectile_item->pos;
    Item_Initialise(item_num);

    projectile_item->rot.x = lara->left_arm.rot.x + lara_item->rot.x;
    projectile_item->rot.y = lara->left_arm.rot.y + lara_item->rot.y;
    projectile_item->rot.z = 0;
    if (!lara->left_arm.lock) {
        projectile_item->rot.x += lara->torso_rot.x;
        projectile_item->rot.y += lara->torso_rot.y;
    }

    projectile_item->speed = 16;
    Item_AddActive(item_num);
    projectile_item->status = IS_ACTIVE;

    Gun_SmashItems(
        origin,
        (GAME_VECTOR) {
            .pos = projectile_item->pos,
            .room_num = projectile_item->room_num,
        },
        nullptr, projectile_item->object_id);

    if (!Game_IsBonusFlagSet(GBF_NGPLUS)) {
        lara->rocket_ammo.ammo--;
    }
    Stats_AddAmmoUsed();

    if (g_TRVersion >= 3) {
        Sound_Effect(SFX_EXPLOSION_1, &lara_item->pos, 0x5000000 | SPM_PITCH);
    }

    Gun_Smoke_OnFire(LGT_ROCKET, true);

    if (g_TRVersion == 3) {
        M_SetTR3ProjectileShade(projectile_item);
        const XYZ_32 back_128 = XYZ_32_FromYawPitch(
            projectile_item->rot.y, projectile_item->rot.x, -128);
        for (int32_t i = 0; i < 8; i++) {
            const int32_t dist = -(Random_GetControl() & 0x7FF);
            const XYZ_32 back_vel = XYZ_32_FromYawPitch(
                projectile_item->rot.y, projectile_item->rot.x, dist);
            Sparks_TriggerRocketFlame(
                back_128,
                (XYZ_32) {
                    .x = back_vel.x - back_128.x,
                    .y = back_vel.y - back_128.y,
                    .z = back_vel.z - back_128.z,
                },
                item_num, projectile_item->room_num);
        }
    }
}

static void M_Fire(const LARA_GUN_TYPE weapon_type, const bool running)
{
    switch (weapon_type) {
    case LGT_HARPOON:
        M_FireHarpoon();
        break;
    case LGT_GRENADE:
        if (!running) {
            M_FireGrenade();
        }
        break;
    case LGT_ROCKET:
        M_FireRocket();
        break;
    case LGT_M16:
    case LGT_MP5:
        M_FireM16(running, weapon_type);
        break;
    default:
        if (!running) {
            M_FireGeneric(weapon_type);
        }
        break;
    }
}

static void M_PlayMachineGunSound(
    const LARA_GUN_TYPE weapon_type, const bool stopping)
{
    const ITEM *const lara_item = Lara_GetItem();
    if (weapon_type == LGT_M16) {
        Sound_Effect(
            stopping ? SFX_M16_STOP : SFX_M16_FIRE, &lara_item->pos,
            SPM_NORMAL);
        return;
    }

    // The MP5 uses a high-pitched explosion when either firing or stopping.
    // This is intentionally omitted in TR1/2 due to the sample's quality when
    // played in rapid succession.
    if (g_TRVersion >= 3) {
        Sound_Effect(SFX_EXPLOSION_1, &lara_item->pos, 0x5000000 | SPM_PITCH);
    }
    if (!stopping) {
        Sound_Effect(SFX_MP5_FIRE, &lara_item->pos, SPM_NORMAL);
    }
}

static void M_Animate(const LARA_GUN_TYPE weapon_type)
{
    const ITEM *const lara_item = Lara_GetItem();
    LARA_INFO *const lara = Lara_GetLaraInfo();

    const bool running = (weapon_type == LGT_M16 || weapon_type == LGT_MP5)
        && lara_item->speed != 0;
    ITEM *const item = Item_Get(lara->gun_item_num);

    switch (item->current_anim_state) {
    case LA_G_AIM:
        m_M16Firing = false;
        if (m_ReloadHarpoon) {
            item->goal_anim_state = LA_G_RELOAD;
            m_ReloadHarpoon = false;
        } else if (lara->water_status == LWS_UNDERWATER || running) {
            item->goal_anim_state = LA_G_UAIM;
        } else if (
            (g_Input.action && lara->target == nullptr)
            || lara->left_arm.lock) {
            item->goal_anim_state = LA_G_RECOIL;
        } else {
            item->goal_anim_state = LA_G_UNAIM;
        }
        break;

    case LA_G_UAIM:
        m_M16Firing = false;
        if (m_ReloadHarpoon) {
            item->goal_anim_state = LA_G_RELOAD;
            m_ReloadHarpoon = false;
        } else if (lara->water_status != LWS_UNDERWATER && !running) {
            item->goal_anim_state = LA_G_AIM;
        } else if (
            (g_Input.action && lara->target == nullptr)
            || lara->left_arm.lock) {
            item->goal_anim_state = LA_G_URECOIL;
        } else {
            item->goal_anim_state = LA_G_UUNAIM;
        }
        break;

    case LA_G_RECOIL:
        if (Item_TestFrameEqual(item, 0)) {
            item->goal_anim_state = LA_G_UNAIM;
            if (lara->water_status != LWS_UNDERWATER && !running
                && !m_ReloadHarpoon) {
                if (g_Input.action) {
                    if (lara->target == nullptr || lara->left_arm.lock) {
                        M_Fire(weapon_type, false);
                        if (weapon_type == LGT_M16 || weapon_type == LGT_MP5) {
                            M_PlayMachineGunSound(weapon_type, false);
                            m_M16Firing = true;
                        }
                        item->goal_anim_state = LA_G_RECOIL;
                    }
                } else if (lara->left_arm.lock) {
                    item->goal_anim_state = LA_G_AIM;
                }
            }

            if (item->goal_anim_state != LA_G_RECOIL && m_M16Firing) {
                M_PlayMachineGunSound(weapon_type, true);
                m_M16Firing = false;
            }
        } else if (m_M16Firing) {
            M_PlayMachineGunSound(weapon_type, false);
        } else if (
            weapon_type == LGT_SHOTGUN && !g_Input.action
            && !lara->left_arm.lock) {
            item->goal_anim_state = LA_G_UNAIM;
        }

        if (weapon_type == LGT_SHOTGUN && Item_TestFrameEqual(item, 12)) {
            Spawn_GunShell(LGT_SHOTGUN, true);
        }
        break;

    case LA_G_URECOIL:
        if (Item_TestFrameEqual(item, 0)) {
            item->goal_anim_state = LA_G_UUNAIM;
            if ((lara->water_status == LWS_UNDERWATER || running)
                && !m_ReloadHarpoon) {
                if (g_Input.action) {
                    if (lara->target == nullptr || lara->left_arm.lock) {
                        M_Fire(weapon_type, true);
                        item->goal_anim_state = LA_G_URECOIL;
                    }
                } else if (lara->left_arm.lock) {
                    item->goal_anim_state = LA_G_UAIM;
                }
            }
        }

        if (item->goal_anim_state == LA_G_URECOIL
            && (weapon_type == LGT_M16 || weapon_type == LGT_MP5)) {
            M_PlayMachineGunSound(weapon_type, false);
        }
        break;

    default:
        break;
    }

    M_AnimateGun(item);
    lara->left_arm.anim_num = item->anim_num;
    lara->left_arm.frame_base = Item_GetAnim(item)->frame_ptr;
    lara->left_arm.frame_num = Item_GetRelativeFrame(item);
    lara->right_arm.anim_num = item->anim_num;
    lara->right_arm.frame_base = Item_GetAnim(item)->frame_ptr;
    lara->right_arm.frame_num = Item_GetRelativeFrame(item);
}

void Gun_Rifle_Control(const LARA_GUN_TYPE weapon_type)
{
    const WEAPON_INFO *const weapon = &g_Weapons[weapon_type];
    LARA_INFO *const lara = Lara_GetLaraInfo();

    Gun_GetNewTarget(weapon);
    if (g_InputDB.change_target && g_Config.gameplay.enable_target_change) {
        Gun_ChangeTarget(weapon);
    }

    Gun_AimWeapon(weapon, &lara->left_arm);

    if (lara->left_arm.lock) {
        lara->torso_rot.x = lara->left_arm.rot.x;
        lara->torso_rot.y = lara->left_arm.rot.y;
        if (g_Config.gameplay.look_mode != LOOK_MODE_UNRESTRICTED
            || g_Camera.type != CAM_LOOK) {
            lara->head_rot.x = 0;
            lara->head_rot.y = 0;
        }
    }

    M_Animate(weapon_type);

    if (lara->right_arm.flash_gun
        && (weapon_type == LGT_SHOTGUN || weapon_type == LGT_M16
            || weapon_type == LGT_MP5)) {
        Gun_AddDynamicLight();
    }
}

void Gun_Rifle_Draw(const LARA_GUN_TYPE weapon_type)
{
    ITEM *item;
    LARA_INFO *const lara = Lara_GetLaraInfo();
    const WEAPON_INFO *const weapon = &g_Weapons[weapon_type];

    if (lara->gun_item_num != NO_ITEM) {
        item = Item_Get(lara->gun_item_num);
    } else {
        lara->gun_item_num = Item_Create();
        item = Item_Get(lara->gun_item_num);
        item->object_id = Gun_GetWeaponAnim(weapon_type);
        Item_SwitchToAnim(item, weapon->equip_anim_idx, 0);
        item->goal_anim_state = LA_G_DRAW;
        item->current_anim_state = LA_G_DRAW;
        item->status = IS_ACTIVE;
        item->room_num = NO_ROOM;
        const OBJECT *const obj = Object_Get(item->object_id);
        lara->right_arm.frame_base = obj->frame_base;
        lara->left_arm.frame_base = obj->frame_base;
    }

    M_AnimateGun(item);

    if (item->current_anim_state == LA_G_AIM
        || item->current_anim_state == LA_G_UAIM) {
        M_Ready(weapon_type);
    } else if (Item_TestFrameEqual(item, weapon->draw_frame)) {
        Gun_Rifle_DrawMeshes(weapon_type);
    } else if (lara->water_status == LWS_UNDERWATER) {
        item->goal_anim_state = LA_G_UAIM;
    }

    lara->left_arm.anim_num = item->anim_num;
    lara->left_arm.frame_base = Item_GetAnim(item)->frame_ptr;
    lara->left_arm.frame_num = Item_GetRelativeFrame(item);
    lara->right_arm.anim_num = item->anim_num;
    lara->right_arm.frame_base = Item_GetAnim(item)->frame_ptr;
    lara->right_arm.frame_num = Item_GetRelativeFrame(item);
}

void Gun_Rifle_Undraw(const LARA_GUN_TYPE weapon_type)
{
    LARA_INFO *const lara = Lara_GetLaraInfo();

    ITEM *const item = Item_Get(lara->gun_item_num);
    const ANIM *const anim = Item_GetAnim(item);
    if (lara->water_status == LWS_SURFACE
        && Anim_HasChange(anim, LA_G_SURF_UNDRAW)) {
        item->goal_anim_state = LA_G_SURF_UNDRAW;
    } else {
        item->goal_anim_state = LA_G_UNDRAW;
    }
    M_AnimateGun(item);

    const WEAPON_INFO *const weapon = &g_Weapons[weapon_type];
    if (item->status == IS_DEACTIVATED) {
        Item_Kill(lara->gun_item_num);
        lara->gun_item_num = NO_ITEM;
        lara->gun_status = LGS_ARMLESS;
        lara->target = nullptr;
        lara->left_arm.frame_num = 0;
        lara->left_arm.lock = 0;
        lara->right_arm.frame_num = 0;
        lara->right_arm.lock = 0;
    } else if (
        item->current_anim_state == LA_G_UNDRAW
        && Item_TestFrameEqual(item, weapon->undraw_frame)) {
        Gun_Rifle_UndrawMeshes(weapon_type);
    }

    if (!g_Input.look || g_Config.gameplay.look_mode == LOOK_MODE_RESTRICTED) {
        lara->head_rot.x = 0;
        lara->head_rot.y = 0;
        lara->torso_rot.x += lara->torso_rot.x / -2;
        lara->torso_rot.y += lara->torso_rot.y / -2;
    }
    lara->left_arm.anim_num = item->anim_num;
    lara->left_arm.frame_base = Item_GetAnim(item)->frame_ptr;
    lara->left_arm.frame_num = Item_GetRelativeFrame(item);
    lara->right_arm.anim_num = item->anim_num;
    lara->right_arm.frame_base = Item_GetAnim(item)->frame_ptr;
    lara->right_arm.frame_num = Item_GetRelativeFrame(item);
}

void Gun_Rifle_DrawMeshes(const LARA_GUN_TYPE weapon_type)
{
    Gun_SetLaraHandLMesh(LGT_UNARMED);
    Gun_SetLaraHandRMesh(weapon_type);
    Gun_SetLaraBackMesh(LGT_UNARMED);
}

void Gun_Rifle_UndrawMeshes(const LARA_GUN_TYPE weapon_type)
{
    Gun_SetLaraHandLMesh(LGT_UNARMED);
    Gun_SetLaraHandRMesh(LGT_UNARMED);
    Gun_SetLaraBackMesh(weapon_type);
}

void Gun_Rifle_EnsureReady(const LARA_GUN_TYPE weapon_type)
{
    Gun_Rifle_Draw(weapon_type);

    const LARA_INFO *const lara = Lara_GetLaraInfo();
    const ITEM *const item = Item_Get(lara->gun_item_num);
    const int16_t goal_anim = M_GetReadyAnim(weapon_type);

    do {
        Gun_Rifle_Draw(weapon_type);
    } while (Item_GetRelativeAnim(item) != goal_anim);
}
