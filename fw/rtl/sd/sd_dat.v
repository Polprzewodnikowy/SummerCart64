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
    output reg o_dat_crc_error,

    output reg o_rx_fifo_push,
    input i_rx_fifo_overrun,
    output reg [31:0] o_rx_fifo_data,

    input i_tx_fifo_full,
    output reg o_tx_fifo_pop,
    input [31:0] i_tx_fifo_data
);

    // Module state

    localparam STATE_IDLE       = 0;
    localparam STATE_READ_WAIT  = 1;
    localparam STATE_RECEIVING  = 2;

    reg [2:0] r_state;


    // Bit counter logic

    reg [12:0] r_bit_counter;
    reg r_bit_done;

    wire w_start_bit = !io_sd_dat[0] && i_sd_clk_strobe_rising && r_state[STATE_READ_WAIT];
    wire w_data_end = r_bit_done && r_state[STATE_RECEIVING];

    assign o_dat_busy = !r_state[STATE_IDLE];

    always @(posedge i_clk) begin
        if (w_start_bit) begin
            r_bit_counter <= (i_dat_width ? (
                    {2'b00, {1'b0, i_dat_block_size} + 1'd1, 3'b000}
                ) : (
                    {{1'b0, i_dat_block_size} + 1'd1, 5'b00000}
                )) + 13'd16;
            r_bit_done <= 1'b0;
        end else if (i_sd_clk_strobe_rising) begin
            if (r_bit_counter > 13'd0) begin
                r_bit_counter <= r_bit_counter - 1'd1;
            end else begin
                r_bit_done <= 1'b1;
            end
        end
    end


    // Block counter logic

    reg [10:0] r_block_counter;

    wire w_read_start = i_dat_start && r_state[STATE_IDLE];
    wire w_read_stop = r_block_counter == 11'd0;

    always @(posedge i_clk) begin
        if (w_read_start) begin
            r_block_counter <= i_dat_num_blocks;
        end else if (w_data_end) begin
            if (r_block_counter > 11'd0) begin
                r_block_counter <= r_block_counter - 1'd1;
            end
        end
    end


    // CRC16 generator

    reg [15:0] r_crc_16_received [0:3];

    wire w_crc_shift_reset = !r_state[STATE_RECEIVING];
    wire w_crc_shift_enabled = r_bit_counter > 13'd16;
    wire w_crc_shift = w_crc_shift_enabled && i_sd_clk_strobe_rising;
    wire [15:0] w_crc_16_calculated [0:3];

    wire w_crc_error = (r_bit_counter == 13'd0) && (i_dat_width ? (
        (w_crc_16_calculated[0] != r_crc_16_received[0]) &&
        (w_crc_16_calculated[1] != r_crc_16_received[1]) &&
        (w_crc_16_calculated[2] != r_crc_16_received[2]) &&
        (w_crc_16_calculated[3] != r_crc_16_received[3])
    ) : (
        w_crc_16_calculated[0] != r_crc_16_received[0]
    ));

    genvar dat_index;
    generate
        for (dat_index = 0; dat_index < 4; dat_index = dat_index + 1) begin : crc_16_loop
            sd_crc_16 sd_crc_16_inst (
                .i_clk(i_clk),
                .i_crc_reset(w_crc_shift_reset),
                .i_crc_shift(w_crc_shift),
                .i_crc_input(io_sd_dat[dat_index]),
                .o_crc_output(w_crc_16_calculated[dat_index])
            );
        end
    endgenerate


    // Control signals

    always @(posedge i_clk) begin
        if (i_reset) begin
            o_dat_crc_error <= 1'b0;
        end else begin
            if (w_data_end) begin
                o_dat_crc_error <= w_crc_error;
            end
        end
    end


    // State machine

    always @(posedge i_clk) begin
        if (i_reset) begin
            r_state <= (1'b1 << STATE_IDLE);
        end else begin
            r_state <= 3'b000;

            if (i_dat_stop) begin
                r_state[STATE_IDLE] <= 1'b1;
            end else begin
                unique case (1'b1)
                    r_state[STATE_IDLE]: begin
                        if (i_dat_start) begin
                            if (i_dat_direction) begin
                                r_state[STATE_IDLE] <= 1'b1;   // TODO: Sending
                            end else begin
                                r_state[STATE_READ_WAIT] <= 1'b1;
                            end
                        end else begin
                            r_state[STATE_IDLE] <= 1'b1;
                        end
                    end

                    r_state[STATE_READ_WAIT]: begin
                        if (w_start_bit) begin
                            r_state[STATE_RECEIVING] <= 1'b1;
                        end else begin
                            r_state[STATE_READ_WAIT] <= 1'b1;
                        end
                    end

                    r_state[STATE_RECEIVING]: begin
                        if (w_crc_error || i_rx_fifo_overrun) begin
                            r_state[STATE_IDLE] <= 1'b1;
                        end else if (w_data_end) begin
                            if (w_read_stop) begin
                                r_state[STATE_IDLE] <= 1'b1;
                            end else begin
                                r_state[STATE_READ_WAIT] <= 1'b1;
                            end
                        end else begin
                            r_state[STATE_RECEIVING] <= 1'b1;
                        end
                    end
                endcase
            end
        end
    end


    // Shifting operation

    wire [31:0] w_shift_1_bit = {o_rx_fifo_data[30:0], io_sd_dat[0]};
    wire [31:0] w_shift_4_bit = {o_rx_fifo_data[27:0], io_sd_dat};

    always @(posedge i_clk) begin
        o_rx_fifo_push <= 1'b0;

        if (i_sd_clk_strobe_rising && r_state[STATE_RECEIVING]) begin
            if (r_bit_counter > 13'd16) begin
                if (i_dat_width) begin
                    o_rx_fifo_data <= w_shift_4_bit;
                    if (r_bit_counter[2:0] == 3'd1) begin
                        o_rx_fifo_push <= 1'b1;
                    end
                end else begin
                    o_rx_fifo_data <= w_shift_1_bit;
                    if (r_bit_counter[4:0] == 5'd17) begin
                        o_rx_fifo_push <= 1'b1;
                    end
                end
            end else if (r_bit_counter > 13'd0) begin
                for (integer i = 0; i < 4; i = i + 1) begin
                    r_crc_16_received[i] <= {r_crc_16_received[i][14:0], io_sd_dat[i]};
                end
            end
        end
    end

endmodule
