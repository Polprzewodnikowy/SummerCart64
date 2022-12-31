- [USB commands](#usb-commands)

---

## USB commands

| id  | name                   | arg0         | arg1         | data | response         | description                                         |
| --- | ---------------------- | ------------ | ------------ | ---- | ---------------- | --------------------------------------------------- |
| `v` | **VERSION_GET**        | ---          | ---          | ---  | api_version      | Get command API version                             |
| `R` | **STATE_RESET**        | ---          | ---          | ---  | ---              | Reset entire flashcart state                        |
| `B` | **CIC_PARAMS_SET**     | cic_params_0 | cic_params_1 | ---  | ---              | Set CIC mode/seed/version/checksum                  |
| `c` | **CONFIG_GET**         | config_id    | ---          | ---  | current_value    | Get config option                                   |
| `C` | **CONFIG_SET**         | config_id    | new_value    | ---  | ---              | Set config option                                   |
| `t` | **TIME_GET**           | ---          | ---          | ---  | time             | Get current RTC value                               |
| `T` | **TIME_SET**           | time_0       | time_1       | ---  | ---              | Set RTC value                                       |
| `m` | **MEMORY_READ**        | address      | length       | data | ---              | Read data from specified memory address             |
| `M` | **MEMORY_WRITE**       | address      | length       | ---  | data             | Write data to specified memory address              |
| `D` | **DD_SET_BLOCK_READY** | success      | ---          | ---  | ---              | Notify flashcart about 64DD block readiness         |
| `U` | **USB_WRITE**          | type         | length       | data | N/A              | Send data to be received by app running on N64      |
| `f` | **FIRMWARE_BACKUP**    | address      | ---          | ---  | status/length    | Backup firmware to specified memory address         |
| `F` | **FIRMWARE_UPDATE**    | address      | length       | ---  | status           | Update firmware from specified memory address       |
| `p` | **FLASH_WAIT_BUSY**    | ---          | ---          | ---  | erase_block_size | Wait until flash ready / get flash block erase size |
| `P` | **FLASH_ERASE_BLOCK**  | address      | ---          | ---  | ---              | Start flash block erase                             |
| `?` | **DEBUG_GET**          | ---          | ---          | ---  | debug_data       | Get internal FPGA debug info                        |
