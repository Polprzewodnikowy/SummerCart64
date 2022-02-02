module cpu_soc (
    if_system.sys sys,
    if_config.cpu cfg,
    if_memory_dma usb_dma,
    if_sdram.cpu sdram,
    if_flashram.cpu flashram,
    if_si.cpu si,
    if_flash flash,
    if_dd.cpu dd,

    output i2c_scl,
    inout i2c_sda,

    output usb_clk,
    output usb_cs,
    input usb_miso,
    inout [7:0] usb_miosi,

    input uart_rxd,
    output uart_txd,

    output sd_clk,
    inout sd_cmd,
    inout [3:0] sd_dat
);

    typedef enum bit [3:0] {
        DEV_FLASH,
        DEV_RAM,
        DEV_CFG,
        DEV_I2C,
        DEV_USB,
        DEV_UART,
        DEV_DD,
        DEV_SDRAM,
        DEV_FLASHRAM,
        DEV_SI,
        __NUM_DEVICES
    } e_bus_id;

    if_cpu_bus #(
        .NUM_DEVICES(__NUM_DEVICES)
    ) bus ();

    cpu_wrapper cpu_wrapper_inst (
        .sys(sys),
        .bus(bus)
    );

    cpu_flash cpu_flash_inst (
        .sys(sys),
        .bus(bus.at[DEV_FLASH].device),
        .flash(flash)
    );

    cpu_ram cpu_ram_inst (
        .sys(sys),
        .bus(bus.at[DEV_RAM].device)
    );

    cpu_cfg cpu_cfg_inst (
        .sys(sys),
        .bus(bus.at[DEV_CFG].device),
        .cfg(cfg)
    );

    cpu_i2c cpu_i2c_inst (
        .sys(sys),
        .bus(bus.at[DEV_I2C].device),
        .i2c_scl(i2c_scl),
        .i2c_sda(i2c_sda)
    );

    cpu_usb cpu_usb_inst (
        .sys(sys),
        .bus(bus.at[DEV_USB].device),
        .dma(usb_dma),
        .usb_clk(usb_clk),
        .usb_cs(usb_cs),
        .usb_miso(usb_miso),
        .usb_miosi(usb_miosi)
    );

    generate
        if (sc64::CPU_HAS_UART) begin
            cpu_uart cpu_uart_inst (
                .sys(sys),
                .bus(bus.at[DEV_UART].device),
                .uart_rxd(uart_rxd),
                .uart_txd(uart_txd)
            );
        end
    endgenerate

    cpu_dd cpu_dd_inst (
        .sys(sys),
        .bus(bus.at[DEV_DD].device),
        .dd(dd)
    );

    cpu_sdram cpu_sdram_inst (
        .sys(sys),
        .bus(bus.at[DEV_SDRAM].device),
        .sdram(sdram)
    );

    cpu_flashram cpu_flashram_inst (
        .sys(sys),
        .bus(bus.at[DEV_FLASHRAM].device),
        .flashram(flashram)
    );

    cpu_si cpu_si_inst (
        .sys(sys),
        .bus(bus.at[DEV_SI].device),
        .si(si)
    );

    assign sd_clk = 1'bZ;
    assign sd_cmd = 1'bZ;
    assign sd_dat = 4'bZZZZ;

endmodule
