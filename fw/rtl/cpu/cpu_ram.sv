module cpu_ram (
    if_system.sys system_if,
    if_cpu_bus_out cpu_bus_if,
    if_cpu_bus_in cpu_ram_if
);

    wire request;
    wire [31:0] rdata;

    cpu_bus_glue #(.ADDRESS(4'h0)) cpu_bus_glue_ram_inst (
        .*,
        .cpu_peripheral_if(cpu_ram_if),
        .request(request),
        .rdata(rdata)
    );

    wire bank;
    reg [3:0][7:0] ram_1 [0:4095];
    reg [3:0][7:0] ram_2 [0:2047];
    reg [31:0] q_1, q_2;
    wire [31:0] q;

    assign bank = cpu_bus_if.address[14];
    assign rdata = bank ? q_2 : q_1;

    always_ff @(posedge system_if.clk) begin
        q_1 <= ram_1[cpu_bus_if.address[13:2]];
        if (request & !bank) begin
            if (cpu_bus_if.wstrb[0]) ram_1[cpu_bus_if.address[13:2]][0] <= cpu_bus_if.wdata[7:0];
            if (cpu_bus_if.wstrb[1]) ram_1[cpu_bus_if.address[13:2]][1] <= cpu_bus_if.wdata[15:8];
            if (cpu_bus_if.wstrb[2]) ram_1[cpu_bus_if.address[13:2]][2] <= cpu_bus_if.wdata[23:16];
            if (cpu_bus_if.wstrb[3]) ram_1[cpu_bus_if.address[13:2]][3] <= cpu_bus_if.wdata[31:24];
        end

        q_2 <= ram_2[cpu_bus_if.address[12:2]];
        if (request & bank) begin
            if (cpu_bus_if.wstrb[0]) ram_2[cpu_bus_if.address[12:2]][0] <= cpu_bus_if.wdata[7:0];
            if (cpu_bus_if.wstrb[1]) ram_2[cpu_bus_if.address[12:2]][1] <= cpu_bus_if.wdata[15:8];
            if (cpu_bus_if.wstrb[2]) ram_2[cpu_bus_if.address[12:2]][2] <= cpu_bus_if.wdata[23:16];
            if (cpu_bus_if.wstrb[3]) ram_2[cpu_bus_if.address[12:2]][3] <= cpu_bus_if.wdata[31:24];
        end
    end

endmodule
