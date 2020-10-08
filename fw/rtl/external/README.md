# External code for SummerCart64 Firmware

This folder contains code downloaded directly from external repositories with minor modifications.

## qflexpress.v

### Source

[https://github.com/ZipCPU/qspiflash/blob/master/rtl/qflexpress.v](https://github.com/ZipCPU/qspiflash/blob/master/rtl/qflexpress.v)

### Modifications

- Changed initialization sequence.
- Extended address space for 16-bit address alignment.

## wbsdram.v

### Source

[https://github.com/ZipCPU/arrowzip/blob/master/rtl/arrowzip/wbsdram.v](https://github.com/ZipCPU/arrowzip/blob/master/rtl/arrowzip/wbsdram.v)

### Modifications

- Changed refresh interval that suits 90 MHz clock and used SDRAM.
- Changed column and row address widths.
