local raw = trxc.log
local LogLevel = trxc.log.LogLevel

local log = { LogLevel = LogLevel }
function log.generic(level, ...)
  raw.log(level, ...)
end
function log.info(...)
  raw.log(LogLevel.INFO, ...)
end
function log.warn(...)
  raw.log(LogLevel.WARNING, ...)
end
function log.warning(...)
  raw.log(LogLevel.WARNING, ...)
end
function log.error(...)
  raw.log(LogLevel.ERROR, ...)
end
function log.debug(...)
  raw.log(LogLevel.DEBUG, ...)
end

trx.log = log
