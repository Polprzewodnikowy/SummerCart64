module sd_top (
    input clk,
    input reset,

    sd_scb sd_scb,

    fifo_bus.fifo fifo_bus,

    output sd_clk,
    inout sd_cmd,
    inout [3:0] sd_dat
);

    logic sd_clk_rising;
    logic sd_clk_falling;

    sd_clk sd_clk_inst (
        .clk(clk),
        .reset(reset),

        .sd_scb(sd_scb),

        .sd_clk_rising(sd_clk_rising),
        .sd_clk_falling(sd_clk_falling),

        .sd_clk(sd_clk)
    );

    sd_cmd sd_cmd_inst (
        .clk(clk),
        .reset(reset),

        .sd_scb(sd_scb),

        .sd_clk_rising(sd_clk_rising),
        .sd_clk_falling(sd_clk_falling),

        .sd_cmd(sd_cmd)
    );

    sd_dat sd_dat_inst (
        .clk(clk),
        .reset(reset),

        .sd_scb(sd_scb),

        .fifo_bus(fifo_bus),

        .sd_clk_rising(sd_clk_rising),
        .sd_clk_falling(sd_clk_falling),

        .sd_dat(sd_dat)
    );

endmodule
