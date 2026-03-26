---
title: Lara's outfits
order: 16
---

# Outfits

In TR1 and TR2 originally, Lara's meshes were taken from the `O_LARA` object,
with mesh swaps being performed as required at runtime using additional objects,
such as `O_LARA_PISTOL`, `O_LARA_SHOTGUN`, `O_LARA_EXTRA` etc. TR3 moved to a
dedicated `O_LARA_SKIN` object, but still depended on the additional gun and
extra mesh swaps, and these remained tightly coupled with the level's outfit.
For example, when putting a shotgun in Lara's hand, the relevant mesh would
include an entire copy of her hand, when in reality only the shotgun was
required. This meant if customizing Lara's gloves, the builder would need to do
so on several different objects.

TRX uses a different skin system, both to allow outfit swaps in-game for players
and to remove unnecessary mesh faces where applicable for a more streamlined
data setup. Custom level builders can define up to 32 outfits; following is a
guide to the data and JSON configuration, and some scenario/workflow examples.

## Data setup

The skin system uses the following objects. These are provided in the
`lara_outfits.bin` injection, and are available to download as a separate WAD
(see [injections](INJECTIONS.md)).

#### `O_LARA_SKIN_SWAP_1`...`O_LARA_SKIN_SWAP_32`
Each of these should contain a distinct Lara model, with the mesh count and bone
order conforming to the standard for Lara. Bone offsets are used (e.g. consider
Bacon Lara's different structure); animations are not used.

#### `O_LARA_SKIN_SWAP_EXTRA`
This object contains various additional meshes for Lara, such as altered torsos
when the TR1 braid is in use, Lara's combat face, and meshes used in extra
animations, such as pulling the dagger in Dragon's Lair. It also contains both
the TR1 and TR2/3 braid.

#### `O_LARA_SKIN_SWAP_GUNS`
This object contains holsters - both empty and equipped with the various guns - 
as well as the guns themselves when they are in Lara's hands or on her back.

#### `O_LARA_SKIN_SWAP_LEGS`
This object contains copies of Lara's legs for each outfit, with holster strap
textures removed. This allows levels such as Home Sweet Home to swap out Lara's
legs when she has no holsters. It is not an essential object to include.

## JSON setup
The file `cfg/outfits.json5` sets up the available outfits, and how they should
behave. The structure of this file is described below.

#### Top-level overview
<table>
  <tr valign="top" align="left">
    <th>Property</th>
    <th>Type</th>
    <th>Description</th>
  </tr>
  <tr valign="top">
    <td><code>outfits</code></td>
    <td>Object map</td>
    <td>
      The keys in this map define the available outfits, and these are the keys
      that should be used in the game-flow. The outfit object is described
      separately below.
    </td>
  </tr>
  <tr valign="top">
    <td><code>extra_meshes</code></td>
    <td>Integer map</td>
    <td>
      This map defines mesh offsets in <code>O_LARA_SKIN_SWAP_EXTRA</code>,
      which are required for various events/paths in the engine.
    </td>
  </tr>
  <tr valign="top">
    <td><code>gun_maps</code></td>
    <td>Object array</td>
    <td>
      These maps dictate which meshes to use in
      <code>O_LARA_SKIN_SWAP_GUNS</code> for a given outfit and gun combination.
    </td>
  </tr>
</table>

### Outfits

<details>
<summary>Show snippet</summary>

```json
"tr1_classic": {
  "name_gs": "dynamic/enums/lara_outfit/tr1_classic",
  "mesh_object": "O_LARA_SKIN_SWAP_2",
  "is_reflective": false,
  "gun_map": 0,
  "combat_face_offset": 1,
  "supports_sunglasses": true,
  "braid": {
    "mode": "BRAID_MODE_TR1_FULL",
    "mesh_offset": 10,
    "gold_offset": 16,
    "hair_pos": {
      "x": 0,
      "y": 20,
      "z": -45,
    },
  },
  "no_holster_offsets": {
    "thigh_r": 1,
    "thigh_l": 2,
  },
  "extra_outfits": {
    "LS_EXTRA_TREX_KILL": "tr1_mauled",
    "LS_EXTRA_MIDAS_KILL": "tr1_golden_lara",
  },
},
```
</details>

<table>
  <tr valign="top" align="left">
    <th>Property</th>
    <th>Type</th>
    <th colspan="2">Description</th>
  </tr>
  <tr valign="top">
    <td><code>name_gs</code></td>
    <td>String</td>
    <td colspan="2">
      The game string key used for localized UI labels for this outfit
      (for example, <code>dynamic/enums/lara_outfit/tr1_classic</code>).
    </td>
  </tr>
  <tr valign="top">
    <td><code>mesh_object</code></td>
    <td>String</td>
    <td colspan="2">
     Indicates which object contains the outfit's meshes and bones.
    </td>
  </tr>
  <tr valign="top">
    <td><code>is_reflective</code></td>
    <td>Boolean</td>
    <td colspan="2">Indicates whether or not the outfit is reflective.</td>
  </tr>
  <tr valign="top">
    <td><code>gun_map</code></td>
    <td>Integer</td>
    <td colspan="2">
      The index into the <code>gun_maps</code> array to use for this outfit.
    </td>
  </tr>
  <tr valign="top">
    <td><code>braid</code></td>
    <td>Object</td>
    <td colspan="2">
      The braid setup specific to this outfit. If omitted, no braid will be
      shown. See the braids section below.
    </td>
  </tr>
  <tr valign="top">
    <td><code>combat_face_offset</code></td>
    <td>Integer</td>
    <td colspan="2">
      The mesh offset in <code>O_LARA_SKIN_SWAP_EXTRA</code> for Lara's combat
      face. <code>-1</code> implies no combat face swap. This mesh is used when
      Lara is firing a weapon (traditionally, the <code>O_LARA_UZI</code> head
      mesh was used).
    </td>
  </tr>
  <tr valign="top">
    <td><code>supports_sunglasses</code></td>
    <td>Boolean</td>
    <td colspan="2">
      Defines whether or not sunglasses can be used with this outfit.
    </td>
  </tr>
  <tr valign="top">
    <td><code>no_holster_offsets</code></td>
    <td>Integer map</td>
    <td colspan="2">
      The mesh offsets in <code>O_LARA_SKIN_SWAP_LEGS</code> to use when Lara's
      holsters aren't visible. Omitting this property infers no mesh swaps.
    </td>
  </tr>
  <tr valign="top">
    <td rowspan="3"><code>extra_outfits</code></td>
    <td rowspan="3">String map</td>
    <td colspan="2">
      Pointers to alternative outfits to use for specific game events. The two
      supported events are as follows. If these are omitted, no swaps will occur
      for the events.
    </td>
  </tr>
  <tr valign="top">
    <td><code>LS_EXTRA_TREX_KILL</code></td>
    <td>When Lara is killed by the T-rex - instant full outfit swap.</td>
  </tr>
  <tr valign="top">
    <td><code>LS_EXTRA_MIDAS_KILL</code></td>
    <td>When Lara steps on the Midas hand - progressive outfit swap.</td>
  </tr>
</table>

### Braids

<table>
  <tr valign="top" align="left">
    <th>Property</th>
    <th>Type</th>
    <th colspan="2">Description</th>
  </tr>
  <tr valign="top" >
    <td rowspan="6"><code>mode</code></td>
    <td rowspan="6">String</td>
    <td colspan="2">Indicates special handling when the braid is active.</td>
  </tr>
  <tr valign="top">
    <td><code>BRAID_MODE_NONE</code></td>
    <td>
      No special treatment (this mode is implied if <code>mode</code> is not
      specified).
    </td>
  </tr>
  <tr valign="top">
    <td><code>BRAID_MODE_TR1_HEAD_ONLY</code></td>
    <td>
      Replaces Lara's head with <code>EXTRA_MESH_TR1_BRAID_DEFAULT_HEAD</code>
      defined in the <code>O_LARA_SKIN_SWAP_EXTRA</code> object.
    </td>
  </tr>
  <tr valign="top">
    <td><code>BRAID_MODE_TR1_FULL</code></td>
    <td>
      As per <code>BRAID_MODE_TR1_HEAD_ONLY</code>, plus Lara's torso will be
      replaced with <code>EXTRA_MESH_TR1_BRAID_DEFAULT_TORSO</code>.
    </td>
  </tr>
  <tr valign="top">
    <td><code>BRAID_MODE_TR1_MAULED</code></td>
    <td>
      As per <code>BRAID_MODE_TR1_FULL</code>, but the torso swap mesh used here
      is <code>EXTRA_MESH_TR1_BRAID_MAULED_TORSO</code>.
    </td>
  </tr>
  <tr valign="top">
    <td><code>BRAID_MODE_TR1_GOLD</code></td>
    <td>
      As per <code>BRAID_MODE_TR1_FULL</code>, but the swap meshes used here
      are <code>EXTRA_MESH_TR1_BRAID_GOLD_HEAD</code> and
      <code>EXTRA_MESH_TR1_BRAID_GOLD_TORSO</code>.
    </td>
  </tr>
  <tr valign="top">
    <td><code>mesh_offset</code></td>
    <td>Integer</td>
    <td colspan="2">
      The starting offset in <code>O_LARA_SKIN_SWAP_EXTRA</code> for the regular
      braid meshes and bones.
    </td>
  </tr>
  <tr valign="top">
    <td><code>gold_offset</code></td>
    <td>Integer</td>
    <td colspan="2">
      The starting offset in <code>O_LARA_SKIN_SWAP_EXTRA</code> for the golden
      braid meshes and bones.
    </td>
  </tr>
  <tr valign="top">
    <td><code>hair_pos</code></td>
    <td>XYZ</td>
    <td colspan="2">
      The position relative to Lara's head where the braid will be drawn.
    </td>
  </tr>
</table>

### Guns

<details>
<summary>Show snippet</summary>

```json
{
  "LGT_DESERT_EAGLE": {
    "hand_r": 62,
    "thigh_r": 9,
  },
  "LGT_UZIS": {
    "hand_r": 63,
    "hand_l": 64,
    "thigh_r": 10,
    "thigh_l": 11,
  },
  "LGT_SHOTGUN": {
    "hand_r": 65,
    "torso": 72,
  },
}
```
</details>

The map keys must match known engine weapons. See [weapons](WEAPONS.md) and
`cfg/weapons.json5` for reference. Any entry may omit hand, thigh or torso, and
note that specific gun types will only look for particular entries. For example,
defining a `thigh_l` property for `LGT_SHOTGUN` is meaningless and will be
ignored. Any missing fields imply that no mesh is drawn for that slot.

<table>
  <tr valign="top" align="left">
    <th>Property</th>
    <th>Type</th>
    <th>Description</th>
  </tr>
  <tr valign="top">
    <td>
      <code>hand_r</code><br/>
      <code>hand_l</code>
    </td>
    <td>Integer</td>
    <td>
      The mesh offset in <code>O_LARA_SKIN_SWAP_GUNS</code> for the gun to draw
      in Lara's hands.
    </td>
  </tr>
  <tr valign="top">
    <td>
      <code>thigh_r</code><br/>
      <code>thigh_l</code>
    </td>
    <td>Integer</td>
    <td>
      The mesh offset in <code>O_LARA_SKIN_SWAP_GUNS</code> for the gun to draw
      against Lara's thighs. This expects the holster to be part of the mesh.
    </td>
  </tr>
  <tr valign="top">
    <td>
      <code>torso</code>
    </td>
    <td>Integer</td>
    <td>
      The mesh offset in <code>O_LARA_SKIN_SWAP_GUNS</code> for the gun to draw
      on Lara's back.
    </td>
  </tr>
</table>

## Custom level use cases

> I don't need to customize Lara's outfit, and I don't mind players freely
switching outfits.

In this scenario, simply ship your level with `lara_outfits.bin` and the default
`cfg/outfits.json5`.

***

> I don't need to customize Lara's outfit, nor do I want the player to change
it.

In this case, you can simply enforce the outfit option, and continue to ship the
standard files as above. Provided your game-flow level has an accurate
`lara_outfit` entry, you can enforce as follows.

```json
"enforced_config": {
  "lara_outfit": null,
},
```

Using null here means that if your second level uses a different outfit, that
will still be honoured.

***

> I don't need to customize Lara's outfits, but I want to restrict the ones that
can be selected by the player.

Ship the default injection file, but edit `cfg/outfits.json5` by removing the 
entries from the `outfits` section that you do not need.

***

> I want to customize Lara's outfits.

Download the provided TRX WAD to access the data included in the shipped
injection. Move each of the relevant objects to your WAD. You can then proceed
to edit the meshes as required. Ensure that you remove the `lara_outfits.bin`
from your game-flow, otherwise these will override the data in your level.

> I want to customize Lara's braid.

Braid meshes and bones are taken from the `O_LARA_SKIN_SWAP_EXTRA` object, so to
customize it you will need to follow the same steps as for customizing outfits
i.e. import the TRX WAD, edit the data and remove the injection.
