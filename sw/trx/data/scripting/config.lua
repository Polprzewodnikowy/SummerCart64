local raw = trxc.config

local config = {}

function config.get(key)
  return raw.get(key)
end

function config.set(key, value)
  return raw.set(key, value)
end

function config.list()
  return raw.list()
end

trx.config = config
