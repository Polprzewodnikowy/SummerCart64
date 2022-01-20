module cpu_soc (
    if_system.sys sys,
    if_config.cpu cfg,
    if_dma dma,
    if_sdram.cpu sdram,
    if_flashram.cpu flashram,
    if_si.cpu si,
    if_flash.cpu flash,
    if_dd.cpu dd,
    if_cpu_ram.cpu cpu_ram,

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
        .bus(bus.at[sc64::ID_CPU_RAM].device),
        .cpu_ram(cpu_ram)
    );

    cpu_flash cpu_flash_inst (
        .sys(sys),
        .bus(bus.at[sc64::ID_CPU_FLASH].device),
        .flash(flash)
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
        .dma(dma.at[sc64::ID_DMA_USB].device),
        .usb_clk(usb_clk),
        .usb_cs(usb_cs),
        .usb_miso(usb_miso),
        .usb_miosi(usb_miosi),
        .usb_pwren(usb_pwren)
    );

    generate
        if (sc64::CPU_HAS_UART) begin
            cpu_uart cpu_uart_inst (
                .sys(sys),
                .bus(bus.at[sc64::ID_CPU_UART].device),
                .uart_rxd(uart_rxd),
                .uart_txd(uart_txd)
            );
        end
    endgenerate

    cpu_dma cpu_dma_inst (
        .sys(sys),
        .bus(bus.at[sc64::ID_CPU_DMA].device),
        .dma(dma)
    );

    cpu_cfg cpu_cfg_inst (
        .sys(sys),
        .bus(bus.at[sc64::ID_CPU_CFG].device),
        .cfg(cfg)
    );

    cpu_sdram cpu_sdram_inst (
        .sys(sys),
        .bus(bus.at[sc64::ID_CPU_SDRAM].device),
        .sdram(sdram)
    );

    cpu_flashram cpu_flashram_inst (
        .sys(sys),
        .bus(bus.at[sc64::ID_CPU_FLASHRAM].device),
        .flashram(flashram)
    );

    cpu_si cpu_si_inst (
        .sys(sys),
        .bus(bus.at[sc64::ID_CPU_SI].device),
        .si(si)
    );

    cpu_dd cpu_dd_inst (
        .sys(sys),
        .bus(bus.at[sc64::ID_CPU_DD].device),
        .dd(dd)
    );

    assign sd_clk = 1'bZ;
    assign sd_cmd = 1'bZ;
    assign sd_dat = 4'bZZZZ;

endmodule
