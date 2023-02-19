- [First time setup](#first-time-setup)
- [Firmware backup/update](#firmware-backupupdate)
- [Internal flashcart state](#internal-flashcart-state)
- [Uploading game/save](#uploading-gamesave)
- [Downloading save](#downloading-save)
- [Running 64DD games](#running-64dd-games)
- [Direct boot option](#direct-boot-option)
- [Debug terminal](#debug-terminal)

---

## First time setup

1. Make sure `python3` and `pip3` are installed in your system
2. Grab latest `SC64.zip` package from GitHub releases
3. Extract package contents to a folder
4. Install requirements: `pip3 install -r requirements.txt`
5. Run `python3 sc64.py --help` to check if requirements are installed
6. Run `python3 sc64.py --print-state` to check if SC64 is detected

---

## Firmware backup/update

Keeping SC64 firmware up to date is highly recommended. `sc64.py` script is tightly coupled with specific firmware versions and will error out when it detects unsupported firmware version.

To download and backup current version of SC64 firmware run `python3 sc64.py --backup-firmware sc64_backup_package.bin`

To update SC64 firmware run `python3 sc64.py --update-firmware sc64_update_package.bin`

---

## Internal flashcart state

SC64 holds some configuration after script has exit. To reset it simply run: `python3 sc64.py --reset-state`. Internal flashcart state can be checked by running: `python3 sc64.py --print-state`

---

## Uploading game/save

`python3 sc64.py --boot rom --rom path_to_rom.n64 --save-type eeprom-4k --save path_to_save.sav`

Replace `path_to_rom.n64` / `eeprom-4k` / `path_to_save.sav` with appropriate values for desired game. Check included help in script to check available save types.
Arguments `--save-type` and/or `--save` can be omitted if game doesn't require any save.

---

## Downloading save

`python3 sc64.py --backup-save path_to_save.sav`

Replace `path_to_save.sav` with appropriate value. Specifying save type isn't required when set correctly previously.

---

## Running 64DD games

64DD games require DDIPL ROM and disk images. To run disk game type `python3 sc64.py --boot ddipl --ddipl path_to_ddipl.n64 --disk path_to_disk_1.ndd --disk path_to_disk_2.ndd`.

Replace `path_to_ddipl.n64` / `path_to_disk_x.ndd` with appropriate values. Argument `--disk` can be specified multiple times. Only `.ndd` disk format is supported currently. To change inserted disk press button on the back of SC64 flashcart.

---

## Direct boot option

If booting game through included bootloader isn't a desired option then flashcart can be put in special mode that omits this step.
Run `python3 sc64.py --boot direct-rom --rom path_to_rom.n64` to disable bootloader during boot and console reset. By default `sc64.py` script will try to guess CIC seed and calculate checksum. To change seed or disable CIC use `--cic-params 0x3F,0` argument with appropriate values. Refer to included help in script for values meaning. This option is useful only for very specific cases (e.g. testing custom IPL3 or running SC64 on top of GameShark).

---

## Debug terminal

`sc64.py` supports UNFLoader protocol and has same functionality implemented as aforementioned program. Use argument `--debug` to activate it.
