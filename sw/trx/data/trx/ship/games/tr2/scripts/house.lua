trx.events.on_game_start(function(level, is_save)
  trx.lara.holsters_visible = trx.lara.has_pistol_weapon
  if trx.lara.extra_anim == -1 then -- TODO: remove in TRX 1.5.
    trx.lara.set_extra_equipment(trx.lara.mesh.hips, trx.lara.extra_mesh.dagger_hips)
  end
end)
