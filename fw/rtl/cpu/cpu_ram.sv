module cpu_ram (
    if_system.sys system_if,
    if_cpu_bus_out cpu_bus_if,
    if_cpu_bus_in cpu_ram_if
);

    wire request;
    wire bank;
    reg ack;
    reg [3:0][7:0] ram1 [0:4095];
    reg [3:0][7:0] ram2 [0:2047];
    reg [31:0] q1;
    reg [31:0] q2;
    wire [31:0] q;

    assign request = (cpu_bus_if.address[31:24] == 8'h00) && cpu_bus_if.req;
    assign bank = cpu_bus_if.address[14];
    assign q = bank ? q2 : q1;
    assign cpu_ram_if.ack = ack & request;
    assign cpu_ram_if.rdata = cpu_ram_if.ack ? q : 32'd0;

    always_ff @(posedge system_if.clk) begin
        ack <= 1'b0;
        if (request) begin
            ack <= 1'b1;
        end

        q1 <= ram1[cpu_bus_if.address[14:2]];
        if (request & !bank) begin
            if (cpu_bus_if.wstrb[0]) ram1[cpu_bus_if.address[13:2]][0] <= cpu_bus_if.wdata[7:0];
            if (cpu_bus_if.wstrb[1]) ram1[cpu_bus_if.address[13:2]][1] <= cpu_bus_if.wdata[15:8];
            if (cpu_bus_if.wstrb[2]) ram1[cpu_bus_if.address[13:2]][2] <= cpu_bus_if.wdata[23:16];
            if (cpu_bus_if.wstrb[3]) ram1[cpu_bus_if.address[13:2]][3] <= cpu_bus_if.wdata[31:24];
        end

        q2 <= ram2[cpu_bus_if.address[12:2]];
        if (request & bank) begin
            if (cpu_bus_if.wstrb[0]) ram2[cpu_bus_if.address[12:2]][0] <= cpu_bus_if.wdata[7:0];
            if (cpu_bus_if.wstrb[1]) ram2[cpu_bus_if.address[12:2]][1] <= cpu_bus_if.wdata[15:8];
            if (cpu_bus_if.wstrb[2]) ram2[cpu_bus_if.address[12:2]][2] <= cpu_bus_if.wdata[23:16];
            if (cpu_bus_if.wstrb[3]) ram2[cpu_bus_if.address[12:2]][3] <= cpu_bus_if.wdata[31:24];
        end
    end

endmodule
