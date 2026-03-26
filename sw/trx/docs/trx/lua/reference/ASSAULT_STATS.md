---
title: Assault course stats
order: 15
---

## Assault course module

Module for controlling the gym assault course statistics.

### Functions

- [lua]`trx.assault_stats.add_record(time)`  
    Adds a new record with the given time (in seconds). Increments internal attempt number.

- [lua]`trx.assault_stats.remove_record(record_id)`  
    Removes a record at the given position, with ids starting from 1.

- [lua]`trx.assault_stats.list_records()`  
    Returns a list of record times.  
    Structure:  
    - `time`: time in seconds.
    - `attempt_num`: which attempt this was.
