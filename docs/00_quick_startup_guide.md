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

1. Make sure `python3` and `pip3` are installed in your system
2. Grab latest `SC64.zip` package from GitHub releases
3. Extract package contents to a folder
4. Install requirements: `pip3 install -r requirements.txt`
5. Run `python3 sc64.py --help` to check if requirements are installed
6. Run `python3 sc64.py --print-state` to check if SC64 is detected
7. Update firmware if `sc64.py` detects unsupported firmware version

---

## Firmware backup/update

Keeping SC64 firmware up to date is highly recommended. `sc64.py` script is tightly coupled with specific firmware versions and will error out when it detects unsupported firmware version.

To download and backup current version of SC64 firmware run `python3 sc64.py --backup-firmware sc64_firmware_backup.bin`

To update SC64 firmware run `python3 sc64.py --update-firmware sc64_firmware.bin`

---

## Internal flashcart state

SC64 holds some configuration after script has exit. To reset it simply run: `python3 sc64.py --reset-state`. Internal flashcart state can be checked by running: `python3 sc64.py --print-state`

---

## Uploading game/save

`python3 sc64.py path_to_rom.n64 --save-type eeprom-4k --save path_to_save.sav`

Replace `path_to_rom.n64` / `eeprom-4k` / `path_to_save.sav` with appropriate values for desired game.
Script will try to autodetect used save type so explicitly setting save type usually isn't needed. Check included help in script to list available save types.
Arguments `--save-type` and/or `--save` can be omitted if game doesn't require any save or you want to start fresh.

---

## Downloading save

`python3 sc64.py --backup-save path_to_save.sav`

Replace `path_to_save.sav` with appropriate value. Specifying save type isn't required when set correctly previously.

---

## Running 64DD games

64DD games require DDIPL ROM and disk images. To run disk game type `python3 sc64.py --ddipl path_to_ddipl.n64 --disk path_to_disk_1.ndd --disk path_to_disk_2.ndd`.

Replace `path_to_ddipl.n64` / `path_to_disk_x.ndd` with appropriate values. Argument `--disk` can be specified multiple times. Only `.ndd` disk format is supported currently. To change inserted disk press button on the back of SC64 flashcart. Make sure retail and development disks aren't mixed together. 64DD IPL can handle only one drive type at a time.

---

## Direct boot option

If booting game through included bootloader isn't a desired option then flashcart can be put in special mode that omits this step.
Run `python3 sc64.py --boot direct-rom path_to_rom.n64` to disable bootloader during boot and console reset. This option is useful only for very specific cases (e.g. testing custom IPL3 or running SC64 on top of GameShark).

---

## Debug terminal

`sc64.py` supports UNFLoader protocol and has same functionality implemented as aforementioned program. Use argument `--debug` to activate it.

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

LED blinking on SD card access can be disabled through `sc64.py` script. Please refer to included help for option to change the LED behavior.
