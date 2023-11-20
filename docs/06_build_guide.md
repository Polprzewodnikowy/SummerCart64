- [Step by step guide how to make SC64](#step-by-step-guide-how-to-make-sc64)
  - [**PCB manufacturing data**](#pcb-manufacturing-data)
  - [**PCB requirements**](#pcb-requirements)
  - [**Components**](#components)
  - [**Plastic shell**](#plastic-shell)
  - [**Putting it together**](#putting-it-together)
  - [**Initial programming**](#initial-programming)
  - [**Troubleshooting**](#troubleshooting)
    - [*`primer.py` threw error on `Bootloader -> SC64 FLASH` step*](#primerpy-threw-error-on-bootloader---sc64-flash-step)

---

## Step by step guide how to make SC64

All necessary manufacturing files are packaged in every `sc64-extra-{version}.zip` file in GitHub releases.
Please download latest release before proceeding with the instructions.

---

### **PCB manufacturing data**

   1. Locate KiCad project inside `hw/pcb` folder
   2. Export gerber/drill files
   3. Export BOM spreadsheet
   4. Export PnP placement files
   5. Send appropriate files to the PCB manufacturer

---

### **PCB requirements**

***Before ordering PCBs make sure you select correct options listed below:***

  - PCB thickness: 1.2 mm
  - Surface finish (plating): ENIG (Hard gold for edge connector is recommended but it's very expensive)
  - PCB contains edge connector: yes
  - Beveled edge connector: yes, 45° (other angles also should work OK)

---

### **Components**

  1. Locate interactive BOM file inside `hw/pcb` folder
  2. Order all parts listed in the BOM file or use PCB assembly service together with your PCB order

---

### **Plastic shell**

  1. Locate `.stl` files inside `hw/shell` folder
  2. Use these files in the slicer and 3D printer of your choice or order ready made prints from 3D printing company
  3. Find matching screws, go to discussions tab for community recommendations

---

### **Putting it together**

There are no special requirements for soldering components to board.
All chips and connectors can be soldered with standard manual soldering iron, although hot air station is recommended.
Interactive BOM has every component and its value highlighted on PCB drawings and it's strongly recommended to use during assembly process.
You can skip this step if PCB assembly service was used in previous steps.

---

### **Initial programming**

**Please read the following instructions carefully before proceeding with programming.**

For initial programming you are going to need a PC and a USB to UART (serial) adapter (3.3V signaling is required, e.g. TTL-232R-3V3).
These steps assume you are using modern Windows OS (version 10 or higher).

As for software, here's list of required applications:
 - [FT_PROG](https://ftdichip.com/utilities/#ft_prog) - FTDI FT232H EEPROM programming software
 - [Python 3](https://www.python.org/downloads/) with `pip3` - necessary for initial programming script: `primer.py` (windows install: check option add python to PATH)

Programming must be done in specific order for `primer.py` script to work correctly.

First, program FT232H EEPROM:
 1. Connect SC64 board to the PC with USB-C cable
 2. Locate FT232H EEPROM template `ft232h_config.xml`
 3. Launch `FT_PROG` software
 4. Click on `Scan and parse` if no device has shown up
 5. Right click on SC64 device and choose `Apply Template -> From File`
 6. Select previously located `ft232h_config.xml`
 7. Right click on SC64 device and choose `Program Device`

Your SC64 should be ready for next programming step.

Second, program FPGA, microcontroller and bootloader:
 1. Disconnect SC64 board from power (unplug USB-C cable)
 2. Connect serial UART/TTL adapter to `TX/RX/GND` pads marked on the PCB. Cross connection TX -> RX, RX -> TX!
 3. Connect serial adapter to the PC
 4. Check in device manager which port number `COMx` is assigned to serial adapter
 5. Connect SC64 board to the PC with USB-C cable (***IMPORTANT:*** connect it to the same computer as serial adapter)
 6. Locate `primer.py` script in root folder
 7. Make sure these files are located in the same folder as `primer.py` script: `requirements.txt`, `sc64-firmware-{version}.bin`
 8. Run `pip3 install -r requirements.txt` to install required python packages
 9. Run `python3 primer.py COMx sc64-firmware-{version}.bin` (replace `COMx` with port located in step **4**). On Windows it's python instead of python3.
 10. Follow the instructions on the screen
 11. Wait until programming process has finished (**DO NOT STOP PROGRAMMING PROCESS OR DISCONNECT SC64 BOARD FROM PC**, doing so might irrecoverably break programming through UART header and you would need to program FPGA and/or microcontroller with separate dedicated programming interfaces through *Tag-Connect* connector on the PCB)

Congratulations! Your SC64 flashcart should be ready for use!

---

### **Troubleshooting**

#### *`primer.py` threw error on `Bootloader -> SC64 FLASH` step*

This issue can be attributed to incorrectly programmed FT232H EEPROM in the first programming step.
Check again in `FT_PROG` application if device was configured properly.
Once FPGA and microcontroller has been programmed successfully `primer.py` script needs to be run in special mode.
Please use command `python3 primer.py COMx sc64-firmware-{version}.bin --bootloader-only` to try programming bootloader again.
