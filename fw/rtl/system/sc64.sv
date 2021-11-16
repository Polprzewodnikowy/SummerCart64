package sc64;

    typedef enum bit [2:0] {
        ID_N64_SDRAM,
        ID_N64_BOOTLOADER,
        ID_N64_FLASHRAM,
        ID_N64_DD,
        ID_N64_CFG,
        __ID_N64_END
    } e_n64_id;

    typedef enum bit [3:0] {
        ID_CPU_RAM,
        ID_CPU_FLASH,
        ID_CPU_GPIO,
        ID_CPU_I2C,
        ID_CPU_USB,
        ID_CPU_UART,
        ID_CPU_DMA,
        ID_CPU_CFG,
        ID_CPU_SDRAM,
        ID_CPU_FLASHRAM,
        ID_CPU_SI,
        __ID_CPU_END
    } e_cpu_id;

    typedef enum bit [1:0] {
        ID_DMA_USB,
        ID_DMA_SD,
        __ID_DMA_END
    } e_dma_id;

    parameter bit [31:0] SC64_VER   = 32'h53437632;

    parameter int CLOCK_FREQUENCY   = 32'd100_000_000;

    parameter bit CPU_HAS_UART      = 1'b0;

    parameter int UART_BAUD_RATE    = 32'd1_000_000;

endpackage
