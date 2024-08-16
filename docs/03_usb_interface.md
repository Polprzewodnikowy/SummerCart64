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
  - [`m`: **MEMORY\_READ**](#m-memory_read)
    - [`arg0` (address)](#arg0-address)
    - [`arg1` (length)](#arg1-length)
    - [`response` (data)](#response-data)
  - [`M`: **MEMORY\_WRITE**](#m-memory_write)
    - [`arg0` (address)](#arg0-address-1)
    - [`arg1` (length)](#arg1-length-1)
    - [`data` (data)](#data-data)
  - [`U`: **USB\_WRITE**](#u-usb_write)
    - [`arg0` (type)](#arg0-type)
    - [`arg1` (length)](#arg1-length-2)
    - [`data` (data)](#data-data-1)
  - [`X`: **AUX\_WRITE**](#x-aux_write)
    - [`arg0` (data)](#arg0-data)
  - [`D`: **DD\_SET\_BLOCK\_READY**](#d-dd_set_block_ready)
    - [`arg0` (error)](#arg0-error)
  - [`W`: **WRITEBACK\_ENABLE**](#w-writeback_enable)
- [Asynchronous packets](#asynchronous-packets)
  - [`X`: **AUX\_DATA**](#x-aux_data)
    - [`data` (data)](#data-data-2)
  - [`B`: **BUTTON**](#b-button)
  - [`U`: **DATA**](#u-data)
    - [`data` (data)](#data-data-3)
  - [`G`: **DATA\_FLUSHED**](#g-data_flushed)
  - [`D`: **DISK\_REQUEST**](#d-disk_request)
    - [`data` (disk\_info/block\_data)](#data-disk_infoblock_data)
    - [Fields details](#fields-details)
  - [`I`: **IS\_VIEWER\_64**](#i-is_viewer_64)
    - [`data` (text)](#data-text)
  - [`S`: **SAVE\_WRITEBACK**](#s-save_writeback)
    - [`data` (save\_contents)](#data-save_contents)
  - [`F`: **UPDATE\_STATUS**](#f-update_status)
    - [`data` (progress)](#data-progress)
    - [Fields details](#fields-details-1)

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
| `4`    | uint32_t             | Response data length   |
| `8`    | uint8_t[data_length] | Response data (if any) |

`RSP`/`ERR` packet is sent as a response to the command sent by the PC.
`ERR` response might contain no (or undefined) data in the arbitrary data field compared to regular `RSP` packet.

#### **`PKT`** packet

General structure of packet:
| offset | type                 | description          |
| ------ | -------------------- | -------------------- |
| `0`    | char[3]              | `PKT` identifier     |
| `3`    | char[1]              | Packet ID            |
| `4`    | uint32_t             | Packet data length   |
| `8`    | uint8_t[data_length] | Packet data (if any) |

Available packet IDs are listed in the [asynchronous packets](#asynchronous-packets) section.

---

## Supported commands

| id  | name                                            | arg0         | arg1          | data | response         | description                                                    |
| --- | ----------------------------------------------- | ------------ | ------------- | ---- | ---------------- | -------------------------------------------------------------- |
| `v` | [**IDENTIFIER_GET**](#v-identifier_get)         | ---          | ---           | ---  | identifier       | Get flashcart identifier `SCv2`                                |
| `V` | [**VERSION_GET**](#v-version_get)               | ---          | ---           | ---  | version          | Get flashcart firmware version                                 |
| `R` | [**STATE_RESET**](#r-state_reset)               | ---          | ---           | ---  | ---              | Reset flashcart state (CIC params and config options)          |
| `B` | [**CIC_PARAMS_SET**](#b-cic_params_set)         | cic_params_0 | cic_params_1  | ---  | ---              | Set CIC emulation parameters (disable/seed/checksum)           |
| `c` | [**CONFIG_GET**](#c-config_get)                 | config_id    | ---           | ---  | config_value     | Get config option                                              |
| `C` | [**CONFIG_SET**](#c-config_set)                 | config_id    | config_value  | ---  | ---              | Set config option                                              |
| `a` | [**SETTING_GET**](#a-setting_get)               | setting_id   | ---           | ---  | setting_value    | Get persistent setting option                                  |
| `A` | [**SETTING_SET**](#a-setting_set)               | setting_id   | setting_value | ---  | ---              | Set persistent setting option                                  |
| `t` | [**TIME_GET**](#t-time_get)                     | ---          | ---           | ---  | time             | Get current RTC value                                          |
| `T` | [**TIME_SET**](#t-time_set)                     | time_0       | time_1        | ---  | ---              | Set new RTC value                                              |
| `m` | [**MEMORY_READ**](#m-memory_read)               | address      | length        | ---  | data             | Read data from specified memory address                        |
| `M` | [**MEMORY_WRITE**](#m-memory_write)             | address      | length        | data | ---              | Write data to specified memory address                         |
| `U` | [**USB_WRITE**](#u-usb_write)                   | type         | length        | data | N/A              | Send data to be received by app running on N64 (no response!)  |
| `X` | [**AUX_WRITE**](#x-aux_write)                   | data         | ---           | ---  | ---              | Send small auxiliary data to be received by app running on N64 |
| `D` | [**DD_SET_BLOCK_READY**](#d-dd_set_block_ready) | error        | ---           | ---  | ---              | Notify flashcart about 64DD block readiness                    |
| `W` | [**WRITEBACK_ENABLE**](#w-writeback_enable)     | ---          | ---           | ---  | ---              | Enable save writeback through USB packet                       |
| `p` | **FLASH_WAIT_BUSY**                             | wait         | ---           | ---  | erase_block_size | Wait until flash ready / Get flash block erase size            |
| `P` | **FLASH_ERASE_BLOCK**                           | address      | ---           | ---  | ---              | Start flash block erase                                        |
| `f` | **FIRMWARE_BACKUP**                             | address      | ---           | ---  | status/length    | Backup firmware to specified memory address                    |
| `F` | **FIRMWARE_UPDATE**                             | address      | length        | ---  | status           | Update firmware from specified memory address                  |
| `?` | **DEBUG_GET**                                   | ---          | ---           | ---  | debug_data       | Get internal FPGA debug info                                   |
| `%` | **DIAGNOSTIC_GET**                              | ---          | ---           | ---  | diagnostic_data  | Get diagnostic data                                            |

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

_This command does not send response data._

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

_This command does not send response data._

Use this command to set value of persistent setting option. Available persistent setting options are listed [here](./04_config_options.md).

---

### `t`: **TIME_GET**

**Get current RTC value**

_This command does not require arguments or data._

#### `response` (time)
| offset | type    | description                             |
| ------ | ------- | --------------------------------------- |
| `0`    | uint8_t | Weekday (1 - 7), 1 represents Monday    |
| `1`    | uint8_t | Hours (0 - 23)                          |
| `2`    | uint8_t | Minutes (0 - 59)                        |
| `3`    | uint8_t | Seconds (0 - 59)                        |
| `4`    | uint8_t | Century (0 - 7), 0 represents year 1900 |
| `5`    | uint8_t | Year (0 - 99)                           |
| `6`    | uint8_t | Month (1 - 12)                          |
| `7`    | uint8_t | Day (1 - 31)                            |

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
| bits      | description                             |
| --------- | --------------------------------------- |
| `[31:24]` | Century (0 - 7), 0 represents year 1900 |
| `[23:16]` | Year (0 - 99)                           |
| `[15:8]`  | Month (1 - 12)                          |
| `[7:0]`   | Day (1 - 31)                            |

_This command does not send response data._

Date/time values use the [BCD](https://en.wikipedia.org/wiki/Binary-coded_decimal) format.

---

### `m`: **MEMORY_READ**

**Read data from specified memory address**

#### `arg0` (address)
| bits     | description             |
| -------- | ----------------------- |
| `[31:0]` | Starting memory address |

#### `arg1` (length)
| bits     | description                             |
| -------- | --------------------------------------- |
| `[31:0]` | Number of bytes to read from the memory |

#### `response` (data)
| offset | type            | description     |
| ------ | --------------- | --------------- |
| `0`    | uint8_t[length] | Memory contents |

Reads bytes from the specified memory address. Please refer to the [internal memory map](./01_memory_map.md) for available memory ranges.

---

### `M`: **MEMORY_WRITE**

**Write data to specified memory address**

#### `arg0` (address)
| bits     | description             |
| -------- | ----------------------- |
| `[31:0]` | Starting memory address |

#### `arg1` (length)
| bits     | description                            |
| -------- | -------------------------------------- |
| `[31:0]` | Number of bytes to write to the memory |

#### `data` (data)
| offset | type            | description                 |
| ------ | --------------- | --------------------------- |
| `0`    | uint8_t[length] | Data to write to the memory |

_This command does not send response data._

Writes bytes to the specified memory address. Please refer to the [internal memory map](./01_memory_map.md) for available memory ranges.

---

### `U`: **USB_WRITE**

**Send data to be received by app running on N64 (no response!)**

#### `arg0` (type)
| bits     | description |
| -------- | ----------- |
| `[31:8]` | _Unused_    |
| `[7:0]`  | Datatype    |

#### `arg1` (length)
| bits     | description |
| -------- | ----------- |
| `[31:0]` | Data length |

#### `data` (data)
| offset | type            | description    |
| ------ | --------------- | -------------- |
| `0`    | uint8_t[length] | Arbitrary data |

_This command does not send response data._

_This command does not send `RSP`/`ERR` packet response!_

This command notifies N64 that data is waiting to be acknowledged.
If N64 side doesn't acknowledge data via [`m` **USB_READ**](./02_n64_commands.md#m-usb_read) N64 command within 1 second then data is flushed and [`G` **DATA_FLUSHED**](#asynchronous-packets) asynchronous packet is sent to the PC.
If N64 acknowledge the request, then data is written to the flashcart memory to address specified in [`m` **USB_READ**](./02_n64_commands.md) N64 command.

---

### `X`: **AUX_WRITE**

**Send small auxiliary data to be received by app running on N64**

#### `arg0` (data)
| bits     | description |
| -------- | ----------- |
| `[31:0]` | Data        |

_This command does not send response data._

This command puts 32 bits of data to the AUX register accessible from the N64 side, and generates cart interrupt (if enabled).

---

### `D`: **DD_SET_BLOCK_READY**

**Notify flashcart about 64DD block readiness**

#### `arg0` (error)
| bits     | description                |
| -------- | -------------------------- |
| `[31:1]` | _Unused_                   |
| `[0]`    | `0` - Success, `1` - Error |

_This command does not send response data._

This command informs SC64 that 64DD disk block data was successfully (or not) read to the requested buffer or written to the 64DD disk file.
This command must be sent for each incoming [**DISK_REQUEST**](#d-disk_request) asynchronous packet.

---

### `W`: **WRITEBACK_ENABLE**

**Enable save writeback through USB packet**

_This command does not require arguments or data._

_This command does not send response data._

This command is used to enable save writeback module and set its mode to send data to the USB interface instead of writing to the SD card.
To disable save writeback change save type via [**SAVE_TYPE**](./04_config_options.md#6-save_type) config option (setting the same save type as currently enabled will also disable writeback module).
Save data is sent via [**SAVE_WRITEBACK**](#s-save_writeback) asynchronous packet.

---

## Asynchronous packets

| id  | name                                    | data                 | description                                                           |
| --- | --------------------------------------- | -------------------- | --------------------------------------------------------------------- |
| `X` | [**AUX_DATA**](#x-aux_data)             | data                 | Data was written to the `AUX` register from the N64 side              |
| `B` | [**BUTTON**](#b-button)                 | ---                  | Button on the back of the SC64 was pressed                            |
| `U` | [**DATA**](#u-data)                     | data                 | Data sent from the N64                                                |
| `G` | [**DATA_FLUSHED**](#g-data_flushed)     | ---                  | Data from [`U` **USB_WRITE**](#u-usb_write) USB command was discarded |
| `D` | [**DISK_REQUEST**](#d-disk_request)     | disk_info/block_data | 64DD disk block R/W request                                           |
| `I` | [**IS_VIEWER_64**](#i-is_viewer_64)     | text                 | IS-Viewer 64 `printf` text                                            |
| `S` | [**SAVE_WRITEBACK**](#s-save_writeback) | save_contents        | Flushed save data                                                     |
| `F` | [**UPDATE_STATUS**](#f-update_status)   | progress             | Firmware update progress                                              |


---

### `X`: **AUX_DATA**

**Data was written to the `AUX` register from the N64 side**

This packet is sent when N64 writes to the `AUX` register in the SC64 register block.

#### `data` (data)
| offset | type     | description |
| ------ | -------- | ----------- |
| `0`    | uint32_t | Data        |

---

### `B`: **BUTTON**

**Button on the back of the SC64 was pressed**

This packet is sent only when [**BUTTON_MODE**](./04_config_options.md#13-button_mode) config option is set to value "`2` - Button press sends USB packet".

_This packet does not send additional data._

---

### `U`: **DATA**

**Data sent from the N64**

This packet is sent when N64 command [**USB_WRITE**](./02_n64_commands.md#m-usb-write) is executed.

#### `data` (data)
| offset | type                 | description |
| ------ | -------------------- | ----------- |
| `0`    | uint8_t              | Datatype    |
| `1`    | uint24_t             | Data length |
| `4`    | uint8_t[data_length] | Packet data |

---

### `G`: **DATA_FLUSHED**

**Data from [`U` **USB_WRITE**](#u-usb_write) USB command was discarded**

This packet is sent only when data sent with USB command [`U` **USB_WRITE**](#u-usb_write) was not acknowledged by the N64 side with [`m` **USB_READ**](./02_n64_commands.md#m-usb_read) within 1 second time limit.

_This packet does not send additional data._

---

### `D`: **DISK_REQUEST**

**64DD disk block R/W request**

This packet is sent when 64DD mode is set to pass R/W requests to the USB interface with [**DD_SD_ENABLE**](./04_config_options.md#9-dd_sd_enable) config option.
Every disk request packet must be acknowledged by the PC side with [`D` **DD_SET_BLOCK_READY**](#d-dd_set_block_ready) USB command.

#### `data` (disk_info/block_data)
| offset | type                        | description                                          |
| ------ | --------------------------- | ---------------------------------------------------- |
| `0`    | uint32_t                    | Disk command                                         |
| `4`    | uint32_t                    | Memory address                                       |
| `8`    | uint32_t                    | Disk track/head/block                                |
| `12`   | uint8_t[packet_length - 12] | Data to be written to the 64DD disk block (optional) |

#### Fields details

**Disk command**:
| value | description                    |
| ----- | ------------------------------ |
| `1`   | Read data from 64DD disk block |
| `2`   | Write data to 64DD disk block  |

**Memory address**:

Internal SC64 address where data is expected to be written for read command

**Disk track/head/block**:
| bits      | description |
| --------- | ----------- |
| `[31:13]` | _Unused_    |
| `[12:2]`  | Track       |
| `[1]`     | Head        |
| `[0]`     | Block       |

---

### `I`: **IS_VIEWER_64**

**IS-Viewer 64 `printf` text**

This packet is sent when IS-Viewer 64 module is enabled in the SC64 with [**ISV_ADDRESS**](./04_config_options.md#4-isv_address) config option, and data is written to the IS-Viewer 64 buffer.

#### `data` (text)
| offset | type                   | description |
| ------ | ---------------------- | ----------- |
| `0`    | uint8_t[packet_length] | Text        |

---

### `S`: **SAVE_WRITEBACK**

**Flushed save data**

This packet is sent when save writeback module is enabled and set to send data to the USB interface with [`W` **WRITEBACK_ENABLE**](#w-writeback_enable) USB command.
Save data is flushed after 1 second delay from the last write to the save region by the app/game running on the N64.

#### `data` (save_contents)
| offset | type                       | description                                                                              |
| ------ | -------------------------- | ---------------------------------------------------------------------------------------- |
| `0`    | uint32_t                   | Save type (same as in [**SAVE_TYPE**](./04_config_options.md#6-save_type) config option) |
| `4`    | uint8_t[packet_length - 4] | Save data                                                                                |

---

### `F`: **UPDATE_STATUS**

**Firmware update progress**

This packet is sent during firmware update process to indicate progress and errors.

#### `data` (progress)
| offset | type     | description |
| ------ | -------- | ----------- |
| `0`    | uint32_t | Progress    |

#### Fields details

**Progress**:
| value  | description                                             |
| ------ | ------------------------------------------------------- |
| `1`    | Update process has started flashing MCU software        |
| `2`    | Update process has started flashing FPGA firmware       |
| `3`    | Update process has started flashing bootloader software |
| `0x80` | Firmware update process was successful                  |
| `0xFF` | Error encountered during firmware update process        |
