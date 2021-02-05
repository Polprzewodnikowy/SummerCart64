module sd_dma (
    input i_clk,
    input i_reset,

    input [3:0] i_dma_bank,
    input [23:0] i_dma_address,
    input [14:0] i_dma_length,
    output reg [14:0] o_dma_left,
    input i_dma_load_bank_address,
    input i_dma_load_length,
    input i_dma_direction,
    input i_dma_start,
    input i_dma_stop,
    output reg o_dma_busy,

    output reg o_rx_fifo_pop,
    input i_rx_fifo_empty,
    input [31:0] i_rx_fifo_data,

    output reg o_tx_fifo_push,
    input i_tx_fifo_full,
    output reg [31:0] o_tx_fifo_data,

    output reg o_request,
    output reg o_write,
    input i_busy,
    input i_ack,
    output reg [3:0] o_bank,
    output reg [23:0] o_address,
    input [31:0] i_data,
    output reg [31:0] o_data
);

endmodule
