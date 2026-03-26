local raw = trxc.creatures

local creatures = {
  add_ally = raw.add_ally,
  add_ally_target = raw.add_ally_target,
}

local getters = {
  hostile_allies = raw.are_allies_hostile,
}

local setters = {
  hostile_allies = raw.set_allies_hostile,
}

local creatures_mt = {
  __index = function(self, key)
    local getter = getters[key]
    return getter and getter() or nil
  end,
  __newindex = function(self, key, value)
    local setter = setters[key]
    if setter then
      setter(value)
      return
    end
    error("Cannot set field '" .. key .. "' on trx.creatures", 2)
  end,
}

setmetatable(creatures, creatures_mt)
trx.creatures = creatures
