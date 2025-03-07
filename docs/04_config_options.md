- [Supported config options](#supported-config-options)
  - [`0`: **BOOTLOADER\_SWITCH**](#0-bootloader_switch)
  - [`1`: **ROM\_WRITE\_ENABLE**](#1-rom_write_enable)
  - [`2`: **ROM\_SHADOW\_ENABLE**](#2-rom_shadow_enable)
  - [`3`: **DD\_MODE**](#3-dd_mode)
  - [`4`: **ISV\_ADDRESS**](#4-isv_address)
  - [`5`: **BOOT\_MODE**](#5-boot_mode)
  - [`6`: **SAVE\_TYPE**](#6-save_type)
  - [`7`: **CIC\_SEED**](#7-cic_seed)
  - [`8`: **TV\_TYPE**](#8-tv_type)
  - [`9`: **DD\_SD\_ENABLE**](#9-dd_sd_enable)
  - [`10`: **DD\_DRIVE\_TYPE**](#10-dd_drive_type)
  - [`11`: **DD\_DISK\_STATE**](#11-dd_disk_state)
  - [`12`: **BUTTON\_STATE**](#12-button_state)
  - [`13`: **BUTTON\_MODE**](#13-button_mode)
  - [`14`: **ROM\_EXTENDED\_ENABLE**](#14-rom_extended_enable)
- [Supported persistent setting options](#supported-persistent-setting-options)
  - [`0`: **LED\_ENABLE**](#0-led_enable)

---

## Supported config options

SC64 provides simple flashcart configuration by exposing various options settable by both USB and N64 side.
All options other than **BOOTLOADER_SWITCH** are preserved on console reset or power cycle (only when powered from USB).

| id   | name                    | type    | description                                                             |
| ---- | ----------------------- | ------- | ----------------------------------------------------------------------- |
| `0`  | **BOOTLOADER_SWITCH**   | *bool*  | Switches between bootloader and ROM mapping on PI address `0x1000_0000` |
| `1`  | **ROM_WRITE_ENABLE**    | *bool*  | Enables write access to ROM section                                     |
| `2`  | **ROM_SHADOW_ENABLE**   | *bool*  | Enables overlapping last 128 kiB of ROM section by flash memory         |
| `3`  | **DD_MODE**             | *enum*  | Enables 64DD register/IPL access                                        |
| `4`  | **ISV_ADDRESS**         | *dword* | Sets IS-Viewer 64 watch address                                         |
| `5`  | **BOOT_MODE**           | *enum*  | Controls bootloader behavior                                            |
| `6`  | **SAVE_TYPE**           | *enum*  | Sets supported save type                                                |
| `7`  | **CIC_SEED**            | *word*  | Sets CIC seed value passed by bootloader to IPL3                        |
| `8`  | **TV_TYPE**             | *enum*  | Sets TV type value passed by bootloader to IPL3                         |
| `9`  | **DD_SD_ENABLE**        | *bool*  | Controls 64DD block request passing to USB or SD card                   |
| `10` | **DD_DRIVE_TYPE**       | *enum*  | Sets 64DD drive type                                                    |
| `11` | **DD_DISK_STATE**       | *enum*  | Sets current 64DD disk state                                            |
| `12` | **BUTTON_STATE**        | *bool*  | Gets button press value                                                 |
| `13` | **BUTTON_MODE**         | *enum*  | Sets button press behavior                                              |
| `14` | **ROM_EXTENDED_ENABLE** | *bool*  | Enables access to extended ROM memory located in flash                  |

---

### `0`: **BOOTLOADER_SWITCH**

type: *bool* | default: `1`

- `0` - ROM data is mapped on PI address `0x1000_0000`
- `1` - Bootloader data is mapped on PI address `0x1000_0000`

Used internally to remap ROM data after bootloader is loaded.

---

### `1`: **ROM_WRITE_ENABLE**

type: *bool* | default: `0`

- `0` - ROM write access is disabled
- `1` - ROM write access is enabled

Used by homebrew applications to freely replace data in SDRAM from N64 side.

---

### `2`: **ROM_SHADOW_ENABLE**

type: *bool* | default: `0`

- `0` - Last 128 kiB of ROM section is mapped to SDRAM
- `1` - Last 128 kiB of ROM section is mapped to flash

Last 128 kiB of SDRAM is shared between ROM data and SRAM/FlashRAM save data.
Use this setting for applications requiring all of ROM section space including last 128 kiB.
Check [PI memory map](./01_memory_map.md#pi-memory-map) for more information.

---

### `3`: **DD_MODE**

type: *enum* | default: `0`

- `0` - 64DD support is disabled
- `1` - 64DD registers are mapped on PI address space
- `2` - 64DD IPL is mapped on PI address space
- `3` - Both 64DD registers and IPL are mapped on PI address space

Use this config for setting level of 64DD emulation.

---

### `4`: **ISV_ADDRESS**

type: *dword* | default: `0x0000_0000`

- `0x0000_0000` - IS-Viewer 64 is disabled
- `0x0000_0004` to `0x03FF_FFFC` - IS-Viewer 64 is enabled

Sets IS-Viewer 64 listening address starting from ROM base.
For most applications this offset is fixed at `0x03FF_0000`.
Address must be 4-byte aligned. Command will return error when setting incorrect value.
Small number of games have support for changing this address (for example debug builds of TLoZ: MM).

---

### `5`: **BOOT_MODE**

type: *enum* | default: `0`

- `0` - Load menu from SD card
- `1` - Attempt to boot application loaded in ROM section
- `2` - Attempt to boot 64DD IPL
- `3` - Direct ROM mode, skips bootloader entirely
- `4` - Direct 64DD IPL mode, skips bootloader entirely

Use this setting to change boot behavior.
Value `3` or `4` (direct boot) will always keep **BOOTLOADER_SWITCH** option disabled.
Value `4` will set CIC emulation to 64DD mode

---

### `6`: **SAVE_TYPE**

type: *enum* | default: `0`

- `0` - All saves are disabled
- `1` - EEPROM 4 kib save is enabled
- `2` - EEPROM 16 kib save is enabled
- `3` - SRAM 256 kib save is enabled
- `4` - FlashRAM 1 Mib save is enabled
- `5` - SRAM 768 kib save is enabled
- `6` - SRAM 1 Mib save is enabled
- `7` - FakeFlashRAM 1 Mib save is enabled (write/erase timings are not emulated and erase before write is not required)

Use this setting for selecting save type that will be emulated. Only one save type can be enabled.
Any successful write to this config will disable automatic save writeback to the USB or SD card if previously enabled.

---

### `7`: **CIC_SEED**

type: *word* | default: `0xFFFF`

- `0x0000` to `0x00FF` - Specified CIC seed value will be used
- `0xFFFF` - CIC seed value will be autodetected

Use this setting to force specific CIC seed.
By setting value `0xFFFF` bootloader will try to guess needed values from loaded ROM IPL3.
This setting is not used when **BOOT_MODE** is set to `0` (menu), `3` or `4` (direct boot).

---

### `8`: **TV_TYPE**

type: *enum* | default: `3`

- `0` - PAL TV type will be used
- `1` - NTSC TV type will be used
- `2` - MPAL TV type will be used
- `3` - Console native TV type will be used

Use this setting to force specific TV type.
By setting value `3` bootloader will passthrough TV type native to the console.
This setting is not used when **BOOT_MODE** is set to `3` or `4` (direct boot).

---

### `9`: **DD_SD_ENABLE**

type: *bool* | default: `0`

- `0` - 64DD block requests are passed to USB interface
- `1` - 64DD block requests are passed to SD card

Use this setting to change where 64DD emulation will be passing incoming block R/W requests.

---

### `10`: **DD_DRIVE_TYPE**

type: *enum* | default: `0`

- `0` - Retail 64DD drive type ID is selected
- `1` - Development 64DD drive type ID is selected

Use this setting to change 64DD drive type emulation.

---

### `11`: **DD_DISK_STATE**

type: *enum* | default: `0`

- `0` - 64DD disk state is set to ejected
- `1` - 64DD disk state is set to inserted
- `2` - 64DD disk state is set to changed

Use this setting to change 64DD current disk state.

---

### `12`: **BUTTON_STATE**

type: *bool* | default: `x` | **read-only**

- `0` - Button is not pressed
- `1` - Button is pressed

This is special read-only option for getting button press state.
Setting this option will return error.

---

### `13`: **BUTTON_MODE**

type: *enum* | default: `0`

- `0` - Button press does nothing
- `1` - Button press raises N64 interrupt
- `2` - Button press sends USB packet
- `3` - Button press changes 64DD disk

Use this setting to change button behavior.
Regardless of this setting button state can always be checked by reading **BUTTON_STATE** config.

---

### `14`: **ROM_EXTENDED_ENABLE**

type: *bool* | default: `0`

- `0` - ROM extended PI access is disabled
- `1` - ROM extended PI access is enabled

Use this setting to enable PI access for extended ROM data located inside flash memory.

---

## Supported persistent setting options

These options are similar to config options but state is persisted through power cycles. Setting are kept in RTC backup memory and require battery to be installed for correct operation.

| id  | name           | type   | description                                   |
| --- | -------------- | ------ | --------------------------------------------- |
| `0` | **LED_ENABLE** | *bool* | Enables or disables LED I/O activity blinking |

---

### `0`: **LED_ENABLE**

type: *bool* | default: `1`

- `0` - LED I/O activity blinking is disabled
- `1` - LED I/O activity blinking is enabled

Use this setting to enable or disable LED I/O activity blinking.
