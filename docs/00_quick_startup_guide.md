- [First time setup](#first-time-setup)
- [Firmware backup/update](#firmware-backupupdate)
- [Internal flashcart state](#internal-flashcart-state)
- [Uploading game/save](#uploading-gamesave)
- [Downloading save](#downloading-save)
- [Running 64DD games](#running-64dd-games)
- [Direct boot option](#direct-boot-option)
- [Debug terminal](#debug-terminal)
- [LED blink patters](#led-blink-patters)

---

## First time setup

**Windows platform: replace `./sc64` in examples below with `sc64.exe`**

1. Download the latest `sc64-{os}-{version}` (choose OS matching your system) and `sc64-firmware-{version}.bin` from GitHub releases page
2. Extract `sc64-{os}-{version}` package contents to a folder and place `sc64-firmware-{version}.bin` inside it
3. Update SC64 firmware to the latest version with `./sc64 --update-firmware sc64-firmware-{version}.bin`
4. Run `./sc64 --print-state` to check if SC64 is detected correctly

---

## Firmware backup/update

Keeping SC64 firmware up to date is highly recommended. `sc64` executable is tightly coupled with specific firmware versions and will error out when it detects unsupported firmware version.

To download and backup current version of SC64 firmware run `./sc64 --backup-firmware sc64-firmware-backup.bin`

To update SC64 firmware run `./sc64 --update-firmware sc64-firmware-{version}.bin`

---

## Internal flashcart state

SC64 holds some internal configuration options after `sc64` executable finished running. To reset it simply run: `./sc64 --reset-state`. Internal flashcart state can be checked by running: `./sc64 --print-state`

---

## Uploading game/save

`./sc64 --boot rom --rom path_to_rom.n64 --save-type eeprom-4k --save path_to_save.sav`

Replace `path_to_rom.n64` / `eeprom-4k` / `path_to_save.sav` with appropriate values for desired game. Check included help in program to check available save types.
Arguments `--save-type` and/or `--save` can be omitted if game doesn't require any save.

---

## Downloading save

`./sc64 --backup-save path_to_save.sav`

Replace `path_to_save.sav` with appropriate value. Specifying save type isn't required when set correctly previously.

---

## Running 64DD games

64DD games require DDIPL ROM and disk images. To run disk game type `./sc64 --boot ddipl --ddipl path_to_ddipl.n64 --disk path_to_disk_1.ndd --disk path_to_disk_2.ndd`.

Replace `path_to_ddipl.n64` / `path_to_disk_x.ndd` with appropriate values. Argument `--disk` can be specified multiple times. Only `.ndd` disk format is supported currently. To change inserted disk press button on the back of SC64 flashcart.

---

## Direct boot option

If booting game through included bootloader isn't a desired option then flashcart can be put in special mode that omits this step.
Run `./sc64 --boot direct-rom --rom path_to_rom.n64` to disable bootloader during boot and console reset. By default `sc64` executable will try to guess CIC seed and calculate checksum. To change seed or disable CIC use `--cic-params 0x3F,0` argument with appropriate values. Refer to included help in program for values meaning. This option is useful only for very specific cases (e.g. testing custom IPL3 or running SC64 on top of GameShark).

---

## Debug terminal

`sc64` executable supports UNFLoader protocol and has same functionality implemented as aforementioned program. Use argument `--debug` to activate it.

---

## LED blink patters

LED on SC64 board can blink in certain situations. Most of them during normal use are related to SD card access. Here's list of blink patters meaning:

| Pattern                              | Meaning                                                                                                      |
| ------------------------------------ | ------------------------------------------------------------------------------------------------------------ |
| Nx [Short ON - Short OFF]            | SD card is being accessed (initialization or data read/write) or save writeback is in progress               |
| Nx [Medium ON - Long OFF]            | CIC region didn't match, please power off console and power on again                                         |
| 2x [Very short ON - Short OFF]       | Pattern used during firmware update process, it means that specific part of firmware has started programming |
| 10x [Very short ON - Very short OFF] | Firmware has been successfully updated                                                                       |
| 30x [Long ON - Long OFF]             | There was serious problem during firmware update, device is most likely bricked                              |

Nx means that blink count is varied.

LED blinking on SD card access can be disabled through `sc64` executable. Please refer to included help for option to change the LED behavior.
