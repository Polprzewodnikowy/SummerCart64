module cpu_bootloader (
    if_system.sys sys,
    if_cpu_bus bus
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
            case (bus.address[6:2])
                0: bus.rdata = 32'h00000793;
                1: bus.rdata = 32'h00000713;
                2: bus.rdata = 32'h50000637;
                3: bus.rdata = 32'h02000593;
                4: bus.rdata = 32'h00064683;
                5: bus.rdata = 32'h0016f693;
                6: bus.rdata = 32'hfe068ce3;
                7: bus.rdata = 32'h00464683;
                8: bus.rdata = 32'h00f696b3;
                9: bus.rdata = 32'h00878793;
                10: bus.rdata = 32'h00d76733;
                11: bus.rdata = 32'hfeb792e3;
                12: bus.rdata = 32'h00000793;
                13: bus.rdata = 32'h50000537;
                14: bus.rdata = 32'h70000637;
                15: bus.rdata = 32'h80000837;
                16: bus.rdata = 32'h00054683;
                17: bus.rdata = 32'h0016f693;
                18: bus.rdata = 32'hfe068ce3;
                19: bus.rdata = 32'h00454683;
                20: bus.rdata = 32'h00178593;
                21: bus.rdata = 32'h0ff6f693;
                22: bus.rdata = 32'h00d78023;
                23: bus.rdata = 32'h00e59c63;
                24: bus.rdata = 32'h00062783;
                25: bus.rdata = 32'h0107e7b3;
                26: bus.rdata = 32'h00f62023;
                27: bus.rdata = 32'hf0000097;
                28: bus.rdata = 32'hf94080e7;
                29: bus.rdata = 32'h00058793;
                30: bus.rdata = 32'hfc9ff06f;
                default: bus.rdata = 32'd0;
            endcase
        end
    end

endmodule
