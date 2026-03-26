trx.events.on_game_start(function(level, is_save)
  trx.lara.holsters_visible = trx.lara.has_pistol_weapon -- TODO: remove in TRX 1.5.
  if is_save then
    return
  end
  local records = trx.assault_stats.list_records()
  if #records > 1 then
    trx.music.play(22)
  else
    trx.music.play(5)
  end
end)
