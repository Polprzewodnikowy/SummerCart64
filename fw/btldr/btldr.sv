module cpu_bootloader (if_cpu_bus bus);

    always_ff @(posedge bus.clk) begin
        bus.ack <= 1'b0;
        if (bus.request) begin
            bus.ack <= 1'b1;
        end
    end

    always_comb begin
        bus.rdata = 32'd0;
        if (bus.ack) begin
            case (bus.address[6:2])
                0: bus.rdata = 32'h40000737;
                1: bus.rdata = 32'h00074783;
                2: bus.rdata = 32'h0027f793;
                3: bus.rdata = 32'hfe078ce3;
                4: bus.rdata = 32'h03e00793;
                5: bus.rdata = 32'h00f70223;
                6: bus.rdata = 32'h400006b7;
                7: bus.rdata = 32'h00000793;
                8: bus.rdata = 32'h00006637;
                9: bus.rdata = 32'h0006c703;
                10: bus.rdata = 32'h00177713;
                11: bus.rdata = 32'h00070a63;
                12: bus.rdata = 32'h0046c703;
                13: bus.rdata = 32'h00178793;
                14: bus.rdata = 32'h0ff77713;
                15: bus.rdata = 32'hfee78fa3;
                16: bus.rdata = 32'hfec792e3;
                17: bus.rdata = 32'hf0000097;
                18: bus.rdata = 32'hfbc080e7;
                19: bus.rdata = 32'hfd9ff06f;
                default: bus.rdata = 32'd0;
            endcase
        end
    end

endmodule
