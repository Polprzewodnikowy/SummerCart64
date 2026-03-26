---
title: Logging
order: 1
---

## Logging module

Lua scripts can log with contextual source info via the global `Log` module.
Each call records the Lua script filename, function name, and line number. The
results are logged to the standard output in the console as well as the
`TRX.log` file in the installation directory.

### Functions

- [lua]`trx.log.info(message)`  
  Logs an information to the terminal output and the log file.

- [lua]`trx.log.warn(message)`  
  Logs a warning to the terminal output and the log file.

- [lua]`trx.log.error(message)`  
  Logs an error to the terminal output and the log file.

- [lua]`trx.log.debug(message)`  
  Logs a debug message to the terminal output and the log file.

### Examples

```lua
trx.log.info("hello from lua")
```
