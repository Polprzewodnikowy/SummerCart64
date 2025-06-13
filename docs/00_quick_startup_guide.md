- [First time setup](#first-time-setup)
  - [Standalone mode (Running menu and games on the N64)](#standalone-mode-running-menu-and-games-on-the-n64)
  - [Developer mode (Uploading ROMs from the PC, and more)](#developer-mode-uploading-roms-from-the-pc-and-more)
    - [Uploading game and/or save from PC](#uploading-game-andor-save-from-pc)
    - [Downloading save to PC](#downloading-save-to-pc)
    - [Running 64DD games from PC](#running-64dd-games-from-pc)
    - [Direct boot option](#direct-boot-option)
    - [Debug terminal on PC](#debug-terminal-on-pc)
    - [Firmware backup/update](#firmware-backupupdate)
- [LED blink patters](#led-blink-patters)

---

# First time setup

## Standalone mode (Running menu and games on the N64)

When the N64 is powered on, it attempts to automatically loaded a menu from the Micro SD card as similar to that used on an 64drive or EverDrive-64. 
The supported Micro SD card filesystem formats are FAT32 and exFAT.
The graphical menu,is developed in another repository: [N64FlashcartMenu](https://github.com/Polprzewodnikowy/N64FlashcartMenu).

> [!TIP]
> Download latest menu version from [here](https://github.com/Polprzewodnikowy/N64FlashcartMenu/releases) and put `sc64menu.n64` file in the root directory of the SD card.

Make sure you read the [documentation]([https://github.com/Polprzeleaseswodnikowy/N64FlashcartMenu/re](https://menu.summercart64.dev/md_docs_200__index.html)) 
for more information about the SD card setup and extra functionality it gives.

---

## Developer mode (Uploading ROMs from the PC, and more)

**Windows platform: replace `./sc64deployer` in examples below with `sc64deployer.exe`**

1. Download the latest deployer tool (`sc64-deployer-{os}-{version}.{ext}`) from the GitHub releases page
2. Extract deployer tool package contents to a folder
3. Connect SC64 device to your computer with USB type C cable
4. Run `./sc64deployer list` to check if device is detected in the system
5. Follow instructions below for specific use cases

### Uploading game and/or save from PC

`./sc64deployer upload path_to_rom.n64 --save-type eeprom4k --save path_to_save.sav`

Replace `path_to_rom.n64` / `eeprom4k` / `path_to_save.sav` with appropriate values for desired game.
Application will try to autodetect used save type so explicitly setting save type usually isn't needed.
Check included help in the application to list available save types.
Arguments `--save-type` and/or `--save` can be omitted if game doesn't require any save or you want to start with fresh save file.

### Downloading save to PC

`./sc64deployer download save path_to_save.sav`

Replace `path_to_save.sav` with appropriate value.
Command will raise error when no save type is currently enabled in the SC64 device.

### Running 64DD games from PC

64DD games require DDIPL ROM and disk images.
To run disk game type `./sc64deployer 64dd path_to_ddipl.n64 path_to_disk_1.ndd path_to_disk_2.ndd`.

Replace `path_to_ddipl.n64` / `path_to_disk_x.ndd` with appropriate values.
Multiple disk files can be passed to the command.
Only `.ndd` disk format is supported.
To change inserted disk press button on the back of SC64 device.
Make sure retail and development disks formats aren't mixed together.
64DD IPL can handle only one drive type at a time.

If disk game supports running in conjunction with cartridge game then `--rom path_to_rom.n64` argument can be added to command above.
N64 will boot cartridge game instead of 64DD IPL.

### Direct boot option

If booting game through included bootloader isn't a desired option then flashcart can be put in special mode that omits this step.
Pass `--direct` option in `upload` or `64dd` command to disable bootloader during boot and console reset.
This option is useful only for very specific cases (e.g. testing custom IPL3 or running SC64 on top of GameShark).
TV type cannot be forced when direct boot mode is enabled.

### Debug terminal on PC

`sc64deployer` application supports UNFLoader protocol and has same functionality implemented as aforementioned program.
Type `./sc64deployer debug` to activate it.

### Firmware backup/update

Keeping SC64 firmware up to date is strongly recommended.
`sc64deployer` application is tightly coupled with specific firmware versions and will error out when it detects unsupported firmware version.

To download and backup current version of the SC64 firmware run `./sc64deployer firmware backup sc64-firmware-backup.bin`

To update SC64 firmware run `./sc64deployer firmware update sc64-firmware-{version}.bin`

To print firmware metadata run `./sc64deployer firmware info sc64-firmware-{version}.bin`

---

# LED blink patters

LED on SC64 board can blink in certain situations. Most of them during normal use are related to SD card access. Here's list of blink patterns:

| Pattern                              | Meaning                                                                                                      |
| ------------------------------------ | ------------------------------------------------------------------------------------------------------------ |
| Irregular                            | SD card access is in progress (initialization or data read/write) or save writeback was finished             |
| Nx [Medium ON - Long OFF]            | CIC region did not match, please power off console and power on again                                        |
| 2x [Very short ON - Short OFF]       | Pattern used during firmware update process, it means that specific part of firmware has started programming |
| 10x [Very short ON - Very short OFF] | Firmware has been successfully updated                                                                       |
| 30x [Long ON - Long OFF]             | There was serious problem during firmware update, device is most likely bricked                              |

Nx means that blink count is varied.

LED blinking on SD card access can be disabled through `sc64deployer` application.
Please refer to the included help for option to change the LED behavior.
