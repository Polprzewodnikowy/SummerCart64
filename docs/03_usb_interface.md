- [Protocol](#protocol)
  - [Resetting communication](#resetting-communication)
  - [PC -\> SC64 packets](#pc---sc64-packets)
    - [**`CMD`** packet](#cmd-packet)
  - [SC64 -\> PC packets](#sc64---pc-packets)
    - [**`RSP`/`ERR`** packets](#rsperr-packets)
    - [**`PKT`** packet](#pkt-packet)
- [Supported commands](#supported-commands)
  - [`v`: **IDENTIFIER\_GET**](#v-identifier_get)
    - [`response` (identifier)](#response-identifier)
  - [`V`: **VERSION\_GET**](#v-version_get)
    - [`response` (version)](#response-version)
  - [`R`: **STATE\_RESET**](#r-state_reset)
  - [`B`: **CIC\_PARAMS\_SET**](#b-cic_params_set)
    - [`arg0` (cic\_params\_0)](#arg0-cic_params_0)
    - [`arg1` (cic\_params\_1)](#arg1-cic_params_1)
  - [`c`: **CONFIG\_GET**](#c-config_get)
    - [`arg0` (config\_id)](#arg0-config_id)
    - [`response` (config\_value)](#response-config_value)
  - [`C`: **CONFIG\_SET**](#c-config_set)
    - [`arg0` (config\_id)](#arg0-config_id-1)
    - [`arg1` (config\_value)](#arg1-config_value)
  - [`a`: **SETTING\_GET**](#a-setting_get)
    - [`arg0` (setting\_id)](#arg0-setting_id)
    - [`response` (setting\_value)](#response-setting_value)
  - [`A`: **SETTING\_SET**](#a-setting_set)
    - [`arg0` (setting\_id)](#arg0-setting_id-1)
    - [`arg1` (setting\_value)](#arg1-setting_value)
  - [`t`: **TIME\_GET**](#t-time_get)
    - [`response` (time)](#response-time)
  - [`T`: **TIME\_SET**](#t-time_set)
    - [`arg0` (time\_0)](#arg0-time_0)
    - [`arg1` (time\_1)](#arg1-time_1)
- [Asynchronous packets](#asynchronous-packets)

---

## Protocol

SC64 exposes itself in the system as a simple serial device. This allows it to work without manually installed drivers.
Serial communication is also well supported in almost all programming languages making it easy to work with.
USB protocol use simple packet based communication. Every packet starts with 3 byte identifier and 1 byte data ID.
Every argument/data is sent as big-endian value.

### Resetting communication

Due to serial communication nature, data transfer between PC and flashcart might get out of sync (for example when long data transfer is aborted before sending/receiving all bytes).
Communication reset is done via emulated `DTR`/`DSR` pins.
Start reset procedure by setting `DTR` pin value to `high (1)`.
Next, observe `DSR` pin until its state is set to `high (1)`.
At this point communication has been reset and USB interface is disabled. Now it is a good moment to clear/purge any buffers.
Finish reset procedure by setting `DTR` pin value to `low (0)` and observe `DSR` pin until its state is set to `low (0)`.
Flashcart should be ready to accept new commands.

### PC -> SC64 packets

| identifier | description              |
| ---------- | ------------------------ |
| `CMD`      | Send command to the SC64 |

SC64 understands only one packet identifier - `CMD`.
Fourth byte denotes command ID listed in [supported commands](#supported-commands) section.

#### **`CMD`** packet

General structure of packet:
| offset | type                 | description                |
| ------ | -------------------- | -------------------------- |
| `0`    | char[3]              | `CMD` identifier           |
| `3`    | char[1]              | Command ID                 |
| `4`    | uint32_t             | First argument (arg0)      |
| `8`    | uint32_t             | Second argument (arg1)     |
| `12`   | uint8_t[data_length] | Command data (if required) |

`CMD` packet always require arguments to be sent even if command does not require them.
Packet data length is derived from the argument if specific command supports it.

### SC64 -> PC packets

| identifier | description                              |
| ---------- | ---------------------------------------- |
| `RSP`      | Success response to the received command |
| `ERR`      | Error response to the received command   |
| `PKT`      | Asynchronous data packet                 |

SC64 sends response packet `RSP`/`ERR` to almost every command received from the PC.
Fourth byte is the same as in the command that triggered the response.
If command execution was not successful, then `RSP` identifier is replaced by the `ERR` identifier.

SC64 can also send `PKT` packet at any time as a response to action triggered by the N64 or the flashcart itself.
Fourth byte denotes packet ID listed in the [asynchronous packets](#asynchronous-packets) section.

#### **`RSP`/`ERR`** packets

General structure of packet:
| offset | type                 | description            |
| ------ | -------------------- | ---------------------- |
| `0`    | char[3]              | `RSP`/`ERR` identifier |
| `3`    | char[1]              | Command ID             |
| `4`    | uint32_t             | Data length            |
| `8`    | uint8_t[data_length] | Response data (if any) |

`RSP`/`ERR` packet is sent as a response to the command sent by the PC.
`ERR` response might contain no (or undefined) data in the arbitrary data field compared to regular `RSP` packet.

#### **`PKT`** packet

General structure of packet:
| offset | type                 | description          |
| ------ | -------------------- | -------------------- |
| `0`    | char[3]              | `PKT` identifier     |
| `3`    | char[1]              | Packet ID            |
| `4`    | uint32_t             | Data length          |
| `8`    | uint8_t[data_length] | Packet data (if any) |

Available packet IDs are listed in the [asynchronous packets](#asynchronous-packets) section.

---

## Supported commands

| id  | name                                    | arg0         | arg1          | data | response         | description                                                   |
| --- | --------------------------------------- | ------------ | ------------- | ---- | ---------------- | ------------------------------------------------------------- |
| `v` | [**IDENTIFIER_GET**](#v-identifier_get) | ---          | ---           | ---  | identifier       | Get flashcart identifier `SCv2`                               |
| `V` | [**VERSION_GET**](#v-version_get)       | ---          | ---           | ---  | version          | Get flashcart firmware version                                |
| `R` | [**STATE_RESET**](#r-state_reset)       | ---          | ---           | ---  | ---              | Reset flashcart state (CIC params and config options)         |
| `B` | [**CIC_PARAMS_SET**](#b-cic_params_set) | cic_params_0 | cic_params_1  | ---  | ---              | Set CIC emulation parameters (disable/seed/checksum)          |
| `c` | [**CONFIG_GET**](#c-config_get)         | config_id    | ---           | ---  | config_value     | Get config option                                             |
| `C` | [**CONFIG_SET**](#c-config_set)         | config_id    | config_value  | ---  | ---              | Set config option                                             |
| `a` | [**SETTING_GET**](#a-setting_get)       | setting_id   | ---           | ---  | setting_value    | Get persistent setting option                                 |
| `A` | [**SETTING_SET**](#a-setting_set)       | setting_id   | setting_value | ---  | ---              | Set persistent setting option                                 |
| `t` | [**TIME_GET**](#t-time_get)             | ---          | ---           | ---  | time             | Get current RTC value                                         |
| `T` | [**TIME_SET**](#t-time_set)             | time_0       | time_1        | ---  | ---              | Set new RTC value                                             |
| `m` | **MEMORY_READ**                         | address      | length        | ---  | data             | Read data from specified memory address                       |
| `M` | **MEMORY_WRITE**                        | address      | length        | data | ---              | Write data to specified memory address                        |
| `U` | **USB_WRITE**                           | type         | length        | data | N/A              | Send data to be received by app running on N64 (no response!) |
| `D` | **DD_SET_BLOCK_READY**                  | success      | ---           | ---  | ---              | Notify flashcart about 64DD block readiness                   |
| `W` | **WRITEBACK_ENABLE**                    | ---          | ---           | ---  | ---              | Enable save writeback through USB packet                      |
| `p` | **FLASH_WAIT_BUSY**                     | wait         | ---           | ---  | erase_block_size | Wait until flash ready / Get flash block erase size           |
| `P` | **FLASH_ERASE_BLOCK**                   | address      | ---           | ---  | ---              | Start flash block erase                                       |
| `f` | **FIRMWARE_BACKUP**                     | address      | ---           | ---  | status/length    | Backup firmware to specified memory address                   |
| `F` | **FIRMWARE_UPDATE**                     | address      | length        | ---  | status           | Update firmware from specified memory address                 |
| `?` | **DEBUG_GET**                           | ---          | ---           | ---  | debug_data       | Get internal FPGA debug info                                  |
| `%` | **STACK_USAGE_GET**                     | ---          | ---           | ---  | stack_usage      | Get per task stack usage                                      |

---

### `v`: **IDENTIFIER_GET**

**Get flashcart identifier `SCv2`**

_This command does not require arguments or data._

#### `response` (identifier)
| offset | type    | description |
| ------ | ------- | ----------- |
| `0`    | char[4] | Identifier  |

Identifier is always `SCv2` represented in ASCII code.

---

### `V`: **VERSION_GET**

**Get flashcart firmware version**

_This command does not require arguments or data._

#### `response` (version)
| offset | type     | description   |
| ------ | -------- | ------------- |
| `0`    | uint16_t | Major version |
| `2`    | uint16_t | Minor version |
| `4`    | uint32_t | Revision      |

Increment in major version represents breaking API changes.

Increment in minor version represents non breaking API changes.

Increment in revision field represents no API changes, usually it's denoting bugfixes.

---
### `R`: **STATE_RESET**

**Reset flashcart state (CIC params and config options)**

_This command does not require arguments or data._

_This command does not send response data._

This command is used to reset most of the config options to default state (same as on power-up).
Additionally, CIC emulation is enabled and 6102/7101 seed/checksum values are set.

---

### `B`: **CIC_PARAMS_SET**

**Set CIC emulation parameters (disable/seed/checksum)**

#### `arg0` (cic_params_0)
| bits      | description                     |
| --------- | ------------------------------- |
| `[32:25]` | _Unused_                        |
| `[24]`    | Disable CIC                     |
| `[23:16]` | CIC seed (IPL2 and IPL3 stages) |
| `[15:0]`  | Checksum (upper 16 bits)        |

#### `arg1` (cic_params_1)
| bits     | description              |
| -------- | ------------------------ |
| `[31:0]` | Checksum (lower 32 bits) |

_This command does not send response data._

Use this command to set custom seed/checksum CIC values. Very useful for testing custom IPL3 replacements.

---

### `c`: **CONFIG_GET**

**Get config option**

#### `arg0` (config_id)
| bits     | description |
| -------- | ----------- |
| `[31:0]` | Config ID   |

#### `response` (config_value)
| offset | type     | description  |
| ------ | -------- | ------------ |
| `0`    | uint32_t | Config value |

Use this command to get value of config option. Available config options are listed [here](./04_config_options.md).

---

### `C`: **CONFIG_SET**

**Set config option**

#### `arg0` (config_id)
| bits     | description |
| -------- | ----------- |
| `[31:0]` | Config ID   |

#### `arg1` (config_value)
| bits     | description  |
| -------- | ------------ |
| `[31:0]` | Config value |

Use this command to set value of config option. Available config options are listed [here](./04_config_options.md).

---

### `a`: **SETTING_GET**

**Get persistent setting option**

#### `arg0` (setting_id)
| bits     | description |
| -------- | ----------- |
| `[31:0]` | Setting ID  |

#### `response` (setting_value)
| offset | type     | description   |
| ------ | -------- | ------------- |
| `0`    | uint32_t | Setting value |

Use this command to get value of persistent setting option. Available persistent setting options are listed [here](./04_config_options.md).

---

### `A`: **SETTING_SET**

**Set persistent setting option**

#### `arg0` (setting_id)
| bits     | description |
| -------- | ----------- |
| `[31:0]` | Setting ID  |

#### `arg1` (setting_value)
| bits     | description   |
| -------- | ------------- |
| `[31:0]` | Setting value |

Use this command to set value of persistent setting option. Available persistent setting options are listed [here](./04_config_options.md).

---

### `t`: **TIME_GET**

**Get current RTC value**

_This command does not require arguments or data._

#### `response` (time)
| offset | type    | description                          |
| ------ | ------- | ------------------------------------ |
| `0`    | uint8_t | Weekday (1 - 7), 1 represents Monday |
| `1`    | uint8_t | Hours (0 - 23)                       |
| `2`    | uint8_t | Minutes (0 - 59)                     |
| `3`    | uint8_t | Seconds (0 - 59)                     |
| `4`    | uint8_t | _Unused_ (returns zero)              |
| `5`    | uint8_t | Year (0 - 99)                        |
| `6`    | uint8_t | Month (1 - 12)                       |
| `7`    | uint8_t | Day (1 - 31)                         |

Date/time values use the [BCD](https://en.wikipedia.org/wiki/Binary-coded_decimal) format.

---

### `T`: **TIME_SET**

**Set new RTC value**

#### `arg0` (time_0)
| bits      | description                          |
| --------- | ------------------------------------ |
| `[31:24]` | Weekday (1 - 7), 1 represents Monday |
| `[23:16]` | Hours (0 - 23)                       |
| `[15:8]`  | Minutes (0 - 59)                     |
| `[7:0]`   | Seconds (0 - 59)                     |

#### `arg1` (time_1)
| bits      | description    |
| --------- | -------------- |
| `[31:24]` | _Unused_       |
| `[23:16]` | Year (0 - 99)  |
| `[15:8]`  | Month (1 - 12) |
| `[7:0]`   | Day (1 - 31)   |

_This command does not send response data._

Date/time values use the [BCD](https://en.wikipedia.org/wiki/Binary-coded_decimal) format.

## Asynchronous packets

| id  | name                   | data                 | description                                 |
| --- | ---------------------- | -------------------- | ------------------------------------------- |
| `B` | [**BUTTON**]()         | ---                  | Button on the back of the SC64 was pressed  |
| `G` | [**DATA_FLUSHED**]()   | ---                  | Data from `USB_WRITE` command was discarded |
| `U` | [**DEBUG_DATA**]()     | debug_data           | Data sent from the N64                      |
| `D` | [**DISK_REQUEST**]()   | disk_info/block_data | 64DD disk block R/W request                 |
| `I` | [**IS_VIEWER_64**]()   | text                 | IS-Viewer 64 `printf` text                  |
| `S` | [**SAVE_WRITEBACK**]() | save_contents        | Flushed save data                           |
| `F` | [**UPDATE_STATUS**]()  | progress             | Firmware update progress                    |
