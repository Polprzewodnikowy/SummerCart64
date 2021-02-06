module sd_cmd (
    input i_clk,
    input i_reset,

    inout io_sd_cmd,

    input i_sd_clk_strobe_rising,
    input i_sd_clk_strobe_falling,

    input [5:0] i_command_index,
    input [31:0] i_command_argument,
    input i_command_long_response,
    input i_command_skip_response,
    output reg [5:0] o_command_index,
    output reg [31:0] o_command_response,
    input i_command_start,
    output o_command_busy,
    output reg o_command_timeout,
    output reg o_command_response_crc_error
);

    // Start synchronizer

    reg r_pending_start;

    wire w_start = r_pending_start && i_sd_clk_strobe_falling;

    always @(posedge i_clk) begin
        if (i_reset) begin
            r_pending_start <= 1'b0;
        end else begin
            if (i_command_start) begin
                r_pending_start <= 1'b1;
            end else if (i_sd_clk_strobe_falling) begin
                r_pending_start <= 1'b0;
            end
        end
    end


    // Module state

    localparam STATE_IDLE       = 0;
    localparam STATE_SENDING    = 1;
    localparam STATE_RESP_WAIT  = 2;
    localparam STATE_RECEIVING  = 3;
    localparam STATE_HIZ_WAIT   = 4;

    reg [4:0] r_state;


    // Bit counter logic

    reg [8:0] r_bit_counter;
    reg r_bit_done;

    wire w_command_start = w_start && r_state[STATE_IDLE];
    wire w_command_end = r_bit_done && r_state[STATE_SENDING];
    wire w_response_timeout = r_bit_done && r_state[STATE_RESP_WAIT];
    wire w_response_start = !io_sd_cmd && i_sd_clk_strobe_rising && r_state[STATE_RESP_WAIT];
    wire w_response_end = r_bit_done && r_state[STATE_RECEIVING];
    wire w_hiz_end = r_bit_done && r_state[STATE_HIZ_WAIT];

    localparam [8:0] COMMAND_BIT_LENGTH         = (9'd48 - 1'd1);
    localparam [8:0] RESPONSE_WAIT_BIT_LENGTH   = (9'd64 - 1'd1);
    localparam [8:0] RESPONSE_SHORT_BIT_LENGTH  = (9'd48 - 1'd1);
    localparam [8:0] RESPONSE_LONG_BIT_LENGTH   = (9'd136 - 1'd1);
    localparam [8:0] HIZ_WAIT_BIT_LENGTH        = (9'd8 - 1'd1);

    always @(posedge i_clk) begin
        if (w_command_start) begin
            r_bit_counter <= COMMAND_BIT_LENGTH;
            r_bit_done <= 1'b0;
        end else if (w_command_end) begin
            r_bit_counter <= RESPONSE_WAIT_BIT_LENGTH;
            r_bit_done <= 1'b0;
        end else if (w_response_start) begin
            r_bit_counter <= i_command_long_response ? RESPONSE_LONG_BIT_LENGTH : RESPONSE_SHORT_BIT_LENGTH;
            r_bit_done <= 1'b0;
        end else if (w_response_end) begin
            r_bit_counter <= HIZ_WAIT_BIT_LENGTH;
            r_bit_done <= 1'b0;
        end else if (i_sd_clk_strobe_rising) begin
            if (r_bit_counter > 9'd0) begin
                r_bit_counter <= r_bit_counter - 1'd1;
            end else begin
                r_bit_done <= 1'b1;
            end
        end
    end


    // CRC7 generator

    reg [6:0] r_crc_7_received;

    wire w_crc_shift_reset = !(r_state[STATE_SENDING] || r_state[STATE_RECEIVING]);
    wire w_crc_shift_enabled = (r_bit_counter <= 9'd127) && (r_bit_counter > (r_state[STATE_SENDING] ? 9'd7 : 9'd8));
    wire w_crc_shift = w_crc_shift_enabled && i_sd_clk_strobe_rising;
    wire [6:0] w_crc_7_calculated;

    sd_crc_7 sd_crc_7_inst (
        .i_clk(i_clk),
        .i_crc_reset(w_crc_shift_reset),
        .i_crc_shift(w_crc_shift),
        .i_crc_input(io_sd_cmd),
        .o_crc_output(w_crc_7_calculated)
    );


    // Control signals

    assign o_command_busy = r_pending_start || (!r_state[STATE_IDLE]);

    always @(posedge i_clk) begin
        if (i_reset) begin
            o_command_timeout <= 1'b0;
            o_command_response_crc_error <= 1'b0;
        end else begin
            if (w_command_start) begin
                o_command_timeout <= 1'b0;
            end

            if (w_response_timeout) begin
                o_command_timeout <= 1'b1;
            end

            if (w_response_end) begin
                o_command_response_crc_error <= (w_crc_7_calculated != r_crc_7_received);
            end
        end
    end


    // State machine

    always @(posedge i_clk) begin
        if (i_reset) begin
            r_state <= (1'b1 << STATE_IDLE);
        end else begin
            r_state <= 5'b00000;

            unique case (1'b1)
                r_state[STATE_IDLE]: begin
                    if (w_start) begin
                        r_state[STATE_SENDING] <= 1'b1;
                    end else begin
                        r_state[STATE_IDLE] <= 1'b1;
                    end
                end

                r_state[STATE_SENDING]: begin
                    if (w_command_end) begin
                        if (i_command_skip_response) begin
                            r_state[STATE_HIZ_WAIT] <= 1'b1;
                        end else begin
                            r_state[STATE_RESP_WAIT] <= 1'b1;
                        end
                    end else begin
                        r_state[STATE_SENDING] <= 1'b1;
                    end
                end

                r_state[STATE_RESP_WAIT]: begin
                    if (w_response_timeout) begin
                        r_state[STATE_IDLE] <= 1'b1;
                    end else if (w_response_start) begin
                        r_state[STATE_RECEIVING] <= 1'b1;
                    end else begin
                        r_state[STATE_RESP_WAIT] <= 1'b1;
                    end
                end

                r_state[STATE_RECEIVING]: begin
                    if (w_response_end) begin
                        r_state[STATE_HIZ_WAIT] <= 1'b1;
                    end else begin
                        r_state[STATE_RECEIVING] <= 1'b1;
                    end
                end

                r_state[STATE_HIZ_WAIT]: begin
                    if (w_hiz_end) begin
                        r_state[STATE_IDLE] <= 1'b1;
                    end else begin
                        r_state[STATE_HIZ_WAIT] <= 1'b1;
                    end
                end
            endcase
        end
    end


    // Shifting operation

    reg [7:0] r_shift;

    wire w_shift = (r_state[STATE_SENDING] && i_sd_clk_strobe_falling) || (r_state[STATE_RECEIVING] && i_sd_clk_strobe_rising);

    assign io_sd_cmd = r_state[STATE_SENDING] ? r_shift[7] : 1'bZ;

    always @(posedge i_clk) begin
        if (w_command_start) begin
            o_command_response <= 32'h0000_0000;
            r_shift <= {2'b01, i_command_index};
        end else if (w_response_start) begin
            r_shift <= 8'h00;
        end else begin
            if (w_shift) begin
                r_shift <= {r_shift[6:0], io_sd_cmd};
            end

            if (i_sd_clk_strobe_falling && r_state[STATE_SENDING]) begin
                if (r_bit_counter == 9'd39) begin
                    r_shift <= i_command_argument[31:24];
                end

                if (r_bit_counter == 9'd31) begin
                    r_shift <= i_command_argument[23:16];
                end

                if (r_bit_counter == 9'd23) begin
                    r_shift <= i_command_argument[15:8];
                end

                if (r_bit_counter == 9'd15) begin
                    r_shift <= i_command_argument[7:0];
                end

                if (r_bit_counter == 9'd7) begin
                    r_shift <= {w_crc_7_calculated, 1'b1};
                end
            end

            if (i_sd_clk_strobe_rising && r_state[STATE_RECEIVING]) begin
                if (r_bit_counter == (i_command_long_response ? 9'd128 : 9'd40)) begin
                    o_command_index <= r_shift[5:0];
                end else if (r_bit_counter[2:0] == 3'd0) begin
                    if (r_bit_counter > 9'd7) begin
                        o_command_response <= {o_command_response[23:0], r_shift};
                    end else begin
                        r_crc_7_received <= r_shift[7:1];
                    end
                end
            end
        end
    end

endmodule
