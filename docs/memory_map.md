## Internal memory map

This mapping is used internally by FPGA/μC and when accessing flashcart from USB side.

| name            | base          | size      | access |
| --------------- | ------------- | --------- | ------ |
| SDRAM           | `0x0000_0000` | 64 MiB    | RW     |
| Flash           | `0x0400_0000` | 16 MiB    | RW     |
| Data buffer     | `0x0500_0000` | 8 kiB     | RW     |
| EEPROM          | `0x0500_2000` | 2 kiB     | RW     |
| 64DD buffer     | `0x0500_2800` | 256 bytes | RW     |
| FlashRAM buffer | `0x0500_2900` | 128 bytes | R      |

---

## PI memory map

This mapping is used when accessing flashcart from N64 side.

| name                | base          | size      | access | mapped base   | device    | availability when                 |
| ------------------- | ------------- | --------- | ------ | ------------- | --------- | --------------------------------- |
| 64DD registers      | `0x0500_0000` | 2 kiB     | RW     | N/A           | N/A       | DD mode is set to REGS or FULL    |
| 64DD IPL [1]        | `0x0600_0000` | 4 MiB     | R      | `0x03BC_0000` | SDRAM     | DD mode is set to IPL or FULL     |
| SRAM [2]            | `0x0800_0000` | 128 kiB   | RW     | `0x03FE_0000` | SDRAM     | SRAM save type is selected        |
| SRAM banked [2][3]  | `0x0800_0000` | 96 kiB    | RW     | `0x03FE_0000` | SDRAM     | SRAM banked save type is selected |
| FlashRAM [2]        | `0x0800_0000` | 128 kiB   | RW     | `0x03FE_0000` | SDRAM     | FlashRAM save type is selected    |
| Bootloader          | `0x1000_0000` | 1920 kiB  | R      | `0x04E0_0000` | Flash     | Bootloader switch is enabled      |
| ROM [4]             | `0x1000_0000` | 64 MiB    | RW     | `0x0000_0000` | SDRAM     | Bootloader switch is disabled     |
| ROM shadow [5]      | `0x13FE_0000` | 128 kiB   | R      | `0x04FE_0000` | Flash     | ROM shadow is enabled             |
| ROM extended        | `0x1400_0000` | 14 MiB    | R      | `0x0400_0000` | Flash     | ROM extended is enabled           |
| ROM shadow [6]      | `0x1FFC_0000` | 128 kiB   | R      | `0x04FE_0000` | Flash     | SC64 register access is enabled   |
| Data buffer         | `0x1FFE_0000` | 8 kiB     | RW     | `0x0500_0000` | Block RAM | SC64 register access is enabled   |
| EEPROM              | `0x1FFE_2000` | 2 kiB     | RW     | `0x0500_2000` | Block RAM | SC64 register access is enabled   |
| 64DD buffer [7]     | `0x1FFE_2800` | 256 bytes | RW     | `0x0500_2800` | Block RAM | SC64 register access is enabled   |
| FlashRAM buffer [7] | `0x1FFE_2900` | 128 bytes | R      | `0x0500_2900` | Block RAM | SC64 register access is enabled   |
| SC64 registers      | `0x1FFF_0000` | 20 bytes  | RW     | N/A           | N/A       | SC64 register access is enabled   |

 - Note [1]: 64DD IPL share SDRAM memory space with ROM (last 4 MiB minus 128 kiB for saves)
 - Note [2]: SRAM and FlashRAM save types share SDRAM memory space with ROM (last 128 kiB)
 - Note [3]: 32 kiB chunks are accesed at `0x0800_0000`, `0x0804_0000` and `0x0808_0000`
 - Note [4]: Write access is available when `ROM_WRITE_ENABLE` config is enabled
 - Note [5]: This address overlaps last 128 kiB of ROM space allowing SRAM and FlashRAM save types to work with games occupying almost all of ROM space (for example Pokemon Stadium 2). Reads are redirected to last 128 kiB of flash.
 - Note [6]: Used internally for performing flash writes from SD card
 - Note [7]: Used internally and exposed only for debugging

---

## SC64 registers

| name               | address       | size    | access | usage                            |
| ------------------ | ------------- | ------- | ------ | -------------------------------- |
| **STATUS/COMMAND** | `0x1FFF_0000` | 4 bytes | RW     | Command execute and status       |
| **DATA0**          | `0x1FFF_0004` | 4 bytes | RW     | Command argument/result 0        |
| **DATA1**          | `0x1FFF_0008` | 4 bytes | RW     | Command argument/result 1        |
| **VERSION**        | `0x1FFF_000C` | 4 bytes | RW     | Hardware version and IRQ clear   |
| **KEY**            | `0x1FFF_0010` | 4 bytes | W      | SC64 register access lock/unlock |

---

#### **STATUS/COMMAND**

| name          | bits   | access | meaning                                               |
| ------------- | ------ | ------ | ----------------------------------------------------- |
| `CMD_BUSY`    | [31]   | R      | `1` if dispatched command is pending/executing        |
| `CMD_ERROR`   | [30]   | R      | `1` if last executed command returned with error code |
| `IRQ_PENDING` | [29]   | R      | `1` if flashcart has raised an interrupt              |
| N/A           | [28:8] | N/A    | Unused, write `0` for future compatibility            |
| `CMD_ID`      | [7:0]  | RW     | Command ID to be executed                             |

Note: Write to this register raises `CMD_BUSY` bit and clears `CMD_ERROR` bit. Flashcart then will start executing provided command.

---

#### **DATA0** and **DATA1**

| name      | bits   | access | meaning                            |
| --------- | ------ | ------ | ---------------------------------- |
| `DATA0/1` | [31:0] | RW     | Command argument (W) or result (R) |

Note: Result is valid only when command has executed and `CMD_BUSY` bit is reset. When `CMD_ERROR` is set then **DATA0** register contains error code.

---

#### **VERSION**

| name      | bits   | access | meaning                                       |
| --------- | ------ | ------ | --------------------------------------------- |
| `VERSION` | [31:0] | RW     | Hardware version (ASCII `SCv2`) and IRQ clear |

Note: Writing any value to this register will clear pending flashcart interrupt.

---

#### **KEY**

| name  | bits   | access | meaning                                    |
| ----- | ------ | ------ | ------------------------------------------ |
| `KEY` | [31:0] | W      | Value to be put into lock/unlock sequencer |

Note: By default from cold boot (power on) or console reset (NMI) flashcart will disable access to SC64 specific memory regions.
**KEY** register is always enabled and listening for writes regardless of lock/unlock state.
To enable SC64 registers it is necesarry to provide sequence of values to this register.
Value `0x00000000` will reset sequencer state.
Two consequentive writes of values `0x5F554E4C` and `0x4F434B5F` will unlock all SC64 registers if flashcart is in lock state.
Value `0xFFFFFFFF` will lock all SC64 registers if flashcart is in unlock state.

