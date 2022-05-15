module sd_cmd (
    input clk,
    input reset,

    sd_scb.cmd sd_scb,

    input sd_clk_rising,
    input sd_clk_falling,

    inout sd_cmd
);

    assign sd_cmd = 1'bZ;

endmodule
