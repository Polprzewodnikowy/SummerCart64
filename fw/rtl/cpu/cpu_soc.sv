module cpu_soc (
    if_system.sys system_if,

    input [7:0] gpio_i,
    output [7:0] gpio_o,
    output [7:0] gpio_oe,

    output usb_clk,
    output usb_cs,
    input usb_miso,
    inout [3:0] usb_miosi,

    output ftdi_clk,
    output ftdi_si,
    input ftdi_so,
    input ftdi_cts,

    inout scl,
    inout sda
);

    if_cpu_bus_out cpu_bus_if ();

    wire cpu_ack;
    wire [31:0] cpu_rdata;

    picorv32 #(
        .ENABLE_COUNTERS(0),
        .ENABLE_COUNTERS64(0),
        .ENABLE_REGS_16_31(1),
        .ENABLE_REGS_DUALPORT(1),
        .LATCHED_MEM_RDATA(0),
        .TWO_STAGE_SHIFT(0),
        .BARREL_SHIFTER(0),
        .TWO_CYCLE_COMPARE(0),
        .TWO_CYCLE_ALU(0),
        .COMPRESSED_ISA(0),
        .CATCH_MISALIGN(0),
        .CATCH_ILLINSN(0),
        .ENABLE_PCPI(0),
        .ENABLE_MUL(0),
        .ENABLE_FAST_MUL(0),
        .ENABLE_DIV(0),
        .ENABLE_IRQ(0),
        .ENABLE_IRQ_QREGS(0),
        .ENABLE_IRQ_TIMER(0),
        .ENABLE_TRACE(0),
        .REGS_INIT_ZERO(0),
        .MASKED_IRQ(32'h0000_0000),
        .LATCHED_IRQ(32'hFFFF_FFFF),
        .PROGADDR_RESET(32'hF000_0000),
        .PROGADDR_IRQ(32'h0000_0010),
        .STACKADDR(32'hFFFF_FFFF)
    ) cpu_inst (
        .clk(system_if.clk),
        .resetn(~system_if.reset),
        .mem_valid(cpu_bus_if.req),
        .mem_ready(cpu_ack),
        .mem_addr(cpu_bus_if.address),
        .mem_wdata(cpu_bus_if.wdata),
        .mem_wstrb(cpu_bus_if.wstrb),
        .mem_rdata(cpu_rdata)
    );


    if_cpu_bus_in cpu_ram_if ();
    cpu_ram cpu_ram_inst (.*);

    if_cpu_bus_in cpu_bootloader_if ();
    cpu_bootloader cpu_bootloader_inst (.*);

    if_cpu_bus_in cpu_gpio_if ();
    cpu_gpio cpu_gpio_inst (
        .*,
        .gpio_i(gpio_i),
        .gpio_o(gpio_o),
        .gpio_oe(gpio_oe)
    );

    if_cpu_bus_in cpu_uart_if ();
    cpu_uart cpu_uart_inst (
        .*,
        .ftdi_clk(ftdi_clk),
        .ftdi_si(ftdi_si),
        .ftdi_so(ftdi_so),
        .ftdi_cts(ftdi_cts)
    );

    if_cpu_bus_in cpu_i2c_if ();
    cpu_i2c cpu_i2c_inst (
        .*,
        .scl(scl),
        .sda(sda)
    );

    if_cpu_bus_in cpu_usb_if ();
    cpu_usb cpu_usb_inst (
        .*,
        .usb_clk(usb_clk),
        .usb_cs(usb_cs),
        .usb_miso(usb_miso),
        .usb_miosi(usb_miosi)
    );


    assign cpu_ack = (
        cpu_ram_if.ack |
        cpu_bootloader_if.ack |
        cpu_gpio_if.ack |
        cpu_uart_if.ack |
        cpu_i2c_if.ack |
        cpu_usb_if.ack
    );
    assign cpu_rdata = (
        cpu_ram_if.rdata |
        cpu_bootloader_if.rdata |
        cpu_gpio_if.rdata |
        cpu_uart_if.rdata |
        cpu_i2c_if.rdata |
        cpu_usb_if.rdata
    );

endmodule
