local raw = trxc.lara

local lara = {
  set_extra_equipment = raw.set_extra_equipment,
  clear_equipment = raw.clear_equipment,
  mesh = raw.mesh,
  extra_mesh = raw.extra_mesh,
}

-- Item proxy metatable
local getters = {
  exposure_bar = raw.get_exposure_bar,
  air_bar = raw.get_air_bar,
  outfit = raw.get_outfit,
  holsters_visible = raw.are_holsters_visible,
  has_pistol_weapon = raw.has_pistol_weapon,
  equipped_gun = raw.get_equipped_gun,
  extra_anim = raw.get_extra_anim,
  item = function()
    return trx.items[raw.get_item()]
  end,
  target = function()
    local target = raw.get_target()
    if target == nil then
      return nil
    end
    return trx.items[target]
  end,
}

local setters = {
  exposure_bar = raw.set_exposure_bar,
  air_bar = raw.set_air_bar,
  outfit = raw.set_outfit,
  holsters_visible = raw.set_holsters_visible,
}

local lara_mt = {
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
    error("Cannot set field '" .. key .. "' on trx.lara", 2)
  end,
}

setmetatable(lara, lara_mt)
trx.lara = lara
