local raw = trxc.rooms

-- Room proxy metatable
local getters = {
  num = function(idx)
    return idx
  end,
  underwater = raw.get_underwater,
  wind = raw.get_wind,
  flip_status = raw.get_flip_status,
  flipped_room = function(self, key)
    local flipped_room = raw.get_flipped_room(self)
    if flipped_room == nil then
      return nil
    end
    return trx.rooms[flipped_room]
  end,
  bounds = raw.get_bounds,
  internal_bounds = function(self, key)
    local bounds = raw.get_bounds(self)
    return {
      min_x = bounds.min_x + 1024,
      min_y = bounds.min_y,
      min_z = bounds.min_z + 1024,
      max_x = bounds.max_x - 1024,
      max_y = bounds.max_y,
      max_z = bounds.max_z - 1024,
    }
  end,
}

local setters = {
  underwater = raw.set_underwater,
  wind = raw.set_wind,
}

local Room = {}

Room.__index = function(self, key)
  local getter = getters[key]
  return getter and getter(self.idx) or nil
end
Room.__newindex = function(self, key, value)
  local setter = setters[key]
  if setter then
    setter(self.idx, value)
    return
  end
  error("Cannot set field '" .. key .. "' on trx.items.Room")
end

-- rooms metatable - functions
local fn = {
  FlipStatus = raw.FlipStatus,
  Room = Room,
  flip = raw.flip,
  flip_effect = raw.flip_effect,
}

function fn.get(arg)
  local idx = raw.get(arg)
  if not idx then
    return nil
  end
  return setmetatable({ idx = idx }, Room)
end

trx.rooms = setmetatable({}, {
  __len = function()
    return raw.count()
  end,
  __index = function(_, key)
    if key == "fn" then
      return fn
    elseif key == "flip" or key == "flip_effect" then
      return fn[key]
    elseif type(key) == "number" or type(key) == "string" then
      return fn.get(key)
    end
    return nil
  end,
})
