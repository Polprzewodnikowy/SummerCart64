module sd_interface (
    input i_clk,
    input i_reset,

    output o_sd_clk,
    inout io_sd_cmd,
    inout [3:0] io_sd_dat,

    input i_request,
    input i_write,
    output o_busy,
    output o_ack,
    input [3:0] i_address,
    output [31:0] o_data,
    input [31:0] i_data,

    output o_dma_request,
    output o_dma_write,
    input i_dma_busy,
    input i_dma_ack,
    output [3:0] o_dma_bank,
    output [23:0] o_dma_address,
    input [31:0] i_dma_data,
    output [31:0] o_dma_data
);

    // Clock generator

    wire [1:0] w_sd_clk_config;
    wire w_sd_clk_strobe_rising;
    wire w_sd_clk_strobe_falling;

    sd_clk sd_clk_inst (
        .i_clk(i_clk),
        .i_reset(i_reset),

        .i_sd_clk_config(w_sd_clk_config),

        .o_sd_clk_strobe_rising(w_sd_clk_strobe_rising),
        .o_sd_clk_strobe_falling(w_sd_clk_strobe_falling),

        .o_sd_clk(o_sd_clk)
    );


    // Command path

    wire [5:0] w_command_index;
    wire [31:0] w_command_argument;
    wire w_command_skip_response;
    wire w_command_long_response;
    wire [5:0] w_command_response_index;
    wire [31:0] w_command_response;
    wire w_command_start;
    wire w_command_busy;
    wire w_command_timeout;
    wire w_command_response_crc_error;

    sd_cmd sd_cmd_inst (
        .i_clk(i_clk),
        .i_reset(i_reset),

        .io_sd_cmd(io_sd_cmd),

        .i_sd_clk_strobe_rising(w_sd_clk_strobe_rising),
        .i_sd_clk_strobe_falling(w_sd_clk_strobe_falling),

        .i_command_index(w_command_index),
        .i_command_argument(w_command_argument),
        .i_command_long_response(w_command_long_response),
        .i_command_skip_response(w_command_skip_response),
        .o_command_index(w_command_response_index),
        .o_command_response(w_command_response),
        .i_command_start(w_command_start),
        .o_command_busy(w_command_busy),
        .o_command_timeout(w_command_timeout),
        .o_command_response_crc_error(w_command_response_crc_error)
    );


    // SD to FPGA (RX) data path FIFO

    wire w_rx_fifo_flush;
    wire w_rx_fifo_push;
    wire w_rx_fifo_regs_pop;
    wire w_rx_fifo_dma_pop;
    wire w_rx_fifo_empty;
    wire [7:0] w_rx_fifo_items;
    wire w_rx_fifo_overrun;
    wire [31:0] w_rx_fifo_i_data;
    wire [31:0] w_rx_fifo_o_data;

    sd_fifo sd_fifo_rx_inst (
        .i_clk(i_clk),
        .i_reset(i_reset),

        .i_fifo_flush(w_rx_fifo_flush),
        .i_fifo_push(w_rx_fifo_push),
        .i_fifo_pop(w_rx_fifo_regs_pop || w_rx_fifo_dma_pop),
        .o_fifo_empty(w_rx_fifo_empty),
        .o_fifo_full(),
        .o_fifo_items(w_rx_fifo_items),
        .o_fifo_underrun(),
        .o_fifo_overrun(w_rx_fifo_overrun),
        .i_fifo_data(w_rx_fifo_i_data),
        .o_fifo_data(w_rx_fifo_o_data)
    );

    
    // FPGA to SD (TX) data path FIFO

    wire w_tx_fifo_flush;
    wire w_tx_fifo_regs_push;
    wire w_tx_fifo_dma_push;
    wire w_tx_fifo_pop;
    wire w_tx_fifo_empty;
    wire w_tx_fifo_full;
    reg [31:0] r_tx_fifo_i_data;
    wire [31:0] w_tx_fifo_o_data;

    wire [31:0] w_tx_fifo_i_data_regs;
    wire [31:0] w_tx_fifo_i_data_dma;

    always @(*) begin
        r_tx_fifo_i_data = 32'h0000_0000;
        if (w_tx_fifo_regs_push) r_tx_fifo_i_data = w_tx_fifo_i_data_regs;
        if (w_tx_fifo_dma_push) r_tx_fifo_i_data = w_tx_fifo_i_data_dma;
    end

    sd_fifo sd_fifo_tx_inst (
        .i_clk(i_clk),
        .i_reset(i_reset),

        .i_fifo_flush(w_tx_fifo_flush),
        .i_fifo_push(w_tx_fifo_regs_push || w_tx_fifo_dma_push),
        .i_fifo_pop(w_tx_fifo_pop),
        .o_fifo_empty(w_tx_fifo_empty),
        .o_fifo_full(w_tx_fifo_full),
        .o_fifo_items(),
        .o_fifo_underrun(),
        .o_fifo_overrun(),
        .i_fifo_data(r_tx_fifo_i_data),
        .o_fifo_data(w_tx_fifo_o_data)
    );


    // Data path

    wire w_dat_width;
    wire w_dat_direction;
    wire [6:0] w_dat_block_size;
    wire [10:0] w_dat_num_blocks;
    wire w_dat_start;
    wire w_dat_stop;
    wire w_dat_busy;
    wire w_dat_crc_error;

    sd_dat sd_dat_inst (
        .i_clk(i_clk),
        .i_reset(i_reset),

        .io_sd_dat(io_sd_dat),

        .i_sd_clk_strobe_rising(w_sd_clk_strobe_rising),
        .i_sd_clk_strobe_falling(w_sd_clk_strobe_falling),

        .i_dat_width(w_dat_width),
        .i_dat_direction(w_dat_direction),
        .i_dat_block_size(w_dat_block_size),
        .i_dat_num_blocks(w_dat_num_blocks),
        .i_dat_start(w_dat_start),
        .i_dat_stop(w_dat_stop),
        .o_dat_busy(w_dat_busy),
        .o_dat_crc_error(w_dat_crc_error),

        .o_rx_fifo_push(w_rx_fifo_push),
        .i_rx_fifo_overrun(w_rx_fifo_overrun),
        .o_rx_fifo_data(w_rx_fifo_i_data),

        .i_tx_fifo_full(w_tx_fifo_full),
        .o_tx_fifo_pop(w_tx_fifo_pop),
        .i_tx_fifo_data(w_tx_fifo_o_data)
    );


    // DMA

    wire [3:0] w_dma_bank;
    wire [23:0] w_dma_address;
    wire [14:0] w_dma_length;
    wire [14:0] w_dma_left;
    wire w_dma_load_bank_address;
    wire w_dma_load_length;
    wire w_dma_direction;
    wire w_dma_start;
    wire w_dma_stop;
    wire w_dma_busy;

    sd_dma sd_dma_inst (
        .i_clk(i_clk),
        .i_reset(i_reset),

        .i_dma_bank(w_dma_bank),
        .i_dma_address(w_dma_address),
        .i_dma_length(w_dma_length),
        .o_dma_left(w_dma_left),
        .i_dma_load_bank_address(w_dma_load_bank_address),
        .i_dma_load_length(w_dma_load_length),
        .i_dma_direction(w_dma_direction),
        .i_dma_start(w_dma_start),
        .i_dma_stop(w_dma_stop),
        .o_dma_busy(w_dma_busy),

        .o_rx_fifo_pop(w_rx_fifo_dma_pop),
        .i_rx_fifo_empty(w_rx_fifo_empty),
        .i_rx_fifo_data(w_rx_fifo_o_data),

        .o_tx_fifo_push(w_tx_fifo_dma_push),
        .i_tx_fifo_full(w_tx_fifo_full),
        .o_tx_fifo_data(w_tx_fifo_i_data_dma),

        .o_request(o_dma_request),
        .o_write(o_dma_write),
        .i_busy(i_dma_busy),
        .i_ack(i_dma_ack),
        .o_bank(o_dma_bank),
        .o_address(o_dma_address),
        .i_data(i_dma_data),
        .o_data(o_dma_data)
    );


    // Peripheral registers

    sd_regs sd_regs_inst (
        .i_clk(i_clk),
        .i_reset(i_reset),

        .o_sd_clk_config(w_sd_clk_config),

        .o_command_index(w_command_index),
        .o_command_argument(w_command_argument),
        .o_command_long_response(w_command_long_response),
        .o_command_skip_response(w_command_skip_response),
        .i_command_index(w_command_response_index),
        .i_command_response(w_command_response),
        .o_command_start(w_command_start),
        .i_command_busy(w_command_busy),
        .i_command_timeout(w_command_timeout),
        .i_command_response_crc_error(w_command_response_crc_error),

        .o_dat_width(w_dat_width),
        .o_dat_direction(w_dat_direction),
        .o_dat_block_size(w_dat_block_size),
        .o_dat_num_blocks(w_dat_num_blocks),
        .o_dat_start(w_dat_start),
        .o_dat_stop(w_dat_stop),
        .i_dat_busy(w_dat_busy),
        .i_dat_crc_error(w_dat_crc_error),

        .o_rx_fifo_flush(w_rx_fifo_flush),
        .o_rx_fifo_pop(w_rx_fifo_regs_pop),
        .i_rx_fifo_items(w_rx_fifo_items),
        .i_rx_fifo_overrun(w_rx_fifo_overrun),
        .i_rx_fifo_data(w_rx_fifo_o_data),

        .o_tx_fifo_flush(w_tx_fifo_flush),
        .o_tx_fifo_push(w_tx_fifo_regs_push),
        .i_tx_fifo_empty(w_tx_fifo_empty),
        .i_tx_fifo_full(w_tx_fifo_full),
        .o_tx_fifo_data(w_tx_fifo_i_data_regs),

        .o_dma_bank(w_dma_bank),
        .o_dma_address(w_dma_address),
        .o_dma_length(w_dma_length),
        .i_dma_bank(o_dma_bank),
        .i_dma_address(o_dma_address),
        .i_dma_left(w_dma_left),
        .o_dma_load_bank_address(w_dma_load_bank_address),
        .o_dma_load_length(w_dma_load_length),
        .o_dma_direction(w_dma_direction),
        .o_dma_start(w_dma_start),
        .o_dma_stop(w_dma_stop),
        .i_dma_busy(w_dma_busy),

        .i_request(i_request),
        .i_write(i_write),
        .o_busy(o_busy),
        .o_ack(o_ack),
        .i_address(i_address),
        .o_data(o_data),
        .i_data(i_data)
    );

endmodule
