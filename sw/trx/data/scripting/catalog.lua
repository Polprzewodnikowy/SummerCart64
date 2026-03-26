local raw = trxc.catalog

local catalog = {
  objects = raw.objects,
  flip_effects = raw.flip_effects,
  lara_states = raw.lara_states,
  lara_anims = raw.lara_anims,
  music = raw.music,
  samples = raw.samples,
  weapons = raw.weapons,
}

trx.catalog = catalog
