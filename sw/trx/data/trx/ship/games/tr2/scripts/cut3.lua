local suit_change_anim = 7
local outfit_changed = false

trx.events.after_control(function()
  local lara_item = trx.lara.item
  if lara_item.anim >= suit_change_anim and not outfit_changed then
    trx.lara.outfit = "tr2_diving_suit"
    outfit_changed = true
  end
end)
