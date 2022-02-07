module vendor_reconfigure (
    input clk,
    input reset,

    input trigger_reconfiguration
);

    logic [1:0] ru_clk;
    logic ru_rconfig;
    logic ru_regout;

    logic pending;

    always_ff @(posedge clk) begin
        if (reset) begin
            ru_clk <= 2'd0;
            ru_rconfig <= 1'b0;
            pending <= 1'b0;
        end else begin
            ru_clk <= ru_clk + 1'd1;

            if (trigger_reconfiguration) begin
                pending <= 1'b1;
            end

            if (ru_clk == 2'd1) begin
                ru_rconfig <= pending;
            end
        end
    end

    fiftyfivenm_rublock fiftyfivenm_rublock_inst (
        .clk(ru_clk[1]),
        .shiftnld(1'b0),
        .captnupdt(1'b0),
        .regin(1'b0),
        .rsttimer(1'b0),
        .rconfig(ru_rconfig),
        .regout(ru_regout)
    );

endmodule
