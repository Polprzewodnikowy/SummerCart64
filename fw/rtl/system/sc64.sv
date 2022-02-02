package sc64;

    typedef enum bit [2:0] {
        ID_N64_SDRAM,
        ID_N64_BOOTLOADER,
        ID_N64_FLASHRAM,
        ID_N64_DD,
        ID_N64_CFG,
        __ID_N64_END
    } e_n64_id;

    parameter bit [31:0] SC64_VER           = 32'h53437632;
    parameter int CLOCK_FREQUENCY           = 32'd100_000_000;
    parameter int UART_BAUD_RATE            = 32'd1_000_000;

`ifdef DEBUG
    parameter bit CPU_HAS_UART              = 1'b1;
`else
    parameter bit CPU_HAS_UART              = 1'b0;
`endif

endpackage
