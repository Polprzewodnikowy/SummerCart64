- [USB commands](#usb-commands)
- [USB command arguments](#usb-command-arguments)
  - [**TIME_GET**](#t-time_get)
  - [**TIME_SET**](#t-time_set)
  - [**CIC_PARAMS_SET**](#b-cic_params_set)

---

## USB commands

| id  | name                   | arg0         | arg1         | data | response         | description                                                   |
| --- | ---------------------- | ------------ | ------------ | ---- | ---------------- | ------------------------------------------------------------- |
| `v` | **IDENTIFIER_GET**     | ---          | ---          | ---  | identifier       | Get flashcart identifier `SCv2`                               |
| `V` | **VERSION_GET**        | ---          | ---          | ---  | version          | Get flashcart firmware version                                |
| `R` | **STATE_RESET**        | ---          | ---          | ---  | ---              | Reset flashcart state (CIC params and config options)         |
| `B` | [**CIC_PARAMS_SET**](#b-cic_params_set) | cic_params_0 | cic_params_1 | ---  | ---              | Set CIC emulation parameters (disable/seed/checksum)          |
| `c` | **CONFIG_GET**         | config_id    | ---          | ---  | current_value    | Get config option                                             |
| `C` | **CONFIG_SET**         | config_id    | new_value    | ---  | ---              | Set config option                                             |
| `a` | **SETTING_GET**        | setting_id   | ---          | ---  | current_value    | Get persistent setting option                                 |
| `A` | **SETTING_SET**        | setting_id   | new_value    | ---  | ---              | Set persistent setting option                                 |
| `t` | [**TIME_GET**](#t-time_get) | ---          | ---          | ---  | time             | Get current RTC value                                         |
| `T` | [**TIME_SET**](#t-time_set) | time_0       | time_1       | ---  | ---              | Set new RTC value                                             |
| `m` | **MEMORY_READ**        | address      | length       | ---  | data             | Read data from specified memory address                       |
| `M` | **MEMORY_WRITE**       | address      | length       | data | ---              | Write data to specified memory address                        |
| `U` | **USB_WRITE**          | type         | length       | data | N/A              | Send data to be received by app running on N64 (no response!) |
| `D` | **DD_SET_BLOCK_READY** | success      | ---          | ---  | ---              | Notify flashcart about 64DD block readiness                   |
| `W` | **WRITEBACK_ENABLE**   | ---          | ---          | ---  | ---              | Enable save writeback through USB packet                      |
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

| | arg0 | arg1 |
| --- | --- | --- |
| bits | `[63:32]` | `[31:0]` |

---

### `B`: [**CIC_PARAMS_SET**](https://github.com/Polprzewodnikowy/SummerCart64/blob/v2.12.1/sw/controller/src/cic.c#L337)

| bits | description |
| ---  | --- |
| `[56]` | Disable CIC |
| `[55:48]` | CIC seed (IPL2 and IPL3 stages) |
| `[47:0]` | Checksum |

---

### `t`: [**TIME_GET**](https://github.com/Polprzewodnikowy/SummerCart64/blob/v2.12.1/sw/controller/src/cfg.c#L410)

Same as [**TIME_SET**](#t-time_set).

---

### `T`: [**TIME_SET**](https://github.com/Polprzewodnikowy/SummerCart64/blob/v2.12.1/sw/pc/sc64.py#L795)

Date values use the [BCD](https://en.wikipedia.org/wiki/Binary-coded_decimal) format.

([RTC chip specifications](https://ww1.microchip.com/downloads/aemDocuments/documents/MPD/ProductDocuments/DataSheets/MCP7940N-Battery-Backed-I2C-RTCC-with-SRAM-20005010J.pdf))

| bits | description |
| ---  | --- |
| `[63:56]` | Week day, starting at 1 (user-defined representation) |
| `[55:48]` | Hours |
| `[47:40]` | Minutes |
| `[39:32]` | Seconds |
| `[23:16]` | Year (from 0 to 99) |
| `[15:8]` | Month |
| `[7:0]` | Day |
