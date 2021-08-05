module cpu_rom (
    if_system.sys system_if,
    if_cpu_bus_out cpu_bus_if,
    if_cpu_bus_in cpu_rom_if
);

    wire request;
    wire ack;
    wire [31:0] rdata;

    assign request = (cpu_bus_if.address[31:24] == 8'h03) && cpu_bus_if.req;
    assign cpu_rom_if.ack = ack & request;
    assign cpu_rom_if.rdata = cpu_rom_if.ack ? rdata : 32'd0;

    intel_flash intel_flash_inst (
        .clock(system_if.clk),
        .avmm_data_addr(cpu_bus_if.address[17:2]),
        .avmm_data_read(request),
        .avmm_data_readdata(rdata),
        .avmm_data_waitrequest(),
        .avmm_data_readdatavalid(ack),
        .avmm_data_burstcount(2'd1),
        .reset_n(~system_if.reset)
    );

endmodule
