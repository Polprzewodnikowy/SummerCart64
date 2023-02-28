- [USB commands](#usb-commands)
- [USB command arguments](#usb-command-arguments)
  - [`T`: **TIME_SET**](#t-time_set)

---

## USB commands

| id  | name                   | arg0         | arg1         | data | response         | description                                                   |
| --- | ---------------------- | ------------ | ------------ | ---- | ---------------- | ------------------------------------------------------------- |
| `v` | **IDENTIFIER_GET**     | ---          | ---          | ---  | identifier       | Get flashcart identifier `SCv2`                               |
| `V` | **VERSION_GET**        | ---          | ---          | ---  | version          | Get flashcart firmware version                                |
| `R` | **STATE_RESET**        | ---          | ---          | ---  | ---              | Reset flashcart state (CIC params and config options)         |
| `B` | **CIC_PARAMS_SET**     | cic_params_0 | cic_params_1 | ---  | ---              | Set CIC emulation parameters (disable/seed/checksum)          |
| `c` | **CONFIG_GET**         | config_id    | ---          | ---  | current_value    | Get config option                                             |
| `C` | **CONFIG_SET**         | config_id    | new_value    | ---  | ---              | Set config option                                             |
| `a` | **SETTING_GET**        | setting_id   | ---          | ---  | current_value    | Get persistent setting option                                 |
| `A` | **SETTING_SET**        | setting_id   | new_value    | ---  | ---              | Set persistent setting option                                 |
| `t` | **TIME_GET**           | ---          | ---          | ---  | time             | Get current RTC value                                         |
| `T` | [**TIME_SET**](#t-time_set)           | time_0       | time_1       | ---  | ---              | Set new RTC value                                             |
| `m` | **MEMORY_READ**        | address      | length       | ---  | data             | Read data from specified memory address                       |
| `M` | **MEMORY_WRITE**       | address      | length       | data | ---              | Write data to specified memory address                        |
| `U` | **USB_WRITE**          | type         | length       | data | N/A              | Send data to be received by app running on N64 (no response!) |
| `D` | **DD_SET_BLOCK_READY** | success      | ---          | ---  | ---              | Notify flashcart about 64DD block readiness                   |
| `p` | **FLASH_WAIT_BUSY**    | wait         | ---          | ---  | erase_block_size | Wait until flash ready / Get flash block erase size           |
| `P` | **FLASH_ERASE_BLOCK**  | address      | ---          | ---  | ---              | Start flash block erase                                       |
| `f` | **FIRMWARE_BACKUP**    | address      | ---          | ---  | status/length    | Backup firmware to specified memory address                   |
| `F` | **FIRMWARE_UPDATE**    | address      | length       | ---  | status           | Update firmware from specified memory address                 |
| `?` | **DEBUG_GET**          | ---          | ---          | ---  | debug_data       | Get internal FPGA debug info                                  |
| `%` | **STACK_USAGE_GET**    | ---          | ---          | ---  | stack_usage      | Get per task stack usage                                      |

---

## USB command arguments

What follows is the description of the command arguments' bit fields.

These arguments are encoded in big-endian, and are laid out as such:

| | arg1 | arg0 |
| --- | --- | --- |
| bits | `[63:32]` | `[31:0]` |

---

### `T`: [**TIME_SET**](https://github.com/Polprzewodnikowy/SummerCart64/blob/f8cb1b20bdc7b7cebbf1c50f7901213b0f705a72/sw/controller/src/cfg.c#L417)

| bits | description |
| ---  | --- |
| `[7:0]` | Seconds |
| `[15:8]` | Minutes |
| `[23:16]` | Hours |
| `[31:24]` | Week day |
| `[39:32]` | Day |
| `[47:40]` | Month |
| `[55:48]` | Year |
