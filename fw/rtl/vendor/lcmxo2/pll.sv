module pll (
    input inclk,
    output logic reset,
    output clk,
    output sdram_clk
);

    logic pll_sdram_clk;
    logic buf_sdram_clk;
    logic pll_lock;

    pll_lattice_generated pll_lattice_generated_inst (
        .CLKI(inclk),
        .CLKOP(clk),
        .CLKOS(pll_sdram_clk),
        .LOCK(pll_lock)
    );

    ODDRXE oddrxe_sdram_clk_inst (
        .D0(1'b1),
        .D1(1'b0),
        .SCLK(pll_sdram_clk),
        .RST(1'b0),
        .Q(buf_sdram_clk)
    );

    OB ob_sdram_clk_inst (
        .I(buf_sdram_clk),
        .O(sdram_clk)
    ) /* synthesis IO_TYPE="LVCMOS33" */;

    always_ff @(posedge clk) begin
        reset <= ~pll_lock;
    end

endmodule
