- [First time setup](#first-time-setup)
- [Check/Reset state](#internal-state)
- [Uploading ROM (and save data)](#uploading-a-romsave)
- [Retriving save data](#downloading-save-data)
- [Uploading 64DD files](#running-ndd-files)
- [Direct boot option](#direct-boot-option)
- [Debug terminal](#debug-terminal)
- [Firmware backup/update](#firmware-backupupdate)

---

## First time setup
In order to use the **SC64**, a ROM (currently) has to be loaded over USB from a compatible system.
The scripts that do this currently rely on Python.

1. Ensure `python3` and `pip3` are installed and available on your system.
2. Grab the latest `SC64.zip` package from GitHub releases.
3. Extract the package contents to a folder and switch to it.
4. Install the requirements using: `pip3 install -r requirements.txt`.
5. Run `python3 sc64.py --help` to check if all requirements are installed.
6. Run `python3 sc64.py --print-state` to check if the **SC64** is detected (assuming you have connected a USB cable between devices).

---

## Internal state

The **SC64** may sometimes hold residual configuration data after a previous script/deployment has exited/ran. 

### Reset
To reset it simply run: `python3 sc64.py --reset-state`. 

### Check
The Internal **SC64** state can be checked by running: `python3 sc64.py --print-state`

---

## Uploading a ROM/save
To upload a ROM (and compatible save data)
`python3 sc64.py --boot rom --rom path_to_rom.n64 --save-type eeprom-4k --save path_to_save.sav`

Replace `path_to_rom.n64` / `eeprom-4k` / `path_to_save.sav` with appropriate values for desired game. Check included help in script to check available save types.
Arguments `--save-type` and/or `--save` can be ommited if game doesn't require any save.

---

## Downloading save data

`python3 sc64.py --backup-save path_to_save.sav`

Replace `path_to_save.sav` with appropriate value. Specifying save type isn't required when set correctly previously.

---

## Running ndd files
The **SC64** is able to run 64DD image files without using a cartridge port!

64DD disks require a `DDIPL` ROM and the associated disk images (see [64dd.org](https://64dd.org/) for examples). 

An example command to run it/them would be: `python3 sc64.py --boot ddipl --ddipl path_to_ddipl.n64 --disk path_to_disk_1.ndd --disk path_to_disk_2.ndd --debug`.

Replace `path_to_ddipl.n64` / `path_to_disk_x.ndd` with appropriate values. Argument `--disk` can be specified multiple times. Only `.ndd` disk format is supported currently. To change inserted disk press button on the back of SC64.

---

## Direct boot option

If booting ROM through the included bootloader isn't a desired option, the SC64 can be put into a special mode that ommits this step.
Run `python3 sc64.py --boot direct --rom path_to_rom.n64` to disable the bootloader during boot and console reset. By default only ROM's with 6102/7101 CIC chips will boot correctly. To change this use the `--cic-params 0,0,0x3F,0x12345678ABCD` argument with appropriate values. Refer to the included help in script for values meaning. This option is only useful for very specific cases (e.g. running SC64 on top of a GameShark/ActionReplay cartridge).

---

## Debug terminal

`sc64.py` supports the UNFLoader protocol and has same functionality implemented as desktop program. Use argument `--debug` to activate it.

---

## Firmware backup/update

### Backup
To backup the current version of **SC64** firmware, run: `python3 sc64.py --backup-firmware sc64_backup.upd`.

### Update
To update the **SC64** firmware run: `python3 sc64.py --update-firmware sc64.upd`
