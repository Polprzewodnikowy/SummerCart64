# SummerCart64 - a fully open source N64 flashcart
[<img src="assets/sc64_logo.svg" />](assets/sc64_logo.svg)

**For non-technical description of the SummerCart64, please head to the https://summercart64.dev website!**

## Features
 - **Hardware-Dependent Game Features**
   - [UltraCIC_C](https://github.com/jago85/UltraCIC_C) emulation with automatic region switching and programmable seed/checksum values
   - 64DD add-on emulation
   - Battery backed real time clock (RTC)

 - **Game Saves**
   - 64 MiB SDRAM memory for game and save data
   - 16 MiB FLASH memory for bootloader and extended game data
   - EEPROM, SRAM and FlashRAM save types with automatic writeback to SD card (no reset required)

 - **Menu**
   - Dedicated open source menu written specifically for this flashcart - [N64FlashcartMenu](https://github.com/Polprzewodnikowy/N64FlashcartMenu)

 - **Game Development**
   - ~23.8 MiB/s peak transfer rate SD card interface
   - ~23.8 MiB/s peak transfer rate USB interface for data upload/download and debug functionality
   - PC app for communicating with flashcart
     - Game/save data upload/download (feature enable control and debug terminal)

   - IS-Viewer 64 debug interface
   - [UNFLoader](https://github.com/buu342/N64-UNFLoader) support
   - 8 kiB on-chip buffer for general use
   - Status LED and button for general use
   - N64 bootloader with support for IPL3 registers spoofing and loading menu from SD card

 - **Cartridge Production**
   - Initial programming via UART header or dedicated JTAG/SWD interfaces
   - Software and firmware updatable via USB interface
   - 3D printable plastic shell


---

## Documentation

- [Quick startup guide](./docs/00_quick_startup_guide.md)
- [Memory map](./docs/01_memory_map.md)
- [N64 commands](./docs/02_n64_commands.md)
- [USB interface](./docs/03_usb_interface.md)
- [Config options](./docs/04_config_options.md)
- [FW and SW info](./docs/05_fw_and_sw_info.md)
- [Build guide](./docs/06_build_guide.md)

---

## Help / Q&A

For any questions related to this project, please use [*Discussions*](https://github.com/Polprzewodnikowy/SummerCart64/discussions) tab in GitHub repository.
Using discussions tab is highly encouraged as it allows to have centralized knowledge database accessible for everyone interested in this project.

I'm also active at [N64brew](https://discord.gg/WqFgNWf) Discord server as `korgeaux` but keep in mind that [*Discussions*](https://github.com/Polprzewodnikowy/SummerCart64/discussions) tab is a preferred option.

Note that my time is limited so I can't answer all questions.

---

## How do I get one?

Most up to date information about purchasing/manufacturing options is available on https://summercart64.dev website!

If you want to order it yourself then I've prepared all necessary manufacturing files on the [PCBWay Shared Project](https://www.pcbway.com/project/member/shareproject/?bmbno=1046ED64-8AEE-44) site.

**Be careful**: this is an advanced project and it is assumed that you have enough knowledge about electronics.
Selecting wrong options or giving PCB manufacturer wrong information might result in an undesired time and/or money loss.
Boards also come unprogrammed from the manufacturer - you need to do **initial programming step** yourself after receiving the board.
**Price of the components is not included in the initial quote at the checkout** - manufacturer will contact you later with updated price.
To avoid problems _**please**_ read **both** [build guide](./docs/06_build_guide.md) and description on the shared project page **in full**.
If you have even slightest doubt about the ordering or programming process, it is better to leave it to someone experienced - ask in the [n64brew Discord server](https://discord.gg/WqFgNWf) if that's the case.

**Full disclosure**: for every order made through [this link](https://www.pcbway.com/project/member/shareproject/?bmbno=1046ED64-8AEE-44) I will receive 10% of PCB manufacturing and PCB assembly service cost (price of the components is not included in the split). This is a great way of supporting further project development.

If you don't need a physical product but still want to support me then check my [GitHub sponsors](https://github.com/sponsors/Polprzewodnikowy) page.

---

## High-level flashcart block diagram

[<img src="assets/sc64_block_diagram.svg" alt="SC64 block diagram" width="800" />](assets/sc64_block_diagram.svg)

---

## Finished example

[<img src="assets/sc64_finished_example.jpg" alt="SC64 finished example" width="800" />](assets/sc64_finished_example.jpg)

[<img src="assets/sc64_pcb_front.jpg" alt="SC64 PCB front" width="800" />](assets/sc64_pcb_front.jpg)

[<img src="assets/sc64_pcb_back.jpg" alt="SC64 PCB back" width="800" />](assets/sc64_pcb_back.jpg)

---

## Acknowledgement

This project wouldn't be possible without these contributions:

- [64drive](https://64drive.retroactive.be) ([archived](https://web.archive.org/web/20240406215731/https://64drive.retroactive.be/)) orders being on permanent hold long before creating this repository.
- [EverDrive-64 X7](https://krikzz.com/our-products/cartridges/ed64x7.html) being disappointment for homebrew development (slow USB upload, unjustified price and overcomplicated SD card access).
  - Context: Both aforementioned products were priced at $199 in 2020. 64drive features made it a vastly more useful tool for homebrew development.
    Since then, 64drive had never been restocked and EverDrive-64 X7 price was lowered to $159 (as of May 2024).
- [Jan Goldacker (@jago85)](https://github.com/jago85) and his projects:
  - [Brutzelkarte](https://github.com/jago85/Brutzelkarte_FPGA) providing solid base for starting this project and sparking hope for true open source N64 flashcarts.
  - [UltraCIC_C](https://github.com/jago85/UltraCIC_C) reimplementation for easy integration in modern microcontrollers. Thanks also goes to everyone involved in N64 CIC reverse engineering.
- [N64brew](https://discord.gg/WqFgNWf) Discord server community being very helpful during flashcart development.
- [FatFs](http://elm-chan.org/fsw/ff/00index_e.html) FAT32/exFAT library being easiest to integrate in embedded environment.
- [Yakumono's (@LuigiBlood)](https://twitter.com/LuigiBlood) extensive [64DD documentation](https://github.com/LuigiBlood/64dd/wiki) and its implementation in various emulators.
- [Libdragon](https://github.com/DragonMinded/libdragon) open source N64 SDK project and its developers.
- [SERV](https://github.com/olofk/serv) bit-serial 32-bit RISC-V CPU soft core.
