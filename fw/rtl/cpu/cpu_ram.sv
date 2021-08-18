module cpu_ram(if_cpu_bus bus);

    logic bank;
    logic [3:0][7:0] ram_1 [0:4095];
    // logic [3:0][7:0] ram_2 [0:2047];
    logic [31:0] q_1;//, q_2;
    // logic [31:0] q;

    assign bank = bus.address[14];

    always_comb begin
        bus.rdata = 32'd0;
        if (bus.ack) begin
            bus.rdata = q_1;
            // if (bank) bus.rdata = q_2;
        end
    end

    always_ff @(posedge bus.clk) begin
        bus.ack <= 1'b0;
        if (bus.request) begin
            bus.ack <= 1'b1;
        end
    end

    always_ff @(posedge bus.clk) begin        
        q_1 <= ram_1[bus.address[13:2]];
        if (bus.request & !bank) begin
            if (bus.wmask[0]) ram_1[bus.address[13:2]][0] <= bus.wdata[7:0];
            if (bus.wmask[1]) ram_1[bus.address[13:2]][1] <= bus.wdata[15:8];
            if (bus.wmask[2]) ram_1[bus.address[13:2]][2] <= bus.wdata[23:16];
            if (bus.wmask[3]) ram_1[bus.address[13:2]][3] <= bus.wdata[31:24];
        end

        // q_2 <= ram_2[bus.address[12:2]];
        // if (bus.request & bank) begin
        //     if (bus.wmask[0]) ram_2[bus.address[12:2]][0] <= bus.wdata[7:0];
        //     if (bus.wmask[1]) ram_2[bus.address[12:2]][1] <= bus.wdata[15:8];
        //     if (bus.wmask[2]) ram_2[bus.address[12:2]][2] <= bus.wdata[23:16];
        //     if (bus.wmask[3]) ram_2[bus.address[12:2]][3] <= bus.wdata[31:24];
        // end
    end

endmodule
