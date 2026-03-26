---
title: Global properties
order: 0
---

## Global properties
The following properties are in the root of the game flow document and control
various pieces of global behaviour. Currently, the majority of this section
remains distinct for each game.

### TR1

#### Example structure

<details>
<summary>Show snippet</summary>

```json5
{
    "main_menu_picture": "data/titleh.png",
    "savegame_file_fmt": "save_tr1_%02d.dat",
    "water_color": [0.45, 1.0, 1.0],
    "fog_transparency": false,
    "fog_color": [0.0, 0.0, 0.0],
    "fog_start": 22.0,
    "fog_end": 30.0,
    "ambient_tracks": [57, 58, 59, 60],
    "injections": [
        "data/global_injection1.bin",
        "data/global_injection2.bin",
        // etc
    ],
    "convert_dropped_guns": false,
    "enforced_config": {
        "enable_save_crystals": false,
    },
    "hidden_config": [
        "enable_legal",
    ],
    // Optional global Lua script file
    "main_script": "data/scripts/global.lua",
    "levels": [
        {
            "path": "data/gym.phd",
            // etc
        },
    ],
    "cutscenes": [
        {
            "path": "data/cut1.phd",
            // etc
        },
    ],
    "demos": [
        {
            "path": "data/gym.phd",
            // etc
        },
    ],
    "fmvs": [
        {"path": "data/snow.rpl"},
        // etc
    },
}
```

</details>

#### Reference

<table>
  <tr valign="top" align="left">
    <th>Property</th>
    <th>Type</th>
    <th>Description</th>
  </tr>
  <tr valign="top">
    <td>
      <a name="cold-water"></a>
      <code>cold_water</code>
    </td>
    <td>Boolean</td>
    <td>
      Enables an exposure meter for Lara when she is in cold water.
    </td>
  </tr>
  <tr valign="top">
    <td>
      <a name="convert-dropped-guns"></a>
      <code>convert_dropped_guns</code>
    </td>
    <td>Boolean</td>
    <td>
      Forces guns dropped by enemies to be converted to the equivalent ammo
      if Lara already has the gun. See
      <a href="./levels/ITEM_DROPS.md">Item drops</a> for full details.
    </td>
  </tr>
  <tr valign="top">
    <td>
      <a name="extends"></a>
      <code>extends</code>
    </td>
    <td>String</td>
    <td>
      Directory name of the base mod this mod extends. Used for asset fallback
      and engine version resolution. Required for custom mods to appear in the
      Switch Game menu.
    </td>
  </tr>
  <tr valign="top">
    <td>
      <a name="fog-transparency"></a>
      <code>fog_transparency</code>
    </td>
    <td>Boolean</td>
    <td>
      Enables blending distant geometry into skybox rather than a solid color.
    </td>
  </tr>
  <tr valign="top">
    <td>
      <a name="fog-color"></a>
      <code>fog_color</code>
    </td>
    <td>Float array or hex string</td>
    <td>
      Fog color (R, G, B) or `#RRGGBB`. OG uses `#000000`. Will have no effect
      if `fog_transparency` is set to true.
    </td>
  </tr>
  <tr valign="top">
    <td>
      <a name="draw-distance-fade"></a>
      <code>fog_start</code>
    </td>
    <td>Double</td>
    <td>
      The distance (in tiles) at which objects and the world start to fade into
      blackness.
      <ul>
        <li>The default value in OG TR1 is hardcoded to 12.</li>
        <li>The default (disabled) value in TombATI is 72.</li>
      </ul>
    </td>
  </tr>
  <tr valign="top">
    <td>
      <a name="draw-distance-max"></a>
      <code>fog_end</code>
    </td>
    <td>Double</td>
    <td>
      The distance (in tiles) at which objects and the world are clipped away.
      <ul>
        <li>The default value in OG TR1 is hardcoded to 20.</li>
        <li>The default (disabled) value in TombATI is 80.</li>
      </ul>
    </td>
  </tr>
  <tr valign="top">
    <td>
      <a name="enable-tr2-item-drops"></a>
      <code>enable_tr2_item_drops</code>
    </td>
    <td>Boolean</td>
    <td>
      Forces enemies who are placed in the same position as pickup items to
      carry those items and drop them when killed (OG TR2+ behavior). See
      <a href="./levels/ITEM_DROPS.md">Item drops</a> for full details.
    </td>
  </tr>
  <tr valign="top">
    <td><a name="enforced-config"></a>
    <code>enforced_config</code></td>
    <td>String-to-object map</td>
    <td>
      This allows <em>any</em> regular game config setting to be overriden. See
      <a href="./USER_CONFIGURATION.md">User configuration</a> for full details.
    </td>
  </tr>
  <tr valign="top">
    <td><a name="hidden-config"></a>
    <code>hidden_config</code></td>
    <td>String array</td>
    <td>
      This allows <em>any</em> regular game config setting to be hidden from
      the ingame settings dialogs. See <a href="./USER_CONFIGURATION.md">User
      configuration</a> for full details.
    </td>
  </tr>
  <tr valign="top">
    <td><code>injections</code></td>
    <td>String array</td>
    <td>
      Global data injection file paths. Individual levels will inherit these
      unless <code>inherit_injections</code> is set to <code>false</code> on
      those levels. See <a href="../INJECTIONS.md">Injections</a> for full details.
    </td>
  </tr>
  <tr valign="top">
    <td>
      <a name="globe-select-entries"></a>
      <code>globe_select_entries</code>
    </td>
    <td>Object array</td>
    <td>
      Defines up to 6 selectable destinations for the <code>globe_select</code>
      sequence. Each entry is an object with the following keys:
      <ul>
        <li>
          <code>rot</code> (integer array, length 3): target rotation
          (<code>[x, y, z]</code>) in engine angle units.
        </li>
        <li>
          <code>start_level_ordinal</code> (integer): ordinal number of the
          first level for this destination within the main level table.
        </li>
        <li>
          <code>completion_level_ordinal</code> (integer): ordinal number of a
          level that, once completed, marks this destination as completed.
        </li>
        <li>
          <code>prereq_zones</code> (integer array): a list of required
          destination indices to unlock this area (e.g. <code>[0, 1, 2, 4]</code>).
        </li>
        <li>
          <code>mesh_idx</code> (integer): globe mesh index used to represent
          the destination (for rotation/selection and hiding unavailable meshes).
        </li>
      </ul>
    </td>
  </tr>
  <tr valign="top">
    <td><code>levels</code></td>
    <td>Object array<strong>*</strong></td>
    <td>
      This is where the individual level details are defined - see
      <a href="./levels/REGULAR_LEVELS.md">Level properties</a> for full details.
    </td>
  </tr>
  <tr valign="top">
    <td><code>main_script</code></td>
    <td>String</td>
    <td>
      Path to a global Lua script to execute after game initialization, before
      the first level loads.
    </td>
  </tr>
  <tr valign="top">
    <td>
      <a name="name"></a>
      <code>name</code>
    </td>
    <td>String</td>
    <td>
      Human-readable display name for this mod, shown in the Switch Game menu.
      If not set, the directory name is used as a fallback.
    </td>
  </tr>
  <tr valign="top">
    <td><code>main_menu_picture</code></td>
    <td>String<strong>*</strong></td>
    <td>Path to the main menu background image.</td>
  </tr>
  <tr valign="top">
    <td><code>savegame_file_fmt</code></td>
    <td>String<strong>*</strong></td>
    <td>Path pattern to look for the savegame files.</td>
  </tr>
  <tr valign="top">
    <td>
      <a name="water-color"></a>
      <code>water_color</code>
    </td>
    <td>Float array or hex string</td>
    <td>
      Water color (R, G, B) or `#RRGGBB`. 1.0 or `FF` means pass-through, 0.0
      or `00` means completely black color.
      See <a href="../7-WATER_COLORS.md">this table</a> for reference values.</a>
    </td>
  </tr>
  <tr valign="top">
    <td>
      <a name="ambient-tracks"></a>
      <code>ambient_tracks</code>
    </td>
    <td>Integer array</td>
    <td>
      A list of music track IDs, which will be treated as ambient music. If
      Lara crosses a trigger for any of these, it will become the current looped
      track, and will persist on save/load.
    </td>
  </tr>
</table>

**\*** Required property.

### TR2

#### Example structure

<details>
<summary>Show snippet</summary>

```json5
{
    // NOTE: bad changes to this file may result in crashes.
    // Lines starting with double slashes are comments and are ignored.

    "main_menu_picture": "data/images/title_eu.webp",
    "savegame_file_fmt": "save_tr2_%02d.dat",

    "demo_version": false,

    "title": {
        "path": "data/title.tr2",
        "music_track": 60,
        "sequence": [
            {"type": "display_picture", "path": "data/images/legal_eu.webp", "legal": true},
            {"type": "play_fmv", "fmv_id": 0},
            {"type": "play_fmv", "fmv_id": 1},
            {"type": "exit_to_title"},
        ],
    },

    "sfx_path": "main.sfx",
    "injections": [
        "data/injections/pda_model.bin",
        "data/injections/winston_model.bin",
        "data/injections/font.bin",
    ],

    "levels": [
        {
            "path": "data/gym.phd",
            // etc
        },
    ],

    "cutscenes": [
        {
            "path": "data/cut1.phd",
            // etc
        },
    ],

    "demos": [
        {
            "path": "data/gym.phd",
            // etc
        },
    ],

    "fmvs": [
        {"path": "data/snow.rpl"},
        // etc
    ],

    "enforced_config": {
        enable_3d_pickups": false,
    },
    "hidden_config": [
        "enable_save_crystals",
    ],
}
```

</details>

#### Reference

<table>
  <tr valign="top" align="left">
    <th>Property</th>
    <th>Type</th>
    <th>Description</th>
  </tr>
  <tr valign="top">
    <td><code>demo_version</code></td>
    <td>Boolean</td>
    <td>Legacy setting scheduled for removal at a later time.</td>
  </tr>
  <tr valign="top">
    <td><code>main_menu_picture</code></td>
    <td>String<strong>*</strong></td>
    <td>Path to the main menu background image.</td>
  </tr>
  <tr valign="top">
    <td><code>savegame_file_fmt</code></td>
    <td>String<strong>*</strong></td>
    <td>Path pattern to look for the savegame files.</td>
  </tr>
  <tr valign="top">
    <td><code>sfx_path</code></td>
    <td>String</td>
    <td>
      The path to the sound effects (.sfx) file to use in the game.
    </td>
  </tr>
  <tr valign="top">
    <td><code>globe_select_entries</code></td>
    <td>Object array</td>
    <td>
      Defines up to 6 selectable destinations for the <code>globe_select</code>
      sequence. See
      <a href="./GLOBAL_PROPERTIES.md#globe-select-entries">TR1 section</a>
      for the full schema.
    </td>
  </tr>
  <tr valign="top">
    <td>
      <a name="fog-transparency"></a>
      <code>fog_transparency</code>
    </td>
    <td>Boolean</td>
    <td>
      Enables blending distant geometry into skybox rather than a solid color.
    </td>
  </tr>
  <tr valign="top">
    <td>
      <a name="fog-color"></a>
      <code>fog_color</code>
    </td>
    <td>Float array or hex string</td>
    <td>
      Fog color (R, G, B) or `#RRGGBB`. OG uses `#000000`. Will have no effect
      if `fog_transparency` is set to true.
    </td>
  </tr>
  <tr valign="top">
    <td>
      <a name="draw-distance-fade"></a>
      <code>fog_start</code>
    </td>
    <td>Double</td>
    <td>
      The distance (in tiles) at which objects and the world start to fade into
      blackness. The default value in OG TR2 is hardcoded to 12.
    </td>
  </tr>
  <tr valign="top">
    <td>
      <a name="draw-distance-max"></a>
      <code>fog_end</code>
    </td>
    <td>Double</td>
    <td>
      The distance (in tiles) at which objects and the world are clipped away.
      The default value in OG TR2 is hardcoded to 20.
    </td>
  </tr>
  <tr valign="top">
    <td>
      <code>water_color</code>
    </td>
    <td>Float array or hex string</td>
    <td>
      Water color (R, G, B) or `#RRGGBB`. 1.0 or `FF` means pass-through, 0.0
      or `00` means completely black color.
      See <a href="../7-WATER_COLORS.md">this table</a> for reference values.</a>
    </td>
  </tr>
  <tr valign="top">
    <td>
      <code>ambient_tracks</code>
    </td>
    <td>Integer array</td>
    <td>
      A list of music track IDs, which will be treated as ambient music. If
      Lara crosses a trigger for any of these, it will become the current looped
      track, and will persist on save/load.
    </td>
  </tr>
</table>
