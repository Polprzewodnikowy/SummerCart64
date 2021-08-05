interface if_system (input in_clk);

    logic clk;
    logic sdram_clk;
    logic reset;

    modport pll (
        input in_clk,
        output clk,
        output sdram_clk,
        output reset
    );

    modport sys (
        input clk,
        input reset
    );

    modport sdram (
        input sdram_clk
    );

endinterface


module system (if_system.pll system_if);

    wire locked;
    wire external_reset;

    assign system_if.reset = ~locked | external_reset;

    intel_pll intel_pll_inst (
        .inclk0(system_if.in_clk),
        .c0(system_if.clk),
        .c1(system_if.sdram_clk),
        .locked(locked)
    );

    intel_snp intel_snp_inst (
        .source(external_reset),
        .source_clk(system_if.clk)
    );

endmodule
