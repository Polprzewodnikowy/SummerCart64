# SC64 - an open source Nintendo 64 flashcart

[<img src="assets/sc64_logo_256_160.png" />](assets/sc64_logo_256_160.png)

---

## Features
 - 64 MiB SDRAM memory for game and save data
 - 16 MiB FLASH memory for bootloader and extended game data
 - 8 kiB on-chip buffer for general use
 - ~23.8 MiB/s peak transfer rate USB interface for data upload/download and debug functionality
 - ~23.8 MiB/s peak transfer rate SD card interface
 - EEPROM, SRAM and FlashRAM save types with automatic writeback to SD card
 - Battery backed real time clock (RTC)
 - Status LED and button for general use
 - 64DD add-on emulation
 - IS-Viewer 64 debug interface
 - Software and firmware update via USB interface
 - N64 bootloader with support for IPL3 registers spoofing and loading menu from SD card
 - Enhanced [UltraCIC_C](https://github.com/jago85/UltraCIC_C) emulation with automatic region switching and programmable seed/checksum values
 - PC app for communicating with flashcart (game/save data upload/download, feature enable control and debug terminal)
 - [UNFLoader](https://github.com/buu342/N64-UNFLoader) support
 - 3D printable plastic shell

---

## Documentation

- [Memory map](./docs/01_memory_map.md)
- [USB commands](./docs/02_usb_commands.md)
- [N64 commands](./docs/03_n64_commands.md)
- [Config options](./docs/04_config_options.md)
- [FW/SW building](./docs/05_fw_sw_building.md)
- [Manufacturing guidelines](./docs/06_manufacturing_guidelines.md)

---

## High-level flashcart block diagram

[<img src="assets/sc64_v2_block_diagram.svg" width="800" />](assets/sc64_v2_block_diagram.svg)

---

## Finished sample

[<img src="assets/sc64_v2_example.jpg" alt="SC64 HW ver: 2.0" width="800" />](assets/sc64_v2_example.jpg)
