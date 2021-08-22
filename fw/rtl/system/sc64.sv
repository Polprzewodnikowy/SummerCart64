package sc64;

    typedef enum bit [2:0] {
        ID_N64_SDRAM,
        ID_N64_BOOTLOADER,
        ID_N64_FLASHRAM,
        ID_N64_DDREGS,
        ID_N64_CPU,
        __ID_N64_END
    } e_n64_id;

    typedef enum bit [3:0] {
        ID_CPU_RAM,
        ID_CPU_BOOTLOADER,
        ID_CPU_GPIO,
        ID_CPU_I2C,
        ID_CPU_USB,
        ID_CPU_UART,
        ID_CPU_DMA,
        __ID_CPU_END
    } e_cpu_id;

    typedef enum bit [1:0] {
        ID_DMA_USB,
        ID_DMA_SD,
        __ID_DMA_END
    } e_dma_id;

    parameter int CLOCK_FREQUENCY   = 32'd100_000_000;

    parameter int UART_BAUD_RATE    = 32'd1_000_000;

    parameter bit DEBUG_ENABLED     = 1'b1;

endpackage
