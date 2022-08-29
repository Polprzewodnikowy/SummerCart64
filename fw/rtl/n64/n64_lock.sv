module n64_lock (
    input clk,
    input reset,

    n64_reg_bus.lock reg_bus,

    n64_scb.lock n64_scb,
);

    const bit [15:0] UNLOCK_SEQUENCE [4] = {
        16'h5F55,
        16'h4E4C,
        16'h4F43,
        16'h4B5F
    };

    always_comb begin
        reg_bus.rdata = 16'd0;
    end

    logic [1:0] sequence_counter;

    always_ff @(posedge clk) begin
        if (reset || n64_scb.n64_reset || n64_scb.n64_nmi) begin
            n64_scb.cfg_unlock <= 1'b0;
            sequence_counter <= 2'd0;
        end else begin
            if (reg_bus.write) begin
                if (reg_bus.address[16] && (reg_bus.address[15:2] == 14'd0)) begin
                    for (int i = 0; i < $size(UNLOCK_SEQUENCE); i++) begin
                        if (sequence_counter == i) begin
                            if (reg_bus.wdata == UNLOCK_SEQUENCE[i]) begin
                                sequence_counter <= sequence_counter + 1'd1;
                                if (i == ($size(UNLOCK_SEQUENCE) - 1'd1)) begin
                                    n64_scb.cfg_unlock <= 1'b1;
                                    sequence_counter <= 2'd0;
                                end
                            end else begin
                                n64_scb.cfg_unlock <= 1'b0;
                                sequence_counter <= 2'd0;
                            end
                        end
                    end
                end else begin
                    n64_scb.cfg_unlock <= 1'b0;
                    sequence_counter <= 2'd0;
                end
            end
        end
    end

endmodule
