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
                0: bus.rdata = 32'h50000737;
                1: bus.rdata = 32'h00074783;
                2: bus.rdata = 32'h0027f793;
                3: bus.rdata = 32'hfe078ce3;
                4: bus.rdata = 32'h03e00793;
                5: bus.rdata = 32'h00f70223;
                6: bus.rdata = 32'h50000637;
                7: bus.rdata = 32'h00000793;
                8: bus.rdata = 32'h00000713;
                9: bus.rdata = 32'h02000593;
                10: bus.rdata = 32'h00064683;
                11: bus.rdata = 32'h0016f693;
                12: bus.rdata = 32'hfe068ce3;
                13: bus.rdata = 32'h00464683;
                14: bus.rdata = 32'h00f696b3;
                15: bus.rdata = 32'h00878793;
                16: bus.rdata = 32'h00d76733;
                17: bus.rdata = 32'hfeb792e3;
                18: bus.rdata = 32'h00000793;
                19: bus.rdata = 32'h500005b7;
                20: bus.rdata = 32'h0005c683;
                21: bus.rdata = 32'h0016f693;
                22: bus.rdata = 32'hfe068ce3;
                23: bus.rdata = 32'h0045c683;
                24: bus.rdata = 32'h00178613;
                25: bus.rdata = 32'h0ff6f693;
                26: bus.rdata = 32'h00d78023;
                27: bus.rdata = 32'h00e61663;
                28: bus.rdata = 32'hf0000097;
                29: bus.rdata = 32'hf90080e7;
                30: bus.rdata = 32'h00060793;
                31: bus.rdata = 32'hfd5ff06f;
                default: bus.rdata = 32'd0;
            endcase
        end
    end

endmodule
