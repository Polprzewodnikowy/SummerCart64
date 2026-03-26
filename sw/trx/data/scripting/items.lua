local raw = trxc.items

-- Item proxy metatable
local getters = {
  pos = raw.get_pos,
  rot = raw.get_rot,
  anim = raw.get_anim,
  frame = raw.get_frame,
  room_num = raw.get_room,
  room = function(idx)
    return trx.rooms[raw.get_room(idx)]
  end,
  status = raw.get_status,
  flags = raw.get_flags,
  timer = raw.get_timer,
  object_id = raw.get_object_id,
  hit_points = raw.get_hit_points,
  max_hit_points = raw.get_max_hit_points,
  name = raw.get_name,
}

local setters = {
  pos = raw.set_pos,
  rot = raw.set_rot,
  anim = raw.set_anim,
  frame = raw.set_frame,
  hit_points = raw.set_hit_points,
  max_hit_points = raw.set_max_hit_points,
  name = raw.set_name,
  object_id = raw.set_object_id,
}

local Item = {}

Item.__index = function(self, key)
  local getter = getters[key]
  return getter and getter(self.idx) or nil
end

Item.__newindex = function(self, key, value)
  local setter = setters[key]
  if setter then
    setter(self.idx, value)
    return
  end
  error("Cannot set field '" .. key .. "' on trx.items.Item")
end

-- items metatable - functions
local fn = {}

function fn.get(arg)
  local idx = raw.get(arg)
  if not idx then
    return nil
  end
  return setmetatable({ idx = idx }, Item)
end

local find_query_keys = {
  object_id = true,
  room_num = true,
}

local function validate_find_query(query, fn_name)
  if type(query) ~= "table" then
    error("trx.items." .. fn_name .. " query must be a table", 2)
  end

  for key, _ in pairs(query) do
    if not find_query_keys[key] then
      trx.log.warn("trx.items." .. fn_name .. ": unknown property '" .. tostring(key) .. "'")
    end
  end
end

local function is_matching(item, query)
  if query.object_id ~= nil and item.object_id ~= query.object_id then
    return false
  end
  if query.room_num ~= nil and item.room_num ~= query.room_num then
    return false
  end
  return true
end

local function find_items(query, first_only)
  local matches = first_only and nil or {}
  local count = raw.count()
  for i = 1, count do
    local item = setmetatable({ idx = i }, Item)
    if is_matching(item, query) then
      if first_only then
        return item
      end
      table.insert(matches, item)
    end
  end
  return first_only and nil or matches
end

function fn.find(query)
  if query == nil then
    return {}
  end
  validate_find_query(query, "find")
  return find_items(query, false)
end

function fn.first(query)
  if query == nil then
    return nil
  end
  validate_find_query(query, "first")
  return find_items(query, true)
end

-- items metatable - metamethods
trx.items = setmetatable({}, {
  Item = Item,
  __len = function()
    return raw.count()
  end,
  __index = function(_, key)
    if key == "fn" then
      return fn
    elseif key == "find" then
      return fn.find
    elseif key == "first" then
      return fn.first
    elseif type(key) == "number" or type(key) == "string" then
      return fn.get(key)
    end
    return nil
  end,
})
