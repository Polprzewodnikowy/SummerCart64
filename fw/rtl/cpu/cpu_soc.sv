interface if_cpu_soc ();

    logic led;
    logic scl;
    logic sda;

    modport peripherals (
        input led,
        inout scl,
        inout sda
    );

    modport cpu (
        output led,
        inout scl,
        inout sda
    );

endinterface


module cpu_soc (
    if_system.sys system_if,
    if_cpu_soc.cpu cpu_soc_if
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
        .PROGADDR_RESET(32'h0100_0000),
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

    wire scl;
    wire sda;

    assign cpu_soc_if.scl = scl;
    assign cpu_soc_if.sda = sda;

    if_cpu_bus_in cpu_ram_if ();
    cpu_ram cpu_ram_inst (.*);

    if_cpu_bus_in cpu_bootloader_if ();
    cpu_bootloader cpu_bootloader_inst (.*);

    if_cpu_bus_in cpu_led_if ();
    cpu_led cpu_led_inst (.*, .led(cpu_soc_if.led));

    if_cpu_bus_in cpu_i2c_if ();
    cpu_i2c cpu_i2c_inst (.*, .scl(scl), .sda(sda));

    assign cpu_ack = (
        cpu_ram_if.ack |
        cpu_bootloader_if.ack |
        cpu_led_if.ack |
        cpu_i2c_if.ack
    );
    assign cpu_rdata = (
        cpu_ram_if.rdata |
        cpu_bootloader_if.rdata |
        cpu_led_if.rdata |
        cpu_i2c_if.rdata
    );

endmodule
