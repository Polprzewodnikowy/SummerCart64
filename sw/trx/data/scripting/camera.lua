local raw = trxc.camera

local getters = {
  pos = raw.get_pos,
  room_num = raw.get_room,
  room = function()
    local room_num = raw.get_room()
    return room_num and trx.rooms[room_num] or nil
  end,
  target_pos = raw.get_target_pos,
  target_room_num = raw.get_target_room,
}

local camera = {
  shake = raw.shake,
}

setmetatable(camera, {
  __index = function(self, key)
    local getter = getters[key]
    return getter and getter() or nil
  end,
  __newindex = function(self, key, value)
    error("Cannot set field '" .. key .. "' on trx.camera", 2)
  end,
})

trx.camera = camera
