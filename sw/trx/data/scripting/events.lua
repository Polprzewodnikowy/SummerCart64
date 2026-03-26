local raw = trxc.events
local types = trxc.events.EventType

local Event = {}
function Event.__call(self, callback)
  local et = self._type
  return raw.attach(et, callback)
end

local function to_event_name(name)
  if string.sub(name, 1, 7) == "BEFORE_" then
    return string.lower(name)
  elseif string.sub(name, 1, 6) == "AFTER_" then
    return string.lower(name)
  end
  return "on_" .. string.lower(name)
end

local events = { EventType = types }
for name, et in pairs(types) do
  local proxy = { _type = et }
  setmetatable(proxy, Event)
  events[to_event_name(name)] = proxy
end

function events.detach(id)
  raw.detach(id)
end

trx.events = events
