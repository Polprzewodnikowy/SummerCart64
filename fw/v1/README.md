# SummerCart64 Firmware

A FPGA firmware written in Verilog for SummerCart64.


# Technical Reference Manual


## Bus peripheral addresses

- **`0x1000 0000 - 0x13FF FFFF`** - [R/W] *SDRAM Memory*
- **`0x1D00 0000 - 0x1D00 07FF`** - [R/W] *EEPROM Memory*
- **`0x1E00 0000 - 0x1E00 0007`** - [R/W] *Cart Registers*


## Memory spaces

### SDRAM Memory

Base address: **`0x1000 0000`**\
Length: **`64 MB`**\
Access: Read or Write, 2 byte (16 bit) aligned

64 MB of SDRAM memory. Available on the bus when **SDRAM** bit in **CART->CR** register is set. Used as ROM storage.

### EEPROM Memory

Base address: **`0x1D00 0000`**\
Length: **`2 kB`**\
Access: Read or Write, 4 byte (32 bit) aligned

2 kB of EEPROM Memory. Available on the bus when **EEPROM_PI** bit in **CART->CR** register is set. Used to upload/download EEPROM contents to/from PC.


## Registers

### Cart (**CART**) registers

Base address: **`0x1E00 0000`**

#### Configuration register (**CR**)

Address offset: **`0x00`**\
Powerup value: **`0b0000 0000`**\
Soft reset value: **`0b00xx x000`**\
Access: Read or write, 4 byte (32 bit) aligned

This register is used to enable or disable various modules available on the cart.

 31:5 | 4          | 3         | 2 | 1        | 0
------|------------|-----------|---|----------|---
 0    | EEPROM_16K | EEPROM_EN | 0 | SDRAM_EN | 0
 x    | R/W        | R/W       | x | R/W      | x

- Bits 31:5: Reserved. Reads as 0, writes are ignored but it's recommended to be set as 0 for future compatibility.
- Bit 4 **EEPROM_16K**: Sets ID returned by EEPROM to identify itself as 4k or 16k variant.
  - 0: EEPROM 4k variant
  - 1: EEPROM 16k variant
- Bit 3 **EEPROM_EN**: Enable EEPROM access through SI bus.
  - 0: EEPROM SI access disabled
  - 1: EEPROM SI access enabled
- Bit 2: Reserved. Reads as 0, writes are ignored but it's recommended to be set as 0 for future compatibility.
- Bit 1 **SDRAM_EN**: Enable SDRAM access at address base **`0x1000 0000`**. When disabled bootloader flash image is mapped at this address. Cleared by hardware on N64 Reset/NMI event.
  - 0: SDRAM disabled
  - 1: SDRAM enabled
- Bit 0: Reserved. Reads as 0, writes are ignored but it's recommended to be set as 0 for future compatibility.
  - 0: Flash disabled
  - 1: Flash enabled

#### CIC and TV type override register (**CIC_TV**)

Address offset: **`0x04`**\
Powerup value: **`0x0000 0000`**\
Access: Read or write, 4 byte (32 bit) aligned

This register is used for PC -> bootloader communication.

 31:6    | 5:4     | 3:0
--------|---------|----------
 SWITCH | TV_TYPE | CIC_TYPE
 R/W    | R/W     | R/W

- Bits 31:6 **SWITCH**: Additional bits that can be passed to bootloader, currently unused.
- Bits 5:4 **TV_TYPE**: Overrides TV type in bootloader. Used only when **CIC_TYPE** value is valid (values 1 - 7).
  - 0: PAL TV type
  - 1: NTSC TV type
  - 2: MPAL TV type
  - 3: No TV type override - use TV type provided by ROM header
- Bits 3:0 **CIC_TYPE**: Overrides CIC type in bootloader.
  - 0: No CIC type override - use CIC type determined from ROM bootcode
  - 1: CIC 5101
  - 2: CIC 6101/7102
  - 3: CIC 6102/7101
  - 4: CIC X103
  - 5: CIC X105
  - 6: CIC X106
  - 7: CIC 8303
  - 8 - 15: Same effect as value 0
