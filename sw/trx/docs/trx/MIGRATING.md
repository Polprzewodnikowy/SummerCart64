---
title: Migrating levels
order: 3
---

# Migration guide for level builders

## TRX

### Version 1.4 to 1.5

1. **Update TR2 detonator box**
   Dynamic light output when using the detonator is no longer hard-coded and now
   uses animation commands. The updated OG asset is available to download 
   [here](INJECTIONS.md#builder-workflow-keep-the-codebincode-or-bake-into-your-wad).

### Version 1.3 to 1.4

1. **Update strings file structure**
   The flat string section has been replaced with nested root sections.
   Please see shipped string files or documentation for details.

### Version 1.2 to 1.3

1. **TR1 missile catalog names were renamed**:
    In `cfg/catalog_objects.csv`, update old missile symbols to the new names:
    - `O_MISSILE_1` → `O_NATLA_GUN`
    - `O_MISSILE_2` → `O_MISSILE_ATLANTEAN_SHARD`
    - `O_MISSILE_3` → `O_MISSILE_ATLANTEAN_BOMB`
    - `O_MISSILE_4` and `O_MISSILE_5` are no longer used and should be removed.

    This also affects catalog-derived Lua names (`trx.catalog.objects`):
    - `missile_1` → `natla_gun`
    - `missile_2` → `missile_atlantean_shard`
    - `missile_3` → `missile_atlantean_bomb`

2. **TR2 breakable window catalog names were renamed**:
    In `cfg/catalog_objects.csv`, update old breakable windows to the new names:
    - `O_WINDOW_1` → `O_SMASH_OBJECT_1`
    - `O_WINDOW_2` → `O_SMASH_OBJECT_2`

    This also affects catalog-derived Lua names (`trx.catalog.objects`):
    - `window_1` → `smash_object_1`
    - `window_2` → `smash_object_2`

3. **Flooding flip effect sound ID was changed**:
    In `cfg/catalog_samples.csv`, add an alias for `SFX_FLOOD`:
    - TR1: `81, SFX_FLOOD`
    - TR2: `79, SFX_FLOOD`

### Version 1.1 to 1.2

1. **Lara skin system**:  
    Lara's outfit must now be defined using additional skin objects, along with
    game-flow and JSON setup. Refer to [outfits documentation](OUTFITS.md).

2. **Lua event name cleanup**:  
    The following events got new names:
    - `on_level_init` → `before_level_file`
    - `on_level_start` → `after_level_file`
    - `on_level_load` → `after_level_state`
    - `on_control` → `before_control`
    - `on_control_post` → `after_control`

3. **Lua objects catalog name cleanup**:  
    All keys in `trx.catalog.objects` had their `O_` prefix removed and were
    converted to lowercase.  
    Before: `trx.catalog.objects.O_BANDIT_1`  
    After: `trx.catalog.objects.bandit_1`

4. **Savegame file pattern rename**:  
    Replace `savegame_fmt_bson` with `savegame_file_fmt` in game flow files.
    The old `savegame_fmt_bson` key is still accepted but logs a warning and is
    scheduled for removal in TRX 1.5.

5. **Legacy savegame pattern removed**:  
    Remove the `savegame_fmt_legacy` key from game flow files.

### Version 1.0 to 1.1

1. **Ally and ally target behavior moved to Lua**:  
    Monks being allies and bandits being enemies who will target allies is no
    longer hardcoded and instead must be defined in Lua. Refer to the game flow
    and linked script files of the original levels for reference.

## TR1X

### Version 4.15 to TRX 1.0

1. **Game flow options moved to the config module**:
    Certain settings are no longer part of the game flow spec and instead
    became hidden player settings. To change them, put them in the
    `enforced_config` section. List of the affected settings:
    - `demo_delay`
    - `enable_killer_pushblocks`

2. **Lara shotgun animation**: 
   Lara now uses the TR2+ approach of a separate shotgun mesh on her back. You
   must use the `lara_guns.bin` injection or otherwise refer to 
   https://github.com/LostArtefacts/TRXInjectionTool/blob/main/docs/ASSETS.md

3. **Lara extra animations**: 
   Lara now uses the TR2+ approach of having defined state changes for extra
   animations (scion pickups, Midas touch etc). You must use the `lara_extra.bin`
   injection or otherwise refer to 
   https://github.com/LostArtefacts/TRXInjectionTool/blob/main/docs/ASSETS.md

### Version 4.13 to 4.14

1. **Update file paths**  
   - Move and rename the `cfg/TR1X_gameflow.json5` file to `cfg/tr1/gameflow.json5`.
   - Move and rename the `cfg/TR1X_strings*.json5` files to `cfg/tr1/strings*.json5`.
   - Move and rename the `cfg/TRX_common_strings*.json5` files to `cfg/base_strings*.json5`.
   - Remove leftover `TR1X_strings_ub.json5`.

    This is how the directory should look:
    ```
    .
    └── cfg
        ├── base_strings.json5
        ├── base_strings-pl.json5 (in case you want to provide translation files)
        ├── base_strings-….json5 (in case you want to provide translation files)
        ├── tr1
        │   ├── gameflow.json5
        │   ├── strings.json5
        │   ├── strings-pl.json5 (in case you want to provide translation files)
        │   └── strings-….json5 (in case you want to provide translation files)
        └── poses.json5
    ```

### Version 4.9 to 4.10

1. **Update fog configuration**  
    If you wish to force your fog settings on player:
    - Rename `draw_distance_fade` to `fog_start`
    - Rename `draw_distance_max` to `fog_end`

    If you wish to give the player agency to change the fog:
    - Remove `draw_distance_fade` and `draw_distance_max`

### Version 4.7 to 4.8

1. **Rename basic keys**  
   - Replace `file` key with `path` for every level.
   - Replace `music` key with `music_track` for every level.

2. **Update level enumeration structure**:
   - The `"type": "title"` property is no longer supported. Instead, the title
     level needs to be placed in the top-level `"title"` key.
   - The `"type": "cutscene"` property is no longer supported. Instead, the
     cutscenes need to be placed in the top-level `"cutscenes"` array.
   - All FMVs need to be placed in its own top-level `"fmvs"` array.

3. **Update individual level sequences**  
   - `start_game` should be removed.
   - `exit_to_cine` should be removed.
   - `exit_to_level` should be replaced with `level_complete`. No parameter needed.
   - `display_picture` no longer takes a `picture_path` argument and instead just takes a `path`.
   - `loading_screen` no longer takes a `picture_path` argument and instead just takes a `path`.
   - `level_stats` no longer takes a `level_id` argument.
   - `total_stats` no longer takes a `picture_path` argument and instead takes a `background_path`.
   - `play_fmv` no longer takes a `fmv_path` argument and instead takes a `fmv_id`.
   - `play_synced_audio` is renamed to `play_music` and takes a `music_track` argument rather than `audio_id`.

4. **Update strings**  
   The game strings are now placed in a separate file, `TR1X_strings.json5` in
   preparation to eventually support internationalization. Elements such as
   item titles or item names need to be configured entirely in the new file, so
   all `"strings"` keys can be safely removed from the game flow. Refer to
   [game strings documentation](4-GAME_STRINGS.md) for more details.



## TR2X

### Version 1.5 to TRX 1.0

1. **Game flow options moved to the config module**:
    Certain settings are no longer part of the game flow spec and instead
    became hidden player settings. To change them, put them in the
    `enforced_config` section. List of the affected settings:
    - `lockout_option_ring`
    - `load_save_disabled`
    - `play_any_level`
    - `demo_delay`
    - `cheat_keys`
    - `enable_killer_pushblocks`

2. **Removed game flow settings**
    The following game flow features were removed and are no longer available:
    - `cmd_init`
    - `cmd_title`
    - `cmd_death_in_demo`
    - `cmd_death_in_game`
    - `cmd_demo_end`
    - `cmd_demo_interrupt`
    - `single_level`
    - `is_demo_version`

3. **Lara extra animations**: 
   Lara's extra animations have been combined with TR1. You must use the
   `lara_extra.bin` injection or otherwise refer to 
   https://github.com/LostArtefacts/TRXInjectionTool/blob/main/docs/ASSETS.md

4. **Secret track**:
   The setting `secret_track` is no longer present – the engine will always
   play `MX_SECRET` track. To change its slot, please refer to the
   `catalog_music.csv` file.

### Version 1.3 to 1.4

1. **Update file paths**  
   - Move and rename the `cfg/TR2X_gameflow.json5` file to `cfg/tr2/gameflow.json5`.
   - Move and rename the `cfg/TR2X_strings*.json5` files to `cfg/tr2/strings*.json5`.
   - Move and rename the `cfg/TRX_common_strings*.json5` files to `cfg/base_strings*.json5`.
   - Remove leftover `TR2X_strings_ub.json5`.

    This is how the directory should look:
    ```
    .
    └── cfg
        ├── base_strings.json5
        ├── base_strings-pl.json5 (in case you want to provide translation files)
        ├── base_strings-….json5 (in case you want to provide translation files)
        ├── tr2
        │   ├── gameflow.json5
        │   ├── strings.json5
        │   ├── strings-pl.json5 (in case you want to provide translation files)
        │   └── strings-….json5 (in case you want to provide translation files)
        └── poses.json5
    ```

### Version 1.2 to 1.3

1. **Rename objects**
    - Replace `"detonator_1"` with `"gong"`.
    - Replace `"detonator_2"` with `"detonator_box"`.

2. **Re-add pistols**  
   Pistols are no longer added automatically to a level that follows one in
   which Lara previously lost her weapons. A game flow entry to re-add pistols
   will be required - refer to the Diving Area level in the default game flow.

3. **Bears, wolves and ice warriors**  
   If you wish to use the bear, wolf or ice warrior (monk with no shadow) from
   The Golden Mask while still being able to use big spiders, small spiders and
   other monks, use the following object slots.
    - Bear: slot 265
    - Wolf: slot 266
    - Ice warrior: slot 267

4. **Disabling gym**
    The option `gym_enabled` is no longer available. If you need to remove the
    access to Lara's Home, please either remove the relevant level from the
    game flow (this may break existing saves), or change its type to `"dummy"`
    to get it ignored (this will work with existing saves).

### Version 1.0.2 to 1.1

1. **Update first level inventory allocation**  
   The first level no longer hard-codes the shotgun, flare and small/large medi
   pack allocations. To continue to have Lara start with these items, refer to
   the shipped game flow file's `Great Wall` sequences, specifically the
   `give_item` entries.
