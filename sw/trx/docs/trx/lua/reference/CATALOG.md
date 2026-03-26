---
title: Catalog
order: 13
---

## Catalog module

A convenience module for accessing TRX catalog IDs.

### Structures

- [lua]`trx.catalog.objects`   
  This table contains each TRX object ID. Names match those defined in `catalog_objects.csv`, with the `O_` prefix stripped and lowercased.   
  Examples:   
  - [lua]`if item.object_id == trx.catalog.objects.wolf then ...`

- [lua]`trx.catalog.flip_effects`   
  This table contains each TRX flip effect action ID. Names match those defined in `catalog_item_actions.csv`, with the `ITEM_ACTION_` prefix stripped and lowercased.   
  Examples:   
  - [lua]`trx.rooms.flip_effect(trx.catalog.flip_effects.floor_shake, 10)`

- [lua]`trx.catalog.lara_states`   
  This table contains each TRX Lara state ID. Names match those defined in `catalog_lara_states.csv`, with the `LS_` prefix stripped and lowercased.   
  Examples:   
  - [lua]`if lara.state == trx.catalog.lara_states.run then ...`

- [lua]`trx.catalog.weapons`   
  This table contains each TRX Lara gun type ID. Names match the keys from `weapons.json5`, with the `LGT_` prefix stripped and lowercased.   
  Examples:   
  - [lua]`if trx.lara.equipped_gun == trx.catalog.weapons.desert_eagle then ...`

- [lua]`trx.catalog.lara_anims`   
  This table contains each TRX Lara animation ID. Names match those defined in `catalog_lara_anims.csv`, with the `LA_` prefix stripped and lowercased.   
  Examples:   
  - [lua]`if anim == trx.catalog.lara_anims.jump_forward then ...`

- [lua]`trx.catalog.music`   
  This table contains each TRX music track ID. Names match those defined in `catalog_music.csv`, with the `MX_` prefix stripped and lowercased.   
  Examples:   
  - [lua]`trx.music.play(trx.catalog.music.secret)`

- [lua]`trx.catalog.samples`   
  This table contains each TRX sample ID. Names match those defined in `catalog_samples.csv`, with the `SFX_` prefix stripped and lowercased.   
  Examples:   
  - [lua]`trx.sound.play(trx.catalog.samples.lara_no)`
