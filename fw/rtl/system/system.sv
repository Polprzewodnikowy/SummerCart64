interface if_system (
    input in_clk,
    input n64_reset,
    input n64_nmi
);

    logic clk;
    logic sdram_clk;
    logic reset;
    logic n64_soft_reset;
    logic n64_hard_reset;

    modport internal (
        input in_clk,
        input n64_reset,
        input n64_nmi,
        output clk,
        output sdram_clk,
        output reset,
        output n64_soft_reset,
        output n64_hard_reset
    );

    modport sys (
        input clk,
        input reset,
        input n64_soft_reset,
        input n64_hard_reset
    );

    modport sdram (
        input sdram_clk
    );

endinterface


module system (if_system.internal sys);

    logic locked;
    logic external_reset;
    logic [1:0] n64_reset_ff;
    logic [1:0] n64_nmi_ff;

    intel_pll intel_pll_inst (
        .inclk0(sys.in_clk),
        .c0(sys.clk),
        .c1(sys.sdram_clk),
        .locked(locked)
    );

    generate
        if (sc64::DEBUG_ENABLED) begin
            intel_snp intel_snp_inst (
                .source(external_reset),
                .source_clk(sys.clk)
            );
        end
    endgenerate

    always_ff @(posedge sys.clk) begin
        n64_reset_ff <= {n64_reset_ff[0], sys.n64_reset};
        n64_nmi_ff <= {n64_nmi_ff[0], sys.n64_nmi};
    end

    always_comb begin
        sys.reset = ~locked | external_reset;
        sys.n64_hard_reset = ~n64_reset_ff[1];
        sys.n64_soft_reset = ~n64_nmi_ff[1];
    end

endmodule
