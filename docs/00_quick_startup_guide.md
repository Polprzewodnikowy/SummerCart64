- [First time setup](#first-time-setup)
- [Internal flashcart state](#internal-flashcart-state)
- [Uploading game/save](#uploading-gamesave)
- [Downloading save](#downloading-save)
- [Running 64DD games](#running-64dd-games)
- [Direct boot option](#direct-boot-option)
- [Debug terminal](#debug-terminal)
- [Firmware backup/update](#firmware-backupupdate)

---

## First time setup

1. Make sure `python3` and `pip3` are installed in your system
2. Grab latest `SC64.zip` package from GitHub releases
3. Extract package contents to a folder
4. Install requirements: `pip3 install -r requirements.txt`
5. Run `python3 sc64.py --help` to check if requirements are installed
6. Run `python3 sc64.py --print-state` to check if SC64 is detected

---

## Internal flashcart state

SC64 holds some configuration after script has exit. To reset it simply run: `python3 sc64.py --reset-state`. Internal flashcart state can be checked by running: `python3 sc64.py --print-state`

---

## Uploading game/save

`python3 sc64.py --boot rom --rom path_to_rom.n64 --save-type eeprom-4k --save path_to_save.sav`

Replace `path_to_rom.n64` / `eeprom-4k` / `path_to_save.sav` with appropriate values for desired game. Check included help in script to check available save types.
Arguments `--save-type` and/or `--save` can be ommited if game doesn't require any save.

---

## Downloading save

`python3 sc64.py --backup-save path_to_save.sav`

Replace `path_to_save.sav` with appropriate value. Specifying save type by `--save` argument isn't required when set correctly previously.

---

## Running 64DD games

64DD games require DDIPL ROM and disk images. To run disk game type `python3 sc64.py --boot ddipl --ddipl path_to_ddipl.n64 --disk path_to_disk_1.ndd --disk path_to_disk_2.ndd --debug`.

Replace `path_to_ddipl.n64` / `path_to_disk_x.ndd` with appropriate values. Argument `--disk` can be specified multiple times. Only `.ndd` disk format is supported currently. To change inserted disk press button on the back of SC64 flashcart.

---

## Direct boot option

If booting game through included bootloader isn't a desired option then flashcart can be put in special mode that ommits this step.
Run `python3 sc64.py --boot direct --rom path_to_rom.n64` to disable bootloader during boot and console reset. By default only games with 6102/7101 CIC chips will boot correctly. To change this use `--cic-params 0,0,0x3F,0x12345678ABCD` argument with appropriate values. Refer to included help in script for values meaning.

---

## Debug terminal

`sc64.py` supports UNFLoader protocol and has same functionality implemented as desktop program. Use argument `--debug` to activate it.

---

## Firmware backup/update

To download and backup current version of SC64 firmware run `python3 sc64.py --backup-firmware sc64_backup.upd`.

To update SC64 firmware run `python3 sc64.py --update-firmware sc64.upd`
