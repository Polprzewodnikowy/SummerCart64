## Internal memory map

| name            | base          | size      | access |
| --------------- | ------------- | --------- | ------ |
| SDRAM           | `0x0000_0000` | 64 MiB    | RW     |
| Flash           | `0x0400_0000` | 16 MiB    | RW     |
| Data buffer     | `0x0500_0000` | 8 kiB     | RW     |
| EEPROM          | `0x0500_2000` | 2 kiB     | RW     |
| 64DD buffer     | `0x0500_2800` | 256 bytes | RW     |
| FlashRAM buffer | `0x0500_2900` | 128 bytes | R      |


## N64 Memory map

| name                | base          | size      | access | mapped base   | device    | availability when                 |
| ------------------- | ------------- | --------- | ------ | ------------- | --------- | --------------------------------- |
| 64DD registers      | `0x0500_0000` | 2 kiB     | RW     | N/A           | N/A       | DD mode is set to REGS or FULL    |
| 64DD IPL [1]        | `0x0600_0000` | 4 MiB     | R      | `0x03BC_0000` | SDRAM     | DD mode is set to IPL or FULL     |
| SRAM [2]            | `0x0800_0000` | 128 kiB   | RW     | `0x03FE_0000` | SDRAM     | SRAM save type is selected        |
| SRAM banked [2][3]  | `0x0800_0000` | 96 kiB    | RW     | `0x03FE_0000` | SDRAM     | SRAM banked save type is selected |
| FlashRAM [2]        | `0x0800_0000` | 128 kiB   | RW     | `0x03FE_0000` | SDRAM     | FlashRAM save type is selected    |
| Bootloader          | `0x1000_0000` | 1920 kiB  | R      | `0x04E0_0000` | Flash     | Bootloader switch is enabled      |
| ROM                 | `0x1000_0000` | 64 MiB    | RW     | `0x0000_0000` | SDRAM     | Bootloader switch is disabled     |
| ROM shadow [4]      | `0x13FE_0000` | 128 kiB   | R      | `0x04FE_0000` | Flash     | ROM shadow is enabled             |
| ROM extended        | `0x1400_0000` | 14 MiB    | R      | `0x0400_0000` | Flash     | ROM extended is enabled           |
| ROM shadow [5]      | `0x1FFC_0000` | 128 kiB   | R      | `0x04FE_0000` | Flash     | SC64 register access is enabled   |
| Data buffer         | `0x1FFE_0000` | 8 kiB     | RW     | `0x0500_0000` | Block RAM | SC64 register access is enabled   |
| EEPROM [6]          | `0x1FFE_2000` | 2 kiB     | RW     | `0x0500_2000` | Block RAM | SC64 register access is enabled   |
| 64DD buffer [6]     | `0x1FFE_2800` | 256 bytes | RW     | `0x0500_2800` | Block RAM | SC64 register access is enabled   |
| FlashRAM buffer [6] | `0x1FFE_2900` | 128 bytes | R      | `0x0500_2900` | Block RAM | SC64 register access is enabled   |
| SC64 registers      | `0x1FFF_0000` | 20 bytes  | RW     | N/A           | N/A       | SC64 register access is enabled   |

 - Note [1]: 64DD IPL share SDRAM memory space with ROM (last 4 MiB minus 128 kiB for saves)
 - Note [2]: SRAM and FlashRAM save types share SDRAM memory space with ROM (last 128 kiB)
 - Note [3]: 32 kiB chunks are accesed at `0x0800_0000`, `0x0804_0000` and `0x0808_0000`
 - Note [4]: This address overlaps last 128 kiB of ROM space allowing SRAM and FlashRAM save types to work with games occupying almost all of ROM space (for example Pokemon Stadium 2). Reads are redirected to last 128 kiB of flash.
 - Note [5]: Used internally for performing flash writes from SD card
 - Note [6]: Used internally and exposed only for debugging