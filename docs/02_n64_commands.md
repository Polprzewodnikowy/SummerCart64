- [N64 commands](#n64-commands)

---

## N64 commands

| id  | name                  | arg0          | arg1         | rsp0             | rsp1           | description                                                |
| --- | --------------------- | ------------- | ------------ | ---------------- | -------------- | ---------------------------------------------------------- |
| `v` | **IDENTIFIER_GET**    | ---           | ---          | identifier       | ---            | Get flashcart identifier `SCv2`                            |
| `V` | **VERSION_GET**       | ---           | ---          | major/minor      | revision       | Get flashcart firmware version                             |
| `c` | **CONFIG_GET**        | config_id     | ---          | ---              | current_value  | Get config option                                          |
| `C` | **CONFIG_SET**        | config_id     | new_value    | ---              | previous_value | Set config option and get previous value                   |
| `a` | **SETTING_GET**       | setting_id    | ---          | ---              | current_value  | Get persistent setting option                              |
| `A` | **SETTING_SET**       | setting_id    | new_value    | ---              | ---            | Set persistent setting option                              |
| `t` | **TIME_GET**          | ---           | ---          | time_0           | time_1         | Get current RTC value                                      |
| `T` | **TIME_SET**          | time_0        | time_1       | ---              | ---            | Set new RTC value                                          |
| `m` | **USB_READ**          | pi_address    | length       | ---              | ---            | Receive data from USB to flashcart                         |
| `M` | **USB_WRITE**         | pi_address    | length/type  | ---              | ---            | Send data from from flashcart to USB                       |
| `u` | **USB_READ_STATUS**   | ---           | ---          | read_status/type | length         | Get USB read status and type/length                        |
| `U` | **USB_WRITE_STATUS**  | ---           | ---          | write_status     | ---            | Get USB write status                                       |
| `i` | **SD_CARD_OP**        | pi_address    | operation    | ---              | return_data    | Perform special operation on SD card                       |
| `I` | **SD_SECTOR_SET**     | sector        | ---          | ---              | ---            | Set starting sector for next SD card R/W operation         |
| `s` | **SD_READ**           | pi_address    | sector_count | ---              | ---            | Read sectors from SD card to flashcart                     |
| `S` | **SD_WRITE**          | pi_address    | sector_count | ---              | ---            | Write sectors from flashcart to SD card                    |
| `D` | **DISK_MAPPING_SET**  | pi_address    | table_size   | ---              | ---            | Set 64DD disk mapping for SD mode                          |
| `w` | **WRITEBACK_PENDING** | ---           | ---          | pending_status   | ---            | Get save writeback status (is write queued to the SD card) |
| `W` | **WRITEBACK_SD_INFO** | pi_address    | ---          | ---              | ---            | Load writeback SD sector table and enable it               |
| `K` | **FLASH_PROGRAM**     | pi_address    | length       | ---              | ---            | Program flash with bytes loaded into data buffer           |
| `p` | **FLASH_WAIT_BUSY**   | wait          | ---          | erase_block_size | ---            | Wait until flash ready / get block erase size              |
| `P` | **FLASH_ERASE_BLOCK** | pi_address    | ---          | ---              | ---            | Start flash block erase                                    |
| `%` | **DIAGNOSTIC_GET**    | diagnostic_id | ---          | ---              | value          | Get diagnostic data                                        |
