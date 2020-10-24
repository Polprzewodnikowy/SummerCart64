module sdram (
    input i_clk,
    input i_reset,

    output o_sdram_cs,
    output o_sdram_ras,
    output o_sdram_cas,
    output o_sdram_we,
    output [1:0] o_sdram_ba,
    output [12:0] o_sdram_a,
    inout [15:0] io_sdram_dq,

    input i_request,
    input i_write,
    output reg o_busy,
    output reg o_ack,
    input [24:0] i_address,
    output [31:0] o_data,
    input [31:0] i_data
);

endmodule
