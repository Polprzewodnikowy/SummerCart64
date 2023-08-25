- [First time setup](#first-time-setup)
- [Firmware backup/update](#firmware-backupupdate)
- [Uploading game and/or save](#uploading-game-andor-save)
- [Downloading save](#downloading-save)
- [Running 64DD games](#running-64dd-games)
- [Direct boot option](#direct-boot-option)
- [Debug terminal](#debug-terminal)
- [LED blink patters](#led-blink-patters)

---

## First time setup

**Windows platform: replace `./sc64deployer` in examples below with `sc64deployer.exe`**

1. Download the latest deployer tool (`sc64-deployer-{os}-{version}.{ext}`) and firmware (`sc64-firmware-{version}.bin`) from GitHub releases page
2. Extract deployer tool package contents to a folder and place firmware file inside it
3. Connect SC64 device to your computer with USB type C cable
4. Run `./sc64deployer list` to check if device is detected in the system
5. Update SC64 firmware to the latest version with `./sc64deployer firmware update sc64-firmware-{version}.bin`
6. Run `./sc64deployer info` to check if update process finished successfully and SC64 is detected correctly

---

## Firmware backup/update

Keeping SC64 firmware up to date is highly recommended.
`sc64deployer` application is tightly coupled with specific firmware versions and will error out when it detects unsupported firmware version.

To download and backup current version of the SC64 firmware run `./sc64deployer firmware backup sc64-firmware-backup.bin`

To update SC64 firmware run `./sc64deployer firmware update sc64-firmware-{version}.bin`

To print firmware metadata run `./sc64deployer firmware info sc64-firmware-{version}.bin`

---

## Uploading game and/or save

`./sc64deployer upload path_to_rom.n64 --save-type eeprom4k --save path_to_save.sav`

Replace `path_to_rom.n64` / `eeprom4k` / `path_to_save.sav` with appropriate values for desired game.
Application will try to autodetect used save type so explicitly setting save type usually isn't needed.
Check included help in the application to list available save types.
Arguments `--save-type` and/or `--save` can be omitted if game doesn't require any save or you want to start with fresh save file.

---

## Downloading save

`./sc64deployer download save path_to_save.sav`

Replace `path_to_save.sav` with appropriate value.
Command will raise error when no save type is currently enabled in the SC64 device.

---

## Running 64DD games

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

---

## Direct boot option

If booting game through included bootloader isn't a desired option then flashcart can be put in special mode that omits this step.
Pass `--direct` option in `upload` or `64dd` command to disable bootloader during boot and console reset.
This option is useful only for very specific cases (e.g. testing custom IPL3 or running SC64 on top of GameShark).
TV type cannot be forced when direct boot mode is enabled.

---

## Debug terminal

`sc64deployer` application supports UNFLoader protocol and has same functionality implemented as aforementioned program.
Type `./sc64deployer debug` to activate it.

---

## LED blink patters

LED on SC64 board can blink in certain situations. Most of them during normal use are related to SD card access. Here's list of blink patterns:

| Pattern                              | Meaning                                                                                                      |
| ------------------------------------ | ------------------------------------------------------------------------------------------------------------ |
| Nx [Short ON - Short OFF]            | SD card access is in progress (initialization or data read/write) or save writeback is in progress           |
| Nx [Medium ON - Long OFF]            | CIC region did not match, please power off console and power on again                                        |
| 2x [Very short ON - Short OFF]       | Pattern used during firmware update process, it means that specific part of firmware has started programming |
| 10x [Very short ON - Very short OFF] | Firmware has been successfully updated                                                                       |
| 30x [Long ON - Long OFF]             | There was serious problem during firmware update, device is most likely bricked                              |

Nx means that blink count is varied.

LED blinking on SD card access can be disabled through `sc64deployer` application.
Please refer to included help for option to change the LED behavior.
