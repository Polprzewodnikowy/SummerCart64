module cpu_soc (
    if_system.sys system_if,

    input [7:0] gpio_i,
    output [7:0] gpio_o,
    output [7:0] gpio_oe,

    output usb_clk,
    output usb_cs,
    input usb_miso,
    inout [3:0] usb_miosi,

    inout i2c_scl,
    inout i2c_sda
);

    enum bit [3:0] {
        RAM,
        BOOTLOADER,
        GPIO,
        I2C,
        USB,
        __NUM_DEVICES
    } e_address_map;

    if_cpu_bus #(
        .NUM_DEVICES(__NUM_DEVICES)
    ) bus (
        .clk(system_if.clk),
        .reset(system_if.reset)
    );

    cpu_wrapper # (
        .ENTRY_DEVICE(BOOTLOADER)
    ) cpu_wrapper_inst (
        .bus(bus)
    );

    cpu_ram cpu_ram_inst (
        .bus(bus.at[RAM].device)
    );

    cpu_bootloader cpu_bootloader_inst (
        .bus(bus.at[BOOTLOADER].device)
    );

    cpu_gpio cpu_gpio_inst (
        .bus(bus.at[GPIO].device),
        .gpio_i(gpio_i),
        .gpio_o(gpio_o),
        .gpio_oe(gpio_oe)
    );

    cpu_i2c cpu_i2c_inst (
        .bus(bus.at[I2C].device),
        .i2c_scl(i2c_scl),
        .i2c_sda(i2c_sda)
    );

    cpu_usb cpu_usb_inst (
        .system_if(system_if),
        .bus(bus.at[USB].device),
        .usb_clk(usb_clk),
        .usb_cs(usb_cs),
        .usb_miso(usb_miso),
        .usb_miosi(usb_miosi)
    );

endmodule
