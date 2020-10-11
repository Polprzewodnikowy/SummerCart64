# SummerCart64 Firmware

A FPGA firmware written in Verilog for SummerCart64.

# Technical Reference Manual

## Bus peripheral addresses

- **`0x1000 0000 - 0x13FF FFFF`** - [R/W] *SDRAM Memory*
- **`0x1000 0000 - 0x10FF FFFF`** - [R] *Flash Memory* (no remap)
- **`0x1800 0000 - 0x18FF FFFF`** - [R] *Flash Memory* (remapped)
- **`0x1C00 0000 - 0x1C00 0000`** - [R/W] *Flash Registers*
- **`0x1D00 0000 - 0x1D00 07FF`** - [R/W] *EEPROM Memory*
- **`0x1D10 0000 - 0x1D10 03FF`** - [R] *Debug RX FIFO* (unimplemented)
- **`0x1D10 0800 - 0x1D10 0FFF`** - [W] *Debug TX FIFO* (unimplemented)
- **`0x1E00 0000 - 0x1E00 0010`** - [R/W] *Cart Registers*

## Memory spaces

### SDRAM Memory

Base address: **`0x1000 0000`**\
Length: **`64 MB`**\
Access: Read or Write, 2 byte (16 bit) aligned

64 MB of SDRAM memory. Available on the bus when **SDRAM** bit in **CART->CR** register is set. Used as ROM storage.

### Flash Memory

Base address: **`0x1000 0000`** (no remap) or **`0x1800 0000`** (remapped)\
Length: **`16 MB`**\
Access: Read only, 2 byte (16 bit) aligned

16 MB of Flash Memory. Available on the bus when **FLASH** bit in **CART->CR** register is set. Used as bootloader storage.

### EEPROM Memory

Base address: **`0x1D00 0000`**\
Length: **`2 kB`**\
Access: Read or Write, 4 byte (32 bit) aligned

2 kB of EEPROM Memory. Available on the bus when **EEPROM_PI** bit in **CART->CR** register is set. Used to upload/download EEPROM contents to/from PC.

### Debug FIFO DMA (unimplemented)

#### RX FIFO DMA Memory (unimplemented)

Base address: **`0x1D100 0000`**\
Length: **`1kB`**\
Access: Read only, 4 byte (32 bit) aligned

1 kB of RX FIFO. Available on the bus when **DEBUG** bit in **CART->CR** register is set. Used to receive arbitrary data from PC. Only 4 byte reads are supported, for single byte reads use **CART->DEBUG_RX** register.

#### TX FIFO DMA Memory (unimplemented)

Base address: **`0x1D100 0800`**\
Length: **`1kB`**\
Access: Write only, 4 byte (32 bit) aligned

1 kB of TX FIFO. Available on the bus when **DEBUG** bit in **CART->CR** register is set. Used to send arbitrary data to PC. Only 4 byte writes are supported, for single byte writes use **CART->DEBUG_TX** register.

## Registers

### Cart (**CART**) registers

Base address: **`0x1E00 0000`**

#### Configuration register (**CR**)

Address offset: **`0x00`**\
Powerup value: **`0b0000 0001`**\
Soft reset value: **`0b00xx x001`**\
Access: Read or write, 4 byte (32 bit) aligned

This register is used to enable or disable various modules available on the cart.

 31:6   | 5     | 4          | 3         | 2         | 1     | 0
--------|-------|------------|-----------|-----------|-------|-------
 0      | DEBUG | EEPROM_16K | EEPROM_SI | EEPROM_PI | SDRAM | FLASH
 x      | R/W   | R/W        | R/W       | R/W       | R/W   | R/W

- Bits 31:6: Reserved. Reads as 0, writes are ignored but it's recommended to be set as 0 for future compatibility.
- Bit 5 **DEBUG** (unimplemented): Enable debug FIFO access at address base **`0x1D100 0000`** and **`0x1D100 0800`**.
  - 0: Debug FIFO disabled
  - 1: Debug FIFO enabled
- Bit 4 **EEPROM_16K**: Sets ID returned by EEPROM to identify itself as 4k or 16k variant.
  - 0: EEPROM 4k variant
  - 1: EEPROM 16k variant
- Bit 3 **EEPROM_SI**: Enable EEPROM access through SI bus.
  - 0: EEPROM SI access disabled
  - 1: EEPROM SI access enabled
- Bit 2 **EEPROM_PI**: Enable EEPROM access through PI bus at address base **`0x1D00 0000`**. Cleared by hardware on N64 Reset/NMI event.
  - 0: EEPROM PI access disabled
  - 1: EEPROM PI access enabled
- Bit 1 **SDRAM**: Enable SDRAM access at address base **`0x1000 0000`**. This bit also remaps Flash base address. Cleared by hardware on N64 Reset/NMI event.
  - 0: SDRAM disabled
  - 1: SDRAM enabled
- Bit 0 **FLASH**: Enable Flash access at address base **`0x1000 0000`** (**SDRAM** = 0) or **`0x1800 0000`** (**SDRAM** = 1). Set by hardware on N64 Reset/NMI event.
  - 0: Flash disabled
  - 1: Flash enabled

#### CIC and TV type override register (**CIC_TV**)

Address offset: **`0x04`**\
Powerup value: **`0x0000 0000`**\
Access: Read or write, 4 byte (32 bit) aligned

This register is used for PC -> bootloader communication.

 31:8   | 7:6    | 5:4     | 3:0
--------|--------|---------|----------
 0      | SWITCH | TV_TYPE | CIC_TYPE
 x      | R/W    | R/W     | R/W

- Bits 31:8: Reserved. Reads as 0, writes are ignored but it's recommended to be set as 0 for future compatibility.
- Bits 7:6 **SWITCH**: Additional bits that can be passed to bootloader, currently unused.
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

#### Debug single byte RX FIFO access (**DEBUG_RX**) (unimplemented)

Address offset: **`0x08`**\
Access: Read only, 4 byte (32 bit) aligned

This register grabs single byte from debug RX FIFO.

 31:8  | 7:0
-------|------------
 0     | RX_DATA
 x     | R

- Bits 31:8: Reserved. Reads as 0.
- Bits 7:0 **RX_DATA**: Data read from debug RX FIFO.

#### Debug single byte TX FIFO access (**DEBUG_TX**) (unimplemented)

Address offset: **`0x0C`**\
Access: Write only, 4 byte (32 bit) aligned

This register puts single byte on debug TX FIFO.

 31:8  | 7:0
-------|------------
 0     | TX_DATA
 x     | W

- Bits 31:8: Reserved. Writes are ignored but it's recommended to be set as 0 for future compatibility.
- Bits 7:0 **TX_DATA**: Data to be written to debug TX FIFO.

#### Debug status register (**DEBUG_SR**) (unimplemented)

Address offset: **`0x10`**\
Access: Read or write, 4 byte (32 bit) aligned

This register is used for reading status and flushing debug RX/TX FIFOs.

 31:24  | 23       | 22       | 21:11      | 10:0
--------|----------|----------|------------|------------
 0      | TX_FLUSH | RX_FLUSH | TX_FIFO_UB | RX_FIFO_UB
 x      | W        | W        | R          | R

- Bits 31:24: Reserved. Reads as 0, writes are ignored but it's recommended to be set as 0 for future compatibility.
- Bit 23 **TX_FLUSH**: Flushes TX FIFO.
  - 0: No action
  - 1: Flush TX FIFO
- Bit 22 **RX_FLUSH**: Flushes RX FIFO.
  - 0: No action
  - 1: Flush RX FIFO
- Bits 21:11 **TX_FIFO_UB**: Number of bytes waiting to be read by PC.
- Bits 10:0 **RX_FIFO_UB**: Number of bytes waiting to be processed by N64.

### Flash (**FLASH**) registers

Base address: **`0x1C00 0000`**

#### Arbitrary command register (**ACR**)

Address offset: **`0x00`**\
Access: Read or write, 4 byte (32 bit) aligned

This register is used to send arbitrary commands to Flash chip, useful for erasing and programming memory from PC or N64. Extended register documentation is available at [this webpage](https://zipcpu.com/blog/2019/03/27/qflexpress.html).

 31:13   | 12   | 11   | 10 | 9         | 8           | 7:0
---------|------|------|----|-----------|-------------|-------
 0       | MODE | QUAD | 0  | DIRECTION | CHIP_SELECT | DATA
 x       | W    | W    | x  | W         | W           | R/W

- Bits 31:13: Reserved. Reads as 0, writes are ignored but it's recommended to be set as 0 for future compatibility
- Bit 12 **MODE**: Sets flash module arbitrary command mode.
  - 0: Normal mode, no command is send to the Flash chip
  - 1: Arbitrary command mode. When set no reads should be made on the *Flash Memory* address space
- Bit 11 **QUAD**: Sets next Q/SPI transaction transfer speed.
  - 0: SPI 1-bit transaction
  - 1: QSPI 4-bit transaction
- Bit 10: Reserved. Reads as 0, writes are ignored but it's recommended to be set as 0 for future compatibility
- Bit 9 **DIRECTION**: Sets next QSPI transaction direction. This bit has no effect when **QUAD** bit is set to 0
  - 0: QSPI read transaction
  - 1: QSPI write transaction
- Bit 8 **CHIP_SELECT**: Sets next Q/SPI transaction CS pin value and enables/disables next transaction.
  - 0: Deselects Flash chip, no transaction is made
  - 1: Selects Flash chip, transaction is made
- Bits 7:0 **DATA**: Data transferred to/from Flash chip
  - Read: Data returned from last transaction
  - Write: Data to be written in next transaction

## PC communication

PC <-> Cart communication uses SPI as hardware layer. Communication is command based and PC is always responsible for transfer initiation.

Module contains two 1024 word (4 kB) FIFOs used as a gate between SPI clock domain and Cart clock domain. Debug communication uses its own set of FIFOs, 1 kB in size each.

### Commands

Every command starts with setting CS pin low, then sending command, writing and/or reading data and setting CS pin high. Only SPI mode 0 is supported.

#### Status (**0x00**)

Reads PC communication module status word.

Command bytes:

 Byte(s)   | 0    | 1:4
-----------|------|-------------
 Value     | 0x00 | Status word
 Direction | W    | R

Status word bits:

 31:24 | 23        | 22          | 21:11      | 10:0
-------|-----------|-------------|------------|------------
 ID    | ADDR_INCR | N64_DISABLE | TX_FIFO_UW | RX_FIFO_UW

- Bits 31:24 **ID**: Always **`0xAA`**, can be used to identify proper communication with cart.
- Bit 23 **ADDR_INCR**: Returns current status of **ADDR_INCR** bit in configuration.
  - 0: No address increment
  - 1: Address increment
- Bit 22  **N64_DISABLE**: Returns current status of **N64_DISABLE** bit in configuration.
  - 0: N64 PI interface enabled, PC bus access disabled
  - 1: N64 PI interface disabled, PC bus access enabled
- Bits 21:11 **TX_FIFO_UW**: Number of 4 byte (32 bit) words waiting to be processed by bus controller.
- Bits 10:0 **RX_FIFO_UW**: Number of 4 byte (32 bit) words waiting to be read by PC.

Example - read status word:

 Byte | 0    | 1    | 2    | 3    | 4
------|------|------|------|------|------
 TX   | 0x00 | x    | x    | x    | x
 RX   | x    | 0xAA | 0x00 | 0x00 | 0x00

*x = don't care*

#### Config (**0x10**)

Sets PC communication module bus controller configuration.

This command consumes space in TX FIFO. Check TX FIFO availability before issuing this command.

Command bytes:

 Byte(s)   | 0    | 1:4
-----------|------|--------------------
 Value     | 0x10 | Configuration word
 Direction | W    | W

Configuration word bits:

 31:2 | 1         | 0
------|-----------|-------------
 x    | ADDR_INCR | N64_DISABLE

- Bits 31:2: Reserved. Writes are ignored but it's recommended to be set as 0 for future compatibility.
- Bit 1 **ADDR_INCR**: Sets address increment mode when reading or writing to bus. Useful for writing/reading many values to/from single address.
  - 0: No address increment
  - 1: Address increment
- Bit 0  **N64_DISABLE**: Disables N64 PI interface and enables communication module access to the bus.
  - 0: N64 PI interface enabled, PC bus access disabled
  - 1: N64 PI interface disabled, PC bus access enabled

Example - set address increment and disable N64:

 Byte | 0    | 1    | 2    | 3    | 4
------|------|------|------|------|------
 TX   | 0x10 | 0x00 | 0x00 | 0x00 | 0x03
 RX   | x    | x    | x    | x    | x

*x = don't care*

#### Address (**0x20**)

Sets starting bus address.

This command consumes space in TX FIFO. Check TX FIFO availability before issuing this command.

Command bytes:

 Byte(s)   | 0    | 1:4
-----------|------|--------------
 Value     | 0x20 | Address word
 Direction | W    | W

Address word bits:

 31:0    |
---------|
 ADDRESS |

- Bits 31:0 **ADDRESS**: Starting address in bus address space.

Example - set starting address **`0x1034 5678`**:

 Byte | 0    | 1    | 2    | 3    | 4
------|------|------|------|------|------
 TX   | 0x20 | 0x10 | 0x34 | 0x56 | 0x78
 RX   | x    | x    | x    | x    | x

*x = don't care*

#### Initiate read to RX FIFO (**0x30**)

Initiates read of X 4 byte (32 bit) words to RX FIFO. Maximum possible read length is 64 MB. However, it's possible to send multiple commands in series to achieve longer read lengths. After issuing this command it's necesarry to read data with "Read from RX FIFO (0x50)" command. Command will read dummy data when **N64_DISABLE** bit is cleared in communication module configuration.

This command consumes space in TX FIFO. Check TX FIFO availability before issuing this command.

Command bytes:

 Byte(s)   | 0    | 1:4
-----------|------|------------------
 Value     | 0x30 | Read length word
 Direction | W    | W

Read length word bits:

 31:24 | 23:0        |
-------|-------------|
 x     | READ_LENGTH |

- Bits 31:24: Reserved. Writes are ignored but it's recommended to be set as 0 for future compatibility.
- Bits 23:0 **READ_LENGTH**: Number of words to be read to RX FIFO minus one.

Example - fill RX FIFO with 5 words (20 bytes):

 Byte | 0    | 1    | 2    | 3    | 4
------|------|------|------|------|------
 TX   | 0x30 | 0x00 | 0x00 | 0x00 | 0x04
 RX   | x    | x    | x    | x    | x

*x = don't care*

#### Write to TX FIFO (**0x40**)

Writes data to TX FIFO. Sent words then are processed immediately as bus writes at current internal bus address. Data can be written only as 4 byte (32 bit) words. When **N64_DISABLE** bit in communication module configuration is cleared then command will process data in TX FIFO but it won't write anything to bus.

This command consumes space in TX FIFO. Check TX FIFO availability before issuing this command.

Command bytes:

 Byte(s)   | 0    | 1:4       | 5:8       | 9:12      | ...
-----------|------|-----------|-----------|-----------|-----
 Value     | 0x40 | Data word | Data word | Data word | ...
 Direction | W    | W         | W         | W         | ...

Data word bits:

 31:0 |
------|
 DATA |

- Bits 31:0 **DATA**: Word to be written to bus.

Example - fill TX FIFO with 2 words (8 bytes) **`0xDEAD BEEF`**, **`0x0102 0304`**:

 Byte | 0    | 1    | 2    | 3    | 4    | 5    | 6    | 7    | 8
------|------|------|------|------|------|------|------|------|------
 TX   | 0x40 | 0xDE | 0xAD | 0xBE | 0xEF | 0x01 | 0x02 | 0x03 | 0x04
 RX   | x    | x    | x    | x    | x    | x    | x    | x    | x

*x = don't care*

#### Read from RX FIFO (**0x50**)

Reads data from RX FIFO. Data can be read only as 4 byte (32 bit) words. Before reading it's necessary to check RX FIFO availability. Reading empty FIFO won't break anything but it's pointless.

Command bytes:

 Byte(s)   | 0    | 1:4       | 5:8       | 9:12      | ...
-----------|------|-----------|-----------|-----------|-----
 Value     | 0x50 | Data word | Data word | Data word | ...
 Direction | W    | R         | R         | R         | ...

Data word bits:

 31:0 |
------|
 DATA |

- Bits 31:0 **DATA**: Word read from the bus.

Example - read 2 words (8 bytes) **`0xDEAD BEEF`**, **`0x0102 0304`** from RX FIFO:

 Byte | 0    | 1    | 2    | 3    | 4    | 5    | 6    | 7    | 8
------|------|------|------|------|------|------|------|------|------
 TX   | 0x50 | x    | x    | x    | x    | x    | x    | x    | x
 RX   | x    | 0xDE | 0xAD | 0xBE | 0xEF | 0x01 | 0x02 | 0x03 | 0x04

*x = don't care*

#### Read debug FIFO status (**0x60**) (unimplemented)

Reads debug FIFO status.

Command bytes:

 Byte(s)   | 0    | 1:4
-----------|------|------------------------
 Value     | 0x00 | Debug FIFO status word
 Direction | W    | R

Debug FIFO status word bits:

 31:22 | 21:11      | 10:0
-------|------------|------------
 x     | TX_FIFO_UB | RX_FIFO_UB

- Bits 31:22: Reserved. Reads as 0.
- Bits 21:11 **TX_FIFO_UB**: Number of bytes waiting to be processed by N64.
- Bits 10:0 **RX_FIFO_UB**: Number of bytes waiting to be read by PC.

Example - read debug FIFO status word:

 Byte | 0    | 1    | 2    | 3    | 4
------|------|------|------|------|------
 TX   | 0x60 | x    | x    | x    | x
 RX   | x    | 0x00 | 0x00 | 0x00 | 0x00

*x = don't care*

#### Write to debug TX FIFO (**0x70**) (unimplemented)

Writes bytes to debug TX FIFO.

This command consumes space in debug TX FIFO. Check debug TX FIFO availability before issuing this command.

Command bytes:

 Byte(s)   | 0    | 1         | 2         | 3         | ...
-----------|------|-----------|-----------|-----------|-----
 Value     | 0x70 | Data byte | Data byte | Data byte | ...
 Direction | W    | W         | W         | W         | ...

Data byte bits:

 7:0  |
------|
 DATA |

- Bits 7:0 **DATA**: Byte to be written to debug TX FIFO.

Example - fill debug TX FIFO with 4 bytes **`0xDE`**, **`0xAD`**, **`0xBE`**, **`0xEF`**:

 Byte | 0    | 1    | 2    | 3    | 4
------|------|------|------|------|------
 TX   | 0x70 | 0xDE | 0xAD | 0xBE | 0xEF
 RX   | x    | x    | x    | x    | x

*x = don't care*

#### Read from RX FIFO (**0x80**) (unimplemented)

Reads bytes from debug RX FIFO. Before reading it's necessary to check debug RX FIFO availability. Reading empty FIFO won't break anything but it's pointless.
Command bytes:

 Byte(s)   | 0    | 1         | 2         | 3         | ...
-----------|------|-----------|-----------|-----------|-----
 Value     | 0x70 | Data byte | Data byte | Data byte | ...
 Direction | W    | R         | R         | R         | ...

Data byte bits:

 7:0  |
------|
 DATA |

- Bits 7:0 **DATA**: Byte read from debug RX FIFO.

Example - read 4 bytes from debug RX FIFO:

 Byte | 0    | 1    | 2    | 3    | 4
------|------|------|------|------|------
 TX   | 0x80 | x    | x    | x    | x
 RX   | x    | 0xDE | 0xAD | 0xBE | 0xEF

*x = don't care*

#### PC communication module bus controller reset (**0xFC**)

Resets PC communication module bus controller. It's recommended to issue "Reset (**0xFF**)" command before sending this one.

This command consumes space in TX FIFO. Check TX FIFO availability before issuing this command.

Command bytes:

 Byte(s)   | 0    | 1:4
-----------|------|------------
 Value     | 0xFC | Reset word
 Direction | W    | W

Reset word bits:

 31:0 |
------|
 x    |

- Bits 31:0: Reserved. Writes are ignored but it's recommended to be set as 0 for future compatibility.

Example - reset PC communication module:

 Byte | 0    | 1    | 2    | 3    | 4
------|------|------|------|------|------
 TX   | 0xFC | 0x00 | 0x00 | 0x00 | 0x00
 RX   | x    | x    | x    | x    | x

*x = don't care*

#### Flush TX FIFO (**0xFD**)

Flushes TX FIFO.

Command bytes:

 Byte(s)   | 0
-----------|------
 Value     | 0xFD
 Direction | W

Example - flush TX FIFO:

 Byte | 0
------|------
 TX   | 0xFD
 RX   | x

*x = don't care*

#### Flush RX FIFO (**0xFE**)

Flushes RX FIFO.

Command bytes:

 Byte(s)   | 0
-----------|------
 Value     | 0xFE
 Direction | W

Example - flush RX FIFO:

 Byte | 0
------|------
 TX   | 0xFE
 RX   | x

*x = don't care*

#### Reset (**0xFF**)

Resets PC communication module SPI controller and flushes both TX and RX data/debug FIFOs.

Command bytes:

 Byte(s)   | 0
-----------|------
 Value     | 0xFF
 Direction | W

Example - send reset:

 Byte | 0
------|------
 TX   | 0xFF
 RX   | x

*x = don't care*
