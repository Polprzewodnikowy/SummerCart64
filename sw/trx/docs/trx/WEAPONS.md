---
title: Weapons
order: 13
---

# Weapons

Lara has a fixed number of weapons as follows. 

- Pistols
- Magnums / Automatic Pistols
- Uzis
- Shotgun
- M16
- Grenade Launcher
- Harpoon Gun
- Flare (not strictly a weapon, but treated similarly by the engine)
- Black Skidoo

The file `cfg/weapons.json5` contains properties for these weapon types, each
described in the table below.

<table>
  <tr valign="top" align="left">
    <th>Property</th>
    <th>Type</th>
    <th>Description</th>
  </tr>
  <tr valign="top">
    <td><code>aim_speed</code></td>
    <td>Integer</td>
    <td>Determines how quickly Lara's arms rotate into position when aiming at a target.</td>
  </tr>
  <tr valign="top">
    <td><code>damage</code></td>
    <td>Integer</td>
    <td>The HP damage value to subtract from targets when struck by this weapon type. This value is doubled when playing either <code>Japanese</code> or <code>Japanese NG+</code> modes.</td>
  </tr>
  <tr valign="top">
    <td><code>draw_frame</code></td>
    <td>Integer</td>
    <td>For rifle type weapons, the relative frame number of the equip animation where the object mesh swap is performed e.g. removing the shotgun from Lara's back and putting it in her hand.</td>
  </tr>
  <tr valign="top">
    <td><code>equip_anim_idx</code></td>
    <td>Integer</td>
    <td>For rifle type weapons, the relative equip animation index of the associated object e.g. <code>O_LARA_SHOTGUN</code>.</td>
  </tr>
  <tr valign="top">
    <td><code>flash_pos</code> / <code>flash_pos_alt</code></td>
    <td>XYZ</td>
    <td>Specifies the offset position where the weapon flash object (<code>O_GUN_FLASH</code> / <code>O_M16_FLASH</code> / <code>O_FLARE_FIRE</code>) will be drawn. <code>flash_pos_alt</code> is used only for discarded flares.</td>
  </tr>
  <tr valign="top">
    <td><code>flash_shade</code></td>
    <td>Integer</td>
    <td>Specifies the shade applied when drawing the weapon flash object (<code>O_GUN_FLASH</code> / <code>O_M16_FLASH</code> / <code>O_FLARE_FIRE</code>).</td>
  </tr>
  <tr valign="top">
    <td><code>flash_color</code></td>
    <td>Float array (length 3)</td>
    <td>Specifies the color applied when drawing the weapon flash object (<code>O_GUN_FLASH</code> / <code>O_M16_FLASH</code> / <code>O_FLARE_FIRE</code>), used in TR3 lighting system.</td>
  </tr>
  <tr valign="top">
    <td><code>glow_color</code></td>
    <td>Float array (length 3)</td>
    <td>Specifies the color applied when drawing the weapon glow object (<code>O_GLOW</code>), used in TR3 lighting system.</td>
  </tr>
  <tr valign="top">
    <td><code>glow_pos</code></td>
    <td>XYZ</td>
    <td>Specifies the additional offset to apply to the glow sprite position.</td>
  </tr>
  <tr valign="top">
    <td><code>flash_time</code></td>
    <td>Integer</td>
    <td>Determines the number of frames to show the weapon flash object (<code>O_GUN_FLASH</code> / <code>O_M16_FLASH</code>) after firing a weapon.</td>
  </tr>
  <tr valign="top">
    <td><code>muzzle_pos</code></td>
    <td>XYZ</td>
    <td>Specifies the additional offset to apply to the muzzle for smoke effects (right hand).</td>
  </tr>
  <tr valign="top">
    <td><code>muzzle_pos_alt</code></td>
    <td>XYZ</td>
    <td>Specifies the additional offset to apply to the muzzle for smoke effects (left hand for dual pistols).</td>
  </tr>
  <tr valign="top">
    <td><code>smoke_count</code></td>
    <td>Integer</td>
    <td>How many smoke effect instances to spawn upon shooting.</td>
  </tr>
  <tr valign="top">
    <td><code>shell_pos</code></td>
    <td>XYZ</td>
    <td>Specifies the additional offset to apply to the gun for shells (right hand).</td>
  </tr>
  <tr valign="top">
    <td><code>shell_pos_alt</code></td>
    <td>XYZ</td>
    <td>Specifies the additional offset to apply to the gun for shells (left hand for dual pistols).</td>
  </tr>
  <tr valign="top">
    <td><code>gun_height</code></td>
    <td>Integer</td>
    <td>Used to determine the start Y position when firing a weapon, and to determine if Lara is too far submerged in water to be able to use a weapon (other than the harpoon).</td>
  </tr>
  <tr valign="top">
    <td><code>is_available</code></td>
    <td>Boolean</td>
    <td>Determines if a weapon can be given to Lara when using item cheats. Pickups for unavailable weapons/flares will still work normally.</td>
  </tr>
  <tr valign="top">
    <td><code>left_angles</code></td>
    <td>Integer array (length 4)</td>
    <td>These values determine if Lara has lost target on her left arm.</td>
  </tr>
  <tr valign="top">
    <td><code>lock_angles</code></td>
    <td>Integer array (length 4)</td>
    <td>These values are used to test if Lara is able to lock on to a target.</td>
  </tr>
  <tr valign="top">
    <td><code>pickup_qty</code></td>
    <td>Integer</td>
    <td>The amount of ammo given for the weapon when equivalent ammo object is picked up.</td>
  </tr>
  <tr valign="top">
    <td><code>pickup_qty_alt</code></td>
    <td>Integer</td>
    <td>As per <code>pickup_qty</code>, but this applies exclusively to flares when playing Japanese NG.</td>
  </tr>
  <tr valign="top">
    <td><code>recoil_frame</code></td>
    <td>Integer</td>
    <td>For pistol type weapons, this value determines when Lara should snap back to the aiming frame after the weapon is fired i.e. Uzis have a lower value than Pistols for faster fire rate.</td>
  </tr>
  <tr valign="top">
    <td><code>right_angles</code></td>
    <td>Integer array (length 4)</td>
    <td>These values determine if Lara has lost target on her right arm.</td>
  </tr>
  <tr valign="top">
    <td><code>sample_num</code></td>
    <td>String</td>
    <td>The sound effect to play when the weapon is fired (see ./SAMPLES.md).</td>
  </tr> 
  <tr valign="top">
    <td><code>shot_accuracy</code></td>
    <td>Integer</td>
    <td>Adds a random factor to angles used when firing a weapon. Higher values mean less accuracy.</td>
  </tr>
  <tr valign="top">
    <td><code>target_dist</code></td>
    <td>Float</td>
    <td>The maximum distance (in world sectors) that a target can be from Lara in order for her to lock on.</td>
  </tr>
  <tr valign="top">
    <td><code>type</code></td>
    <td>String</td>
    <td>
      The category that determines how the gun is handled. Accepted values are as follows.
      <ul>
        <li><code>WEAPON_TYPE_DUAL_PISTOLS</code></li>
        <li><code>WEAPON_TYPE_SINGLE_PISTOL</code></li>
        <li><code>WEAPON_TYPE_RIFLE</code></li>
        <li><code>WEAPON_TYPE_MOUNTED</code></li>
      </ul>
    </td>
  </tr>
  <tr valign="top">
    <td><code>undraw_frame</code></td>
    <td>Integer</td>
    <td>For rifle type weapons, the relative frame number of the unequip animation where the object mesh swap is performed e.g. removing the shotgun from Lara's hand and putting it on her back.</td>
  </tr>
</table>
