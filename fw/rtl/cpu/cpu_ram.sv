module cpu_ram (
    if_system.sys sys,
    if_cpu_bus bus
);

    logic [3:0][7:0] ram [0:8191];
    logic [31:0] q;

    always_ff @(posedge sys.clk) begin
        bus.ack <= 1'b0;
        if (bus.request) begin
            bus.ack <= 1'b1;
        end
    end

    always_comb begin
        bus.rdata = 32'd0;
        if (bus.ack) begin
            bus.rdata = q;
        end
    end

    always_ff @(posedge sys.clk) begin
        q <= ram[bus.address[14:2]];
        if (bus.request) begin
            if (bus.wmask[0]) ram[bus.address[14:2]][0] <= bus.wdata[7:0];
            if (bus.wmask[1]) ram[bus.address[14:2]][1] <= bus.wdata[15:8];
            if (bus.wmask[2]) ram[bus.address[14:2]][2] <= bus.wdata[23:16];
            if (bus.wmask[3]) ram[bus.address[14:2]][3] <= bus.wdata[31:24];
        end
    end

endmodule
