module sd_dat (
    input i_clk,
    input i_reset,

    inout reg [3:0] io_sd_dat,

    input i_sd_clk_strobe_rising,
    input i_sd_clk_strobe_falling,

    input i_dat_width,
    input i_dat_direction,
    input [6:0] i_dat_block_size,
    input [7:0] i_dat_num_blocks,
    input i_dat_start,
    input i_dat_stop,
    output o_dat_busy,
    output o_dat_write_busy,
    output reg o_dat_crc_error,
    output reg o_dat_write_error,
    output reg o_dat_write_ok,

    input i_rx_fifo_overrun,
    output reg o_rx_fifo_push,
    output reg [31:0] o_rx_fifo_data,

    input [8:0] i_tx_fifo_items,
    input i_tx_fifo_underrun,
    output reg o_tx_fifo_pop,
    input [31:0] i_tx_fifo_data
);

    // Module state

    localparam STATE_IDLE       = 0;
    localparam STATE_READ_WAIT  = 1;
    localparam STATE_RECEIVING  = 2;
    localparam STATE_WRITE_WAIT = 3;
    localparam STATE_SENDING    = 4;
    localparam STATE_STATUS     = 5;
    localparam STATE_BUSY       = 6;

    reg [6:0] r_state;

    assign o_dat_busy = !r_state[STATE_IDLE];
    assign o_dat_write_busy = !io_sd_dat[0];


    // Bit counter logic

    reg [12:0] r_bit_counter;
    reg r_bit_counter_done;

    wire w_read_start = r_state[STATE_READ_WAIT] && !io_sd_dat[0] && i_sd_clk_strobe_rising;
    wire w_write_start = r_state[STATE_WRITE_WAIT] && (i_tx_fifo_items >= ({1'b0, i_dat_block_size} + 1)) && i_sd_clk_strobe_falling;
    wire w_status_start = r_state[STATE_SENDING] && r_bit_counter_done;
    wire w_status_done = r_state[STATE_STATUS] && r_bit_counter_done;
    wire w_block_read_done = r_state[STATE_RECEIVING] && r_bit_counter_done;
    wire w_block_write_done = r_state[STATE_SENDING] && r_bit_counter_done;
    wire w_block_write_busy_done = r_state[STATE_BUSY] && io_sd_dat[0];
    wire w_block_done = w_block_read_done || w_block_write_busy_done;

    wire [12:0] w_block_bit_length = i_dat_width ? ({2'b00, {1'b0, i_dat_block_size} + 1'd1, 3'b000}) : ({{1'b0, i_dat_block_size} + 1'd1, 5'b00000});

    always @(posedge i_clk) begin
        if (w_read_start) begin
            r_bit_counter <= w_block_bit_length + 13'd16;
            r_bit_counter_done <= 1'b0;
        end else if (w_write_start) begin
            r_bit_counter <= w_block_bit_length + 13'd17;
            r_bit_counter_done <= 1'b0;
        end else if (w_status_start) begin
            r_bit_counter <= 13'd6;
            r_bit_counter_done <= 1'b0;
        end else if (i_sd_clk_strobe_rising) begin
            if (r_bit_counter > 13'd0) begin
                r_bit_counter <= r_bit_counter - 1'd1;
            end else begin
                r_bit_counter_done <= 1'd1;
            end
        end
    end


    // Block counter logic

    reg [7:0] r_block_counter;

    wire w_block_start = i_dat_start && r_state[STATE_IDLE];
    wire w_block_stop = r_block_counter == 8'd0;

    always @(posedge i_clk) begin
        if (w_block_start) begin
            r_block_counter <= i_dat_num_blocks;
        end else if (w_block_done) begin
            if (r_block_counter > 8'd0) begin
                r_block_counter <= r_block_counter - 1'd1;
            end
        end
    end


    // CRC16 generator

    reg [15:0] r_crc_16_received [0:3];

    wire w_crc_shift_reset = !(r_state[STATE_RECEIVING] || r_state[STATE_SENDING]);
    wire w_crc_shift_enabled = r_bit_counter > 13'd16;
    wire w_crc_shift = w_crc_shift_enabled && i_sd_clk_strobe_rising;
    wire [15:0] w_crc_16_calculated [0:3];

    wire w_crc_read_error = (r_bit_counter == 13'd0) && (i_dat_width ? (
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

    localparam [4:0] STATUS_NO_ERROR    = 5'b00101;
    localparam [4:0] STATUS_CRC_ERROR   = 5'b01011;
    localparam [4:0] STATUS_WRITE_ERROR = 5'b01101;

    reg [4:0] r_status;

    wire w_no_write_error = r_status == STATUS_NO_ERROR;
    wire w_crc_write_error = r_status == STATUS_CRC_ERROR;
    wire w_data_write_error = r_status == STATUS_WRITE_ERROR;

    always @(posedge i_clk) begin
        if (i_reset || w_block_start) begin
            o_dat_crc_error <= 1'b0;
            o_dat_write_error <= 1'b0;
        end else begin
            if (w_block_read_done) begin
                o_dat_crc_error <= w_crc_read_error;
            end
            if (w_status_done) begin
                o_dat_crc_error <= w_crc_write_error;
                o_dat_write_error <= w_data_write_error;
                o_dat_write_ok <= w_no_write_error;
            end
        end
    end


    // State machine

    always @(posedge i_clk) begin
        if (i_reset) begin
            r_state <= (1'b1 << STATE_IDLE);
        end else begin
            r_state <= 7'b0000000;

            if (i_dat_stop) begin
                r_state[STATE_IDLE] <= 1'b1;
            end else begin
                unique case (1'b1)
                    r_state[STATE_IDLE]: begin
                        if (i_dat_start) begin
                            if (i_dat_direction) begin
                                r_state[STATE_WRITE_WAIT] <= 1'b1;
                            end else begin
                                r_state[STATE_READ_WAIT] <= 1'b1;
                            end
                        end else begin
                            r_state[STATE_IDLE] <= 1'b1;
                        end
                    end

                    r_state[STATE_READ_WAIT]: begin
                        if (w_read_start) begin
                            r_state[STATE_RECEIVING] <= 1'b1;
                        end else begin
                            r_state[STATE_READ_WAIT] <= 1'b1;
                        end
                    end

                    r_state[STATE_RECEIVING]: begin
                        if (w_crc_read_error || i_rx_fifo_overrun) begin
                            r_state[STATE_IDLE] <= 1'b1;
                        end else if (w_block_read_done) begin
                            if (w_block_stop) begin
                                r_state[STATE_IDLE] <= 1'b1;
                            end else begin
                                r_state[STATE_READ_WAIT] <= 1'b1;
                            end
                        end else begin
                            r_state[STATE_RECEIVING] <= 1'b1;
                        end
                    end

                    r_state[STATE_WRITE_WAIT]: begin
                        if (w_write_start) begin
                            r_state[STATE_SENDING] <= 1'b1;
                        end else begin
                            r_state[STATE_WRITE_WAIT] <= 1'b1;
                        end
                    end

                    r_state[STATE_SENDING]: begin
                        if (i_tx_fifo_underrun) begin
                            r_state[STATE_IDLE] <= 1'b1;
                        end else if (w_block_write_done) begin
                            r_state[STATE_STATUS] <= 1'b1;
                        end else begin
                            r_state[STATE_SENDING] <= 1'b1;
                        end
                    end

                    r_state[STATE_STATUS]: begin
                        if (w_status_done) begin
                            r_state[STATE_BUSY] <= 1'b1;
                        end else begin
                            r_state[STATE_STATUS] <= 1'b1;
                        end
                    end

                    r_state[STATE_BUSY]: begin
                        if (w_block_write_busy_done) begin
                            if (w_block_stop || !w_no_write_error) begin
                                r_state[STATE_IDLE] <= 1'b1;
                            end else begin
                                r_state[STATE_WRITE_WAIT] <= 1'b1;
                            end
                        end else begin
                            r_state[STATE_BUSY] <= 1'b1;
                        end
                    end
                endcase
            end
        end
    end


    // RX shifting operation

    wire w_rx_latch = r_state[STATE_RECEIVING] && i_sd_clk_strobe_rising;
    wire w_rx_data_phase = r_bit_counter >= 13'd17;
    wire w_rx_crc_phase = r_bit_counter >= 13'd1;
    wire w_rx_fifo_push = i_dat_width ? (r_bit_counter[2:0] == 3'd1) : (r_bit_counter[4:0] == 5'd17);
    wire [31:0] w_rx_fifo_shift = i_dat_width ? {o_rx_fifo_data[27:0], io_sd_dat} : {o_rx_fifo_data[30:0], io_sd_dat[0]};

    always @(posedge i_clk) begin
        o_rx_fifo_push <= 1'b0;

        if (w_rx_latch) begin
            if (w_rx_data_phase) begin
                o_rx_fifo_data <= w_rx_fifo_shift;
                o_rx_fifo_push <= w_rx_fifo_push;
            end else if (w_rx_crc_phase) begin
                for (integer i = 0; i < 4; i = i + 1) begin
                    r_crc_16_received[i] <= {r_crc_16_received[i][14:0], io_sd_dat[i]};
                end
            end
        end
    end


    // TX shifting operation

    wire w_tx_latch = r_state[STATE_SENDING] && i_sd_clk_strobe_falling;
    wire w_tx_data_phase = r_bit_counter >= 13'd17;
    wire w_tx_crc_phase = r_bit_counter >= 13'd1;
    wire w_tx_fifo_pop = i_dat_width ? (r_bit_counter[2:0] == 3'd0) : (r_bit_counter[4:0] == 5'd17);
    wire w_tx_shift_load = r_bit_counter[2:0] == 3'd0;

    reg [7:0] r_tx_shift [0:3];

    always @(*) begin
        io_sd_dat = 4'bZZZZ;
        if (r_state[STATE_SENDING]) begin
            io_sd_dat = i_dat_width ? (
                {r_tx_shift[3][7], r_tx_shift[2][7], r_tx_shift[1][7], r_tx_shift[0][7]}
            ) : (
                {3'bZZZ, r_tx_shift[0][7]}
            );
        end
    end

    always @(posedge i_clk) begin
        o_tx_fifo_pop <= 1'b0;

        if (w_write_start) begin
            {r_tx_shift[3][7], r_tx_shift[2][7], r_tx_shift[1][7], r_tx_shift[0][7]} <= 4'b0000;
        end else if (w_tx_latch) begin
            if (w_tx_data_phase) begin
                o_tx_fifo_pop <= w_tx_fifo_pop;
                for (integer i = 0; i < 4; i = i + 1) begin
                    r_tx_shift[i] <= {r_tx_shift[i][6:0], 1'b0};
                end
                if (w_tx_shift_load) begin
                    if (i_dat_width) begin
                        for (integer i = 0; i < 4; i = i + 1) begin
                            for (integer j = 0; j < 8; j = j + 1) begin
                                r_tx_shift[i][j] <= i_tx_fifo_data[((j * 4) + i)];
                            end
                        end
                    end else begin
                        case (r_bit_counter[4:3])
                            2'b10: r_tx_shift[0] <= i_tx_fifo_data[31:24];
                            2'b01: r_tx_shift[0] <= i_tx_fifo_data[23:16];
                            2'b00: r_tx_shift[0] <= i_tx_fifo_data[15:8];
                            2'b11: r_tx_shift[0] <= i_tx_fifo_data[7:0];
                        endcase
                    end
                end
            end else if (w_tx_crc_phase) begin
                for (integer i = 0; i < 4; i = i + 1) begin
                    r_tx_shift[i] <= {r_tx_shift[i][6:0], 1'b0};
                    if (w_tx_shift_load) begin
                        r_tx_shift[i] <= !r_bit_counter[3] ? w_crc_16_calculated[i][15:8] : w_crc_16_calculated[i][7:0];
                    end
                end
            end else begin
                {r_tx_shift[3][7], r_tx_shift[2][7], r_tx_shift[1][7], r_tx_shift[0][7]} <= 4'b1111;
            end
        end
    end


    // Status shifting operation

    wire w_status_latch = r_state[STATE_STATUS] && i_sd_clk_strobe_rising;

    always @(posedge i_clk) begin
        if (w_status_latch) begin
            r_status <= {r_status[3:0], io_sd_dat[0]};
        end
    end

endmodule
