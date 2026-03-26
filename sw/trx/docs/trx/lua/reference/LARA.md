---
title: Lara
order: 3
---

## Lara module

Module for interacting with the Lara's object.

### Functions

- [lua]`trx.lara.item`  
    Returns [lua]`trx.items.Item` associated with Lara, or [lua]`nil` if the
    Lara object is not available.

- [lua]`trx.lara.target`  
    Read-only - returns Lara's current gun target as [lua]`trx.items.Item`,
    or [lua]`nil` if no target is locked.

- [lua]`trx.lara.exposure_bar`  
    Reads/writes exposure timer (cold water bar). The maximum value is 600.
    If the cold bar setting is disabled on the game flow level, the health must
    be managed manually from LUA.

- [lua]`trx.lara.air_bar`  
    Reads/writes Lara's air timer. The maximum value is 1800.  
      
    Example:
    ```lua
    -- Infinite oxygen
    trx.events.after_control(function()
        trx.lara.air_bar = 1800
    end)
    ```

- [lua]`trx.lara.outfit`  
    Reads/writes Lara's outfit name string (for example [lua]`"tr2_diving_suit"`).
    Outfit names are the keys defined in [md]`cfg/outfits.json5`. Outfits are
    stored in saves, but writing this value does not change the global config
    setting, so subsequent levels will adhere to regular outfit changes.

- [lua]`trx.lara.set_extra_equipment(lara_mesh_id, extra_mesh_id)`  
    Defines a specific extra mesh to be drawn at the same position as the given
    Lara mesh.

    The extra mesh must be present in the `O_LARA_SKIN_SWAP_EXTRA` object and 
    should be setup properly in the [outfits JSON](../../OUTFITS.md). Refer
    to the constants further below.

    Example:
    ```lua
    -- Put an oar in Lara's right hand
    trx.lara.set_extra_equipment(trx.lara.mesh.hand_r, trx.lara.extra_mesh.oar)
    ```

- [lua]`trx.lara.clear_equipment(lara_mesh_id)`  
    Removes any equipment on the given Lara mesh. This applies to guns and extra
    meshes.

- [lua]`trx.lara.holsters_visible`  
    Hides or shows Lara's holsters. This is used in OG TR1/2 gym and Home Sweet
    Home levels to maintain original outfit appearance. If Lara picks up a
    holster type weapon, or the weapon cheat is used, or a gun is given via the
    console, the holsters will automatically be made visible.

- [lua]`trx.lara.has_pistol_weapon`  
    Read-only - returns true if Lara has any pistol-type weapon currently in her
    inventory.

- [lua]`trx.lara.extra_anim`  
    Read-only - if Lara is currently in an extra anim state, returns the
    relative animation number of the `O_LARA_EXTRA` object.  
    Otherwise, returns -1.

- [lua]`trx.lara.equipped_gun`  
    Read-only - returns Lara's currently equipped gun ID.
    Compare values using [lua]`trx.catalog.weapons`.

## Mesh constants

- `trx.lara.mesh.hips`  
    An index to Lara's hips mesh.
- `trx.lara.mesh.thigh_l`  
    An index to Lara's left thigh mesh.
- `trx.lara.mesh.calf_l`  
    An index to Lara's left calf mesh.
- `trx.lara.mesh.foot_l`  
    An index to Lara's left foot mesh.
- `trx.lara.mesh.thigh_r`  
    An index to Lara's right thigh mesh.
- `trx.lara.mesh.calf_r`  
    An index to Lara's right calf mesh.
- `trx.lara.mesh.foot_r`  
    An index to Lara's right foot mesh.
- `trx.lara.mesh.torso`  
    An index to Lara's torso mesh.
- `trx.lara.mesh.uarm_r`  
    An index to Lara's upper right arm mesh.
- `trx.lara.mesh.larm_r`  
    An index to Lara's lower right arm mesh.
- `trx.lara.mesh.hand_r`  
    An index to Lara's right hand mesh.
- `trx.lara.mesh.uarm_l`  
    An index to Lara's upper left arm mesh.
- `trx.lara.mesh.larm_l`  
    An index to Lara's lower left arm mesh.
- `trx.lara.mesh.hand_l`  
    An index to Lara's left hand mesh.
- `trx.lara.mesh.head`  
    An index to Lara's head mesh.

## Extra mesh constants

- `trx.lara.extra_mesh.dagger_hand`  
    An index to the dagger mesh used when Lara retrieves it from the dragon and
    when inspecting it at home.
- `trx.lara.extra_mesh.dagger_hips`  
    An index to the dagger mesh that sits on Lara's hips during Home Sweet Home.
- `trx.lara.extra_mesh.oar`  
    An index to the oar mesh used when Lara is in the kayak.
- `trx.lara.extra_mesh.spanner`  
    An index to the spanner mesh used when Lara is in the minecart.
- `trx.lara.extra_mesh.drink_can`  
    An index to the drink can mesh used in the High Security Compound cutscene.
- `trx.lara.extra_mesh.glasses_opaque`  
    An index to Lara's opaque sunglasses mesh.
- `trx.lara.extra_mesh.glasses_transparent`  
    An index to Lara's transparent sunglasses mesh.
