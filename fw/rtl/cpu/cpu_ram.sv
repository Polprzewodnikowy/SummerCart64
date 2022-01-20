interface if_cpu_ram ();

    logic write;
    logic [12:0] address;
    logic [15:0] rdata;
    logic [15:0] wdata;

    modport cpu (
        input write,
        input address,
        output rdata,
        input wdata
    );

    modport external (
        output write,
        output address,
        input rdata,
        output wdata
    );

endinterface


module cpu_ram (
    if_system.sys sys,
    if_cpu_bus bus,
    if_cpu_ram.cpu cpu_ram
);

    logic [3:0][7:0] ram [0:4095];
    logic [31:0] q_cpu;
    logic [31:0] q_external;

    always_ff @(posedge sys.clk) begin
        bus.ack <= 1'b0;
        if (bus.request) begin
            bus.ack <= 1'b1;
        end
    end

    always_comb begin
        cpu_ram.rdata = cpu_ram.address[0] ? q_external[31:16] : q_external[15:0];
        bus.rdata = 32'd0;
        if (bus.ack) begin
            bus.rdata = q_cpu;
        end
    end

    always_ff @(posedge sys.clk) begin
        q_cpu <= ram[bus.address[13:2]];
        if (bus.request) begin
            if (bus.wmask[0]) ram[bus.address[13:2]][0] <= bus.wdata[7:0];
            if (bus.wmask[1]) ram[bus.address[13:2]][1] <= bus.wdata[15:8];
            if (bus.wmask[2]) ram[bus.address[13:2]][2] <= bus.wdata[23:16];
            if (bus.wmask[3]) ram[bus.address[13:2]][3] <= bus.wdata[31:24];
        end
    end

    always_ff @(posedge sys.clk) begin
        q_external <= ram[cpu_ram.address[12:1]];
        if (cpu_ram.write) begin
            if (cpu_ram.address[0]) begin
                ram[cpu_ram.address[12:1]][2] <= cpu_ram.wdata[7:0];
                ram[cpu_ram.address[12:1]][3] <= cpu_ram.wdata[15:8];
            end else begin
                ram[cpu_ram.address[12:1]][0] <= cpu_ram.wdata[7:0];
                ram[cpu_ram.address[12:1]][1] <= cpu_ram.wdata[15:8];
            end
        end
    end

endmodule
