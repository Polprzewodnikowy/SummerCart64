---
title: Config
order: 8
---

## Config module

Module for querying and modifying engine configuration settings.

### Functions

- [lua]`trx.config.get(key)`  
   Retrieves the current value of the config option specified by `key`.  

- [lua]`trx.config.set(key, value)`  
    Sets the configuration option identified by `key` to `value`. Returns an
    error if the option is unknown or the value has an invalid format. All
    values are currently passed as strings, even for numeric or boolean
    options. Color settings expect a 6-digit hexadecimal string.

    Note that using this API overrides the player's settings. The old value
    isn't stored anywhere – the new value immediately becomes the active
    setting, as if changed directly by the player, and will be remembered
    across game saves and relaunches globally. Because of this, it's best to
    mark any options you plan to modify (like water or fog color, view
    distances, etc.) as frozen or hidden in the gameflow settings.

- [lua]`trx.config.list()`  
   Returns a Lua table mapping each config key to its current value.

   Example - to list all settings and their values in the log file:

   ```lua
   for opt, val in pairs(trx.config.list()) do
     trx.log.info(opt .. " = " .. val)
   end
   ```
