- [Internal memory map](#internal-memory-map)
- [PI memory map](#pi-memory-map)
  - [Address decoding limitations](#address-decoding-limitations)
  - [Flash mapped sections](#flash-mapped-sections)
- [SC64 registers](#sc64-registers)
    - [`0x1FFF_0000`: **STATUS/COMMAND**](#0x1fff_0000-statuscommand)
    - [`0x1FFF_0004`: **DATA0** and `0x1FFF_0008`: **DATA1**](#0x1fff_0004-data0-and-0x1fff_0008-data1)
    - [`0x1FFF_000C`: **IDENTIFIER**](#0x1fff_000c-identifier)
    - [`0x1FFF_0010`: **KEY**](#0x1fff_0010-key)
    - [`0x1FFF_0014`: **IRQ**](#0x1fff_0014-irq)
    - [`0x1FFF_0018`: **AUX**](#0x1fff_0018-aux)
- [Command execution flow](#command-execution-flow)
  - [Without interrupt](#without-interrupt)
  - [With interrupt](#with-interrupt)

---

## Internal memory map

This mapping is used internally by FPGA/μC and when accessing flashcart from USB side.

| section             | base          | size             | access | device   |
| ------------------- | ------------- | ---------------- | ------ | -------- |
| SDRAM               | `0x0000_0000` | 64 MiB           | RW     | SDRAM    |
| Flash [1]           | `0x0400_0000` | 16 MiB           | RW/R   | Flash    |
| Data buffer         | `0x0500_0000` | 8 kiB            | RW     | BlockRAM |
| EEPROM              | `0x0500_2000` | 2 kiB            | RW     | BlockRAM |
| 64DD buffer         | `0x0500_2800` | 256 bytes        | RW     | BlockRAM |
| FlashRAM buffer [2] | `0x0500_2900` | 128 bytes        | R      | BlockRAM |
| N/A [3]             | `0x0500_2980` | to `0x07FF_FFFF` | R      | N/A      |

 - Note [1]: Flash memory region `0x04E0_0000` - `0x04FD_FFFF` is write protected as it contains N64 bootloader. This section can be overwritten only via firmware update process.
 - Note [2]: Due to BlockRAM usage optimization this section is read only.
 - Note [3]: Read returns `0`. Maximum accessible address space is 128 MiB.

---

## PI memory map

This mapping is used when accessing flashcart from N64 side.

| section             | base          | size      | access | mapped base   | mapped device         | mapped bus  | mapped when                       |
| ------------------- | ------------- | --------- | ------ | ------------- | --------------------- | ----------- | --------------------------------- |
| 64DD registers      | `0x0500_0000` | 2 kiB     | RW     | N/A           | 64DD Controller       | reg bus     | DD mode is set to REGS or FULL    |
| 64DD IPL [1]        | `0x0600_0000` | 4 MiB     | R      | `0x03BC_0000` | SDRAM                 | mem bus     | DD mode is set to IPL or FULL     |
| SRAM [2]            | `0x0800_0000` | 128 kiB   | RW     | `0x03FE_0000` | SDRAM                 | mem bus     | SRAM save type is selected        |
| SRAM banked [2][3]  | `0x0800_0000` | 96 kiB    | RW     | `0x03FE_0000` | SDRAM                 | mem bus     | SRAM banked save type is selected |
| FlashRAM [2][4]     | `0x0800_0000` | 128 kiB   | RW     | `0x03FE_0000` | FlashRAM Cntrl./SDRAM | reg/mem bus | FlashRAM save type is selected    |
| Bootloader          | `0x1000_0000` | 1920 kiB  | R      | `0x04E0_0000` | Flash                 | mem bus     | Bootloader switch is enabled      |
| ROM [5]             | `0x1000_0000` | 64 MiB    | RW     | `0x0000_0000` | SDRAM                 | mem bus     | Bootloader switch is disabled     |
| ROM shadow [6]      | `0x13FE_0000` | 128 kiB   | R      | `0x04FE_0000` | Flash                 | mem bus     | ROM shadow is enabled             |
| ROM extended        | `0x1400_0000` | 14 MiB    | R      | `0x0400_0000` | Flash                 | mem bus     | ROM extended is enabled           |
| ROM shadow [7]      | `0x1FFC_0000` | 128 kiB   | R      | `0x04FE_0000` | Flash                 | mem bus     | SC64 register access is enabled   |
| Data buffer         | `0x1FFE_0000` | 8 kiB     | RW     | `0x0500_0000` | Block RAM             | mem bus     | SC64 register access is enabled   |
| EEPROM              | `0x1FFE_2000` | 2 kiB     | RW     | `0x0500_2000` | Block RAM             | mem bus     | SC64 register access is enabled   |
| 64DD buffer [8]     | `0x1FFE_2800` | 256 bytes | RW     | `0x0500_2800` | Block RAM             | mem bus     | SC64 register access is enabled   |
| FlashRAM buffer [8] | `0x1FFE_2900` | 128 bytes | R      | `0x0500_2900` | Block RAM             | mem bus     | SC64 register access is enabled   |
| SC64 registers      | `0x1FFF_0000` | 28 bytes  | RW     | N/A           | Flashcart Interface   | reg bus     | SC64 register access is enabled   |

 - Note [1]: 64DD IPL share SDRAM memory space with ROM (last 4 MiB minus 128 kiB for saves). Write access is always disabled for this section.
 - Note [2]: SRAM and FlashRAM save types share SDRAM memory space with ROM (last 128 kiB).
 - Note [3]: 32 kiB chunks are accessed at `0x0800_0000`, `0x0804_0000` and `0x0808_0000`.
 - Note [4]: FlashRAM read access is multiplexed between mem and reg bus, writes are always mapped to reg bus.
 - Note [5]: Write access is available when `ROM_WRITE_ENABLE` config is enabled.
 - Note [6]: This address overlaps last 128 kiB of ROM space allowing SRAM and FlashRAM save types to work with games occupying almost all of ROM space (for example Pokemon Stadium 2). Reads are redirected to last 128 kiB of flash.
 - Note [7]: Always accessible regardless of ROM shadow switch.
 - Note [8]: Used internally and exposed only for debugging.

### Address decoding limitations

Current implementation of PI interface checks only upper 16 bits of address. Bus and device are chosen only from value of starting address.
In specific situations this could lead to unexpected behavior when performing R/W operations crossing 64 kiB boundaries.
Page size (as called by N64 docs) is configurable by `PI_BSD_DOMn_PGS` register. Maximum page size can be set up to 128 kiB blocks.
PI controller inside N64 will automatically reissue address at set boundary when performing R/W operation that crosses it.

For example, setting largest page size then doing 128 kiB read starting from address `0x1FFE_0000` will select *mem bus* and start fetching data from mapped internal address `0x0500_0000`.
SC64 registers are available at base address `0x1FFF_0000` (`0x1FFE_0000` + 64 kiB), but are connected to *reg bus*.
As a consequence of this design data read by N64 in single transaction will not contain values of SC64 registers at 64 kiB offset.

### Flash mapped sections

Due to flash memory timing requirements it's not possible to directly write data from N64 side.
Special commands are provided for performing flash erase and program.
During those operations avoid accessing flash mapped sections.
Data read will be corrupted and erase/program operations slows down.

---

## SC64 registers

SC64 contains small register region used for communication between N64 and controller code running on the μC.
Protocol is command based with support for up to 256 different commands and two 32-bit argument/result values per operation.
Support for interrupts is provided but currently no command relies on it, 64DD IRQ is handled separately.

| name               | address       | size    | access | usage                            |
| ------------------ | ------------- | ------- | ------ | -------------------------------- |
| **STATUS/COMMAND** | `0x1FFF_0000` | 4 bytes | RW     | Command execution and status     |
| **DATA0**          | `0x1FFF_0004` | 4 bytes | RW     | Command argument/result 0        |
| **DATA1**          | `0x1FFF_0008` | 4 bytes | RW     | Command argument/result 1        |
| **IDENTIFIER**     | `0x1FFF_000C` | 4 bytes | RW     | Flashcart identifier             |
| **KEY**            | `0x1FFF_0010` | 4 bytes | W      | SC64 register access lock/unlock |
| **IRQ**            | `0x1FFF_0014` | 4 bytes | RW     | IRQ clear and enable             |
| **AUX**            | `0x1FFF_0018` | 4 bytes | RW     | Auxiliary interrupt data channel |

---

#### `0x1FFF_0000`: **STATUS/COMMAND**

| name              | bits   | access | meaning                                                     |
| ----------------- | ------ | ------ | ----------------------------------------------------------- |
| `CMD_BUSY`        | [31]   | R      | `1` if dispatched command is pending/executing              |
| `CMD_ERROR`       | [30]   | R      | `1` if last executed command returned with error code       |
| `BTN_IRQ_PENDING` | [29]   | R      | `1` if flashcart has raised a "button pressed" interrupt    |
| `CMD_IRQ_PENDING` | [28]   | R      | `1` if flashcart has raised a "command finish" interrupt    |
| `USB_IRQ_PENDING` | [27]   | R      | `1` if flashcart has raised au "USB not empty" interrupt    |
| `AUX_IRQ_PENDING` | [26]   | R      | `1` if flashcart has raised an "AUX not empty" interrupt    |
| N/A               | [25:9] | N/A    | Unused, write `0` for future compatibility                  |
| `CMD_IRQ_REQUEST` | [8]    | RW     | Raise cart interrupt signal when command finishes execution |
| `CMD_ID`          | [7:0]  | RW     | Command ID to be executed                                   |

Note: Write to this register raises `CMD_BUSY` bit and clears `CMD_ERROR` bit. Flashcart then will start executing provided command.

---

####  `0x1FFF_0004`: **DATA0** and `0x1FFF_0008`: **DATA1**

| name      | bits   | access | meaning                            |
| --------- | ------ | ------ | ---------------------------------- |
| `DATA0/1` | [31:0] | RW     | Command argument (W) or result (R) |

Note: Result is valid only when command has executed and `CMD_BUSY` bit is reset. When `CMD_ERROR` is set then **DATA0** register contains error code.

---

#### `0x1FFF_000C`: **IDENTIFIER**

| name         | bits   | access | meaning                             |
| ------------ | ------ | ------ | ----------------------------------- |
| `IDENTIFIER` | [31:0] | R      | Flashcart identifier (ASCII `SCv2`) |


---

#### `0x1FFF_0010`: **KEY**

| name  | bits   | access | meaning                                    |
| ----- | ------ | ------ | ------------------------------------------ |
| `KEY` | [31:0] | W      | Value to be put into lock/unlock sequencer |

Note: By default from cold boot (power on) or console reset (NMI) flashcart will disable access to SC64 specific memory regions.
**KEY** register is always enabled and listening for writes regardless of lock/unlock state.
To enable SC64 registers it is necessary to provide sequence of values to this register.
Value `0x00000000` will reset sequencer state.
Two consecutive writes of values `0x5F554E4C` and `0x4F434B5F` will unlock all SC64 registers if flashcart is in lock state.
Value `0xFFFFFFFF` will lock all SC64 registers if flashcart is in unlock state.

---

#### `0x1FFF_0014`: **IRQ**

| name              | bits    | access | meaning                                                           |
| ----------------- | ------- | ------ | ----------------------------------------------------------------- |
| `BTN_CLEAR`       | [31]    | W      | Write `1` to clear "button pressed" interrupt                     |
| `CMD_CLEAR`       | [30]    | W      | Write `1` to clear "command finish" interrupt                     |
| `USB_CLEAR`       | [29]    | W      | Write `1` to clear "USB not empty" interrupt                      |
| `AUX_CLEAR`       | [28]    | W      | Write `1` to clear "AUX not empty" interrupt                      |
| N/A               | [27:24] | N/A    | Unused, write `0` for future compatibility                        |
| `MCU_IRQ_MASK`    | [23]    | R      | `1` means "button pressed" interrupt is enabled (it's always `1`) |
| `CMD_IRQ_MASK`    | [22]    | R      | `1` means "command finish" interrupt is enabled (it's always `1`) |
| `USB_IRQ_MASK`    | [21]    | R      | `1` means "USB not empty" interrupt is enabled                    |
| `AUX_IRQ_MASK`    | [20]    | R      | `1` means "AUX not empty" interrupt is enabled                    |
| N/A               | [19:16] | N/A    | Unused, write `0` for future compatibility                        |
| `USB_IRQ_DISABLE` | [11]    | W      | Write `1` to disable "USB not empty" interrupt                    |
| `USB_IRQ_ENABLE`  | [10]    | W      | Write `1` to enable "USB not empty" interrupt                     |
| `AUX_IRQ_DISABLE` | [9]     | W      | Write `1` to disable "AUX not empty" interrupt                    |
| `AUX_IRQ_ENABLE`  | [8]     | W      | Write `1` to enable "AUX not empty" interrupt                     |
| N/A               | [7:0]   | N/A    | Unused, write `0` for future compatibility                        |

Note: All interrupts are cleared and disabled when any of the following events occur:
 - Hard reset
 - NMI reset
 - SC64 registers locking

SC64 interrupts are completely disabled when register access is not enabled.

---

#### `0x1FFF_0018`: **AUX**

This register can be used as a very simple interface to the PC via USB.
Writing to this register generates an USB transfer with the contents of the written data.
New data available in the register is signaled via cart interrupt that needs to be enabled beforehand in the `IRQ` register.
There is no flow control, use this register as a ping-pong interface.
For example, PC sends AUX data, N64 receives interrupt, reads the data then writes to this register with a response.
This flow can be reversed if needed - N64 can be the initiating side.

| name   | bits   | access | meaning        |
| ------ | ------ | ------ | -------------- |
| `DATA` | [31:0] | RW     | Arbitrary data |

---

## Command execution flow

### Without interrupt

1. Check if command is already executing by reading `CMD_BUSY` bit in **STATUS/COMMAND** register (optional).
2. Write command argument values to **DATA0** and **DATA1** registers, can be skipped if command doesn't require it.
3. Write command ID to **STATUS/COMMAND** register.
4. Wait for `CMD_BUSY` bit in **STATUS/COMMAND** register to go low.
5. Check if `CMD_ERROR` bit in **STATUS/COMMAND** is set:
   - If error is set then read **DATA0** register containing error code.
   - If error is not set then read **DATA0** and **DATA1** registers containing command result values, can be skipped if command doesn't return any values.

### With interrupt

1. Check if command is already executing by reading `CMD_BUSY` bit in **STATUS/COMMAND** register (optional).
2. Write command argument values to **DATA0** and **DATA1** registers, can be skipped if command doesn't require it.
3. Write command ID to **STATUS/COMMAND** register and set `CMD_IRQ_REQUEST` bit high.
4. Wait for cart interrupt.
5. Check (in cart interrupt handler) if `CMD_IRQ_PENDING` bit in **STATUS/COMMAND** register is set high.
6. Clear interrupt by setting `CMD_CLEAR` bit high in the **IRQ** register.
7. Check if `CMD_ERROR` bit in **STATUS/COMMAND** is set:
   - If error is set then read **DATA0** register containing error code.
   - If error is not set then read **DATA0** and **DATA1** registers containing command result values, can be skipped if command doesn't return any values.
