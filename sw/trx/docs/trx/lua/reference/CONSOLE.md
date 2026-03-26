---
title: Console
order: 5
---

## Console module

Module for interacting with the developer console.

### Functions

- [lua]`trx.console.log("string1", "string2", ...)`  
  [lua]`trx.console.log.generic(level, ...)`  
  [lua]`trx.console.log.info(...)`  
  [lua]`trx.console.log.error(...)`  
  [lua]`trx.console.log.warn(...)`  
  [lua]`trx.console.log.warning(...)`  
  [lua]`trx.console.log.debug(...)`  
    Logs a line to the developer console with a specific level.

- [lua]`trx.console.eval("string"[, opts])`  
   Evaluates a given string as a developer console command.

   By default, output from commands is silenced and only appears in the
   terminal and the log file. To see output from the commands normally, pass `{ verbose = true }` in `opts`.  

   Example:
   > ```lua
   > trx.console.eval("play 1", { verbose = true })
   > ```
   will play the first level and show an according message in the console log.

- [lua]`trx.console.clear()`  
    Clears the console.
