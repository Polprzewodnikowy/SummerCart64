---
title: Injections (.bin files)
order: 15
---

# Injections (`.bin` files)

In TRX, an *injection* is a binary patch file (`.bin`) that the engine applies
to level data at load time. Injections are used to fix or extend base game data
in a way that stays compatible with custom levels (unless you intentionally
replace the same data in your own WAD).

Most builders only need injections for the "default TRX assets" (extra Lara
animations, extended fonts, PDA model, etc).

## How injections are configured (gameflow)

Gameflow JSON supports injections in two places:

- **Global injections**: applied to all levels.
- **Per-level injections**: applied only for that level, optionally inheriting global injections.

By default, injections defined in the global gameflow are applied to every
level. If a level defines its own injections, those are merged with the global
set when the level loads.

Individual levels can set `inherit_injections` to `false`. In that case, global
injection files are not used. If such a level defines its own `injections`,
only those are applied; if it defines none, nothing is injected.

Relevant keys (names may differ slightly per gameflow version):

```json5
{
  // global
  "injections": [
    "data/injections/lara_extra.bin",
    "data/injections/font.bin"
  ],

  "levels": [
    {
      "path": "data/levels/MY_LEVEL.TR2",
      "inherit_injections": true,
      "injections": [
        "data/injections/pda_model.bin"
      ]
    }
  ]
}
```

> [!WARNING]
> If you **import** the assets into your level WAD (see below), you should then
> **remove** the corresponding `.bin` from gameflow to avoid
> double-applying/replacing data!

> [!NOTE]
> If a level should **not** receive the global injections, set
> `"inherit_injections": false` (or omit inheritance, depending on the
> schema/version you're targeting).

> [!NOTE]
> The gameflow ignores referenced injection files that do not exist, but it's
> best practice to remove references to keep gameflow clean.

## Builder workflow: keep the `.bin`, or bake into your WAD

You can handle TRX default assets in two ways:

1. **Keep using injections** (recommended for most cases):
   ship the `.bin` files and reference them in gameflow.
2. **Bake assets into your WAD**:
   import the provided assets into your level's WAD, then remove the related
   `.bin` references from gameflow.

### Common steps for importing TRX assets into your WAD (WadTool)

1. Open your level's WAD in **WadTool**.
2. Open the extracted `.wad2` file for the applicable game as the **source level** in WadTool.
3. Move the required assets from the source to the destination, replacing the existing ones.
4. Follow any asset-specific notes in the table below.
5. Update your gameflow to remove references to the asset's `.bin` file.

TRX provides asset packs intended for WadTool import:

- `https://lostartefacts.dev/pub/tr1-assets.zip`
- `https://lostartefacts.dev/pub/tr2-assets.zip`

The zips also include Tomb Editor catalogs (`Moveables.xml` /
`SpriteSequences.xml`) so TRX object names show up (and to enable cross-game
placements like TR2 guns in TR1 levels). See the README inside the zip for
details.

## Builder note on custom levels

Custom levels should generally not rely on injections for correctness; instead,
provide data that is already correct and consistent.

Note however that the injections that relate to Lara can work in custom levels
that do not modify Lara's default mesh structure or animations. These injection
files are based on the original Lara model.

## Default injection files (reference)

The rule of thumb for custom levels is that if an injection file name starts
with a level name, it is meant for specific original levels and should
generally be removed from your custom gameflow unless you know what you're doing.

### Core files useful for most builders

| Injection file                        | Usage    | Purpose                                                                                                                                                                                                                                                                                                                                           |
| ---                                   | ---      | ---                                                                                                                                                                                                                                                                                                                                               |
| `lara_animations.bin`                 | TR1, TR2 | Lara animations/state/commands (jump-twist, somersault, underwater roll, wading, etc). If Lara's appearance is customised, move the source object to another slot and replace meshes manually. **TR1 only:** add `wet-feet.xml` to the sound catalogue (adds sound IDs 15 & 17) and provide the referenced wet-feet `.wav` samples (or your own). |
| `lara_guns.bin` / `lara_gym_guns.bin` | TR1, TR2 | In TR1, replaces Lara's fixed shotgun-torso mesh with the TR2+ approach of an independent resting gun mesh. These files also contain Lara's guns from the other games, including flares. The gym file injects all of Lara's weapons and weapon animations in the gym level (for cheats only).                                                     |
| `lara_extra.bin`                      | TR1, TR2 | Combined object containing extra animations shared between TR1, TR2 and TR3.                                                                                                                                                                      |
| `lara_outfits.bin`                    | TR1, TR2 | Contains each of Lara's outfits to offer live skin swaps in-game.                                                                                                                                                                      |
| `pda_model.bin`                       | TR1, TR2 | The original PDA model with an opening animation. Used by the Gameplay options UI.                                                                                                                                                                                                                                                                |
| `font.bin`                            | TR1, TR2 | Replacement font sprites to support more characters than OG.                                                                                                                                                                                                                                                                                      |
| `secret_models_*.bin`                 | TR2      | 3D models for secret pickups in OG and Golden Mask.                                                                                                                                                                                                                                                                                               |
| `braid.bin`                           | TR1      | Braid option: injects braid plus mesh swaps for Lara's head/backpack (incl. Midas variant).                                                                                                                                                                                                                                                       |
| `bubbles.bin`                         | TR1      | Replacement sprites for Lara's underwater bubbles (OG sprites are cut off).                                                                                                                                                                                                                                                                       |
| `pickup_aid.bin`                      | TR1, TR2 | Sprite sequence for pickup aids option; custom levels should define a suitable sprite sequence in slot 185.                                                                                                                                                                                                                                       |
| `photo.bin`                           | TR1, TR2 | Camera shutter SFX for photo mode (needed only for cutscene levels).                                                                                                                                                                                                                                                                              |
| `purple_crystal.bin`                  | TR1      | Replacement savegame crystal model (PS1 style).                                                                                                                                                                                                                                                                                                   |
| `scion_collision.bin`                 | TR1      | Increases collision radius on the targetable Scion so it can be shot with the shotgun.                                                                                                                                                                                                                                                            |
| `guardian_death_commands.bin`         | TR2      | Bird guardian death anim command to end the level on the final frame (TRX removes the hard-coded behavior).                                                                                                                                                                                                                                       |
| `mines_pushblocks.bin`                | TR1      | Restores missing scraping SFX for pushblock types 2/3/4 by injecting anim command data.                                                                                                                                                                                                                                                           |
| `boat_bits.bin`                       | TR2      | Model for `O_BOAT_BITS` (221) used to show the boat exploding when it crosses mines.                                                                                                                                                                                                                                                              |
| `explosion.bin`                       | TR1      | Explosion sprites for certain console commands.                                                                                                                                                                                                                                                                                                   |
| `misc_sprites.bin`                    | TR1, TR2 | Various special effects such as snowflake or shadow sprites.                                                                                                                                                                                                                                                                                      |

### Level-specific files 

| Injection file           | Usage    | Purpose                                                                                     |
| ---                      | ---      | ---                                                                                         |
| `*_cameras.bin`          | TR1, TR2 | Positional adjustments for cameras that can otherwise cause visual issues.                  |
| `*_fd.bin`               | TR1, TR2 | Fixes for floor data issues in original levels.                                             |
| `*_itemrots.bin`         | TR1, TR2 | Pickup item rotations for better visuals with 3D pickups.                                   |
| `*_meshfixes.bin`        | TR1      | Miscellaneous mesh adjustments for objects (e.g., to avoid z-fighting).                     |
| `*_music_tracks.bin`     | TR2      | Trigger adjustments to convert music track numbers to match file names (OG levels only).    |
| `*_pickup_meshes.bin`    | TR1, TR2 | Pickup mesh edits (e.g., rescaling keys / specific pickups).                                |
| `*_sfx.bin`              | TR1, TR2 | Various SFX fixes/additions.                                                                |
| `*_skybox.bin`           | TR1      | Predefined skybox injected into specific levels, and specific rooms marked to use it.       |
| `*_textures.bin`         | TR1, TR2 | Texture fixes in original levels (e.g., gaps, wrong colors).                                |
| `cistern_plants.bin`     | TR1      | Disables animation on sprite ID 193 in The Cistern and Tomb of Tihocan.                     |
| `detonator_lights.bin`   | TR2      | Adds animation commands to the Bartoli's Hideout detonator to control the dynamic lighting. |
| `khamoon_mummy.bin`      | TR1      | Mummy in City of Khamoon room 25 (present on PS1, missing on PC).                           |
| `seaweed_collision.bin`  | TR2      | Fixes seaweed in Living Quarters blocking Lara from exiting the water.                      |
| `breakable_tile_sfx.bin` | TR2      | Adds missing breakable tiles (collapsing floor) sounds that are otherwise silent in the OG. |
| `loose_boards_sfx.bin`   | TR2      | Adds missing breakable tiles (collapsing floor) sounds that are otherwise silent in the OG. |
| `dagger_sprite.bin`      | TR2      | Adds a UI sprite for the Dagger of Xian when 3D pickups are disabled.                       |
| `lara_feet_sfx.bin`      | TR1      | Resets Lara's footstep sound effects in the gym level to allow for contextual outfit SFX.   |
