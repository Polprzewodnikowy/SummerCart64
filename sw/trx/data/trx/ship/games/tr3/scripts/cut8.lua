local drink_frame_num = 737
local equipment_set = false

trx.events.after_control(function()
  local lara_item = trx.lara.item
  if lara_item.frame >= drink_frame_num and not equipment_set then
    trx.lara.set_extra_equipment(trx.lara.mesh.hand_r, trx.lara.extra_mesh.drink_can)
    equipment_set = true
  end
end)
