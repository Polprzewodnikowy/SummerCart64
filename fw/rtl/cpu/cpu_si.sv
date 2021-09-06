module cpu_si (
    if_system.sys sys,
    if_cpu_bus bus,
    if_si.cpu si
);
    always_ff @(posedge sys.clk) begin
        bus.ack <= 1'b0;
        if (bus.request) begin
            bus.ack <= 1'b1;
        end
    end

    always_comb begin
        bus.rdata = 32'd0;
        if (bus.ack) begin
            case (bus.address[3:2])
                0: bus.rdata = {
                    20'd0,
                    si.rx_length[6:3],
                    4'd0,
                    si.tx_busy,
                    1'b0,
                    si.rx_data[0],
                    si.rx_ready
                };
                1: bus.rdata = {si.rx_data[56:49], si.rx_data[64:57], si.rx_data[72:65], si.rx_data[80:73]};
                2: bus.rdata = {si.rx_data[24:17], si.rx_data[32:25], si.rx_data[40:33], si.rx_data[48:41]};
                3: bus.rdata = {16'd0, si.rx_data[8:1], si.rx_data[16:9]};
                default: bus.rdata = 32'd0;
            endcase
        end
    end

    always_comb begin
        si.tx_data = {bus.wdata[7:0], bus.wdata[15:8], bus.wdata[23:16], bus.wdata[31:24]};
        si.tx_length = bus.wdata[22:16];
    end

    always_ff @(posedge sys.clk) begin
        si.tx_reset <= 1'b0;
        si.rx_reset <= 1'b0;
        si.tx_start <= 1'b0;
        si.tx_wmask <= 3'b000;

        if (bus.request && (&bus.wmask)) begin
            case (bus.address[3:2])
                0: begin
                    si.tx_reset <= bus.wdata[7];
                    si.rx_reset <= bus.wdata[6];
                    si.tx_start <= bus.wdata[2];
                end
                1: si.tx_wmask[0] <= 1'b1;
                2: si.tx_wmask[1] <= 1'b1;
                3: si.tx_wmask[2] <= 1'b1;
            endcase
        end
    end

endmodule
