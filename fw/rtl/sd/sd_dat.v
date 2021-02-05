module sd_dat (
    input i_clk,
    input i_reset,

    inout reg [3:0] io_sd_dat,

    input i_sd_clk_strobe_rising,
    input i_sd_clk_strobe_falling,

    input i_dat_width,
    input i_dat_direction,
    input [6:0] i_dat_block_size,
    input [10:0] i_dat_num_blocks,
    input i_dat_start,
    input i_dat_stop,
    output o_dat_busy,
    output o_dat_crc_error,

    output reg o_rx_fifo_push,
    input i_rx_fifo_overrun,
    output [31:0] o_rx_fifo_data,

    input i_tx_fifo_full,
    output reg o_tx_fifo_pop,
    input [31:0] i_tx_fifo_data
);

endmodule
