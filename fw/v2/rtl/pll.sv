interface if_pll (
    input in_clk
);

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


module pll (if_pll.pll iface);

    wire locked;

    assign iface.reset = ~locked;

    intel_pll intel_pll_inst (
        .inclk0(iface.in_clk),
        .c0(iface.clk),
        .c1(iface.sdram_clk),
        .locked(locked)
    );

endmodule
