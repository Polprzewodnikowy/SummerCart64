module cpu_soc (
    if_system.sys sys,
    if_config cfg,
    if_dma.cpu dma,

    input [7:0] gpio_i,
    output [7:0] gpio_o,
    output [7:0] gpio_oe,

    output i2c_scl,
    inout i2c_sda,

    output usb_clk,
    output usb_cs,
    input usb_miso,
    inout [3:0] usb_miosi,
    input usb_pwren,

    input uart_rxd,
    output uart_txd,
    input uart_cts,
    output uart_rts,

    output sd_clk,
    inout sd_cmd,
    inout [3:0] sd_dat
);

    if_cpu_bus bus ();

    cpu_wrapper cpu_wrapper_inst (
        .sys(sys),
        .bus(bus)
    );

    cpu_ram cpu_ram_inst (
        .sys(sys),
        .bus(bus.at[sc64::ID_CPU_RAM].device)
    );

    cpu_bootloader cpu_bootloader_inst (
        .sys(sys),
        .bus(bus.at[sc64::ID_CPU_BOOTLOADER].device)
    );

    cpu_gpio cpu_gpio_inst (
        .sys(sys),
        .bus(bus.at[sc64::ID_CPU_GPIO].device),
        .gpio_i(gpio_i),
        .gpio_o(gpio_o),
        .gpio_oe(gpio_oe)
    );

    cpu_i2c cpu_i2c_inst (
        .sys(sys),
        .bus(bus.at[sc64::ID_CPU_I2C].device),
        .i2c_scl(i2c_scl),
        .i2c_sda(i2c_sda)
    );

    cpu_usb cpu_usb_inst (
        .sys(sys),
        .bus(bus.at[sc64::ID_CPU_USB].device),
        .dma(dma),
        .usb_clk(usb_clk),
        .usb_cs(usb_cs),
        .usb_miso(usb_miso),
        .usb_miosi(usb_miosi),
        .usb_pwren(usb_pwren)
    );

    cpu_uart cpu_uart_inst (
        .sys(sys),
        .bus(bus.at[sc64::ID_CPU_UART].device),
        .uart_rxd(uart_rxd),
        .uart_txd(uart_txd),
        .uart_cts(uart_cts),
        .uart_rts(uart_rts)
    );

endmodule
