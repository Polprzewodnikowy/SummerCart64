module sd_cmd (
    input clk,
    input reset,

    sd_scb.cmd sd_scb,

    input sd_clk_rising,
    input sd_clk_falling,

    inout sd_cmd
);

    // Input and output data sampling

    logic sd_cmd_oe;
    logic sd_cmd_out;
    logic sd_cmd_in;
    logic sd_cmd_oe_data;
    logic sd_cmd_data;

    assign sd_cmd = sd_cmd_oe ? sd_cmd_out : 1'bZ;

    always_ff @(posedge clk) begin
        sd_cmd_oe <= sd_cmd_oe_data;
        sd_cmd_out <= sd_cmd_data;
        sd_cmd_in <= sd_cmd;
    end


    // CMD state

    typedef enum bit [1:0] {
        STATE_IDLE,
        STATE_TX,
        STATE_WAIT,
        STATE_RX
    } e_state;

    e_state state;
    e_state next_state;

    always_ff @(posedge clk) begin
        if (reset) begin
            state <= STATE_IDLE;
        end else begin
            state <= next_state;
        end
    end

    assign sd_scb.cmd_busy = (state != STATE_IDLE);

    logic [7:0] counter;

    always_comb begin
        next_state = state;

        case (state)
            STATE_IDLE: begin
                if (sd_scb.cmd_start) begin
                    next_state = STATE_TX;
                end
            end

            STATE_TX: begin
                if (sd_clk_falling) begin
                    if (counter == 8'd48) begin
                        if (sd_scb.cmd_skip_response) begin
                            next_state = STATE_IDLE;
                        end else begin
                            next_state = STATE_WAIT;
                        end
                    end
                end
            end

            STATE_WAIT: begin
                if (sd_clk_rising) begin
                    if (counter == 8'd64) begin
                        next_state = STATE_IDLE;
                    end
                    if (!sd_cmd_in) begin
                        next_state = STATE_RX;
                    end
                end
            end

            STATE_RX: begin
                if (sd_clk_rising) begin
                    if (sd_scb.cmd_long_response) begin
                        if (counter == 8'd136) begin
                            next_state = STATE_IDLE;
                        end
                    end else begin
                        if (counter == 8'd48) begin
                            next_state = STATE_IDLE;
                        end
                    end
                end
            end

            default: begin
                next_state = STATE_IDLE;
            end
        endcase
    end


    // CRC7 unit

    logic crc_reset;
    logic crc_enable;
    logic crc_data;
    logic [6:0] crc_result;

    sd_crc_7 sd_crc_7_inst (
        .clk(clk),
        .reset(crc_reset),
        .enable(crc_enable),
        .data(crc_data),
        .result(crc_result)
    );


    // Data shifting

    logic [7:0] data_shift;

    assign crc_data = (state == STATE_RX) ? data_shift[0] : data_shift[7];

    always_ff @(posedge clk) begin
        crc_reset <= 1'b0;
        crc_enable <= 1'b0;

        if (reset) begin
            sd_cmd_oe_data <= 1'b0;
            sd_cmd_data <= 1'b1;
        end else begin
            case (state)
                STATE_IDLE: begin
                    if (sd_scb.cmd_start) begin
                        sd_scb.cmd_error <= 1'b0;
                        crc_reset <= 1'b1;
                        data_shift <= {2'b01, sd_scb.cmd_index};
                        counter <= 8'd0;
                    end
                end

                STATE_TX: begin
                    if (sd_clk_falling) begin
                        sd_cmd_oe_data <= 1'b1;
                        sd_cmd_data <= data_shift[7];
                        counter <= counter + 1'd1;
                        crc_enable <= 1'b1;
                        data_shift <= {data_shift[6:0], 1'bX};
                        if (counter == 8'd7) begin
                            data_shift <= sd_scb.cmd_arg[31:24];
                        end
                        if (counter == 8'd15) begin
                            data_shift <= sd_scb.cmd_arg[23:16];
                        end
                        if (counter == 8'd23) begin
                            data_shift <= sd_scb.cmd_arg[15:8];
                        end
                        if (counter == 8'd31) begin
                            data_shift <= sd_scb.cmd_arg[7:0];
                        end
                        if (counter == 8'd39) begin
                            data_shift <= {crc_result, 1'b1};
                        end
                        if (counter == 8'd48) begin
                            sd_cmd_oe_data <= 1'b0;
                            counter <= 8'd0;
                        end
                    end
                end

                STATE_WAIT: begin
                    if (sd_clk_rising) begin
                        counter <= counter + 1'd1;
                        if (counter == 8'd64) begin
                            sd_scb.cmd_error <= 1'b1;
                        end
                        if (!sd_cmd_in) begin
                            counter <= 8'd1;
                            crc_reset <= 1'b1;
                            data_shift <= 8'h00;
                        end
                    end
                end

                STATE_RX: begin
                    if (sd_clk_rising) begin
                        counter <= counter + 1'd1;
                        data_shift <= {data_shift[6:0], sd_cmd_in};
                        if (counter == 8'd8) begin
                            if (data_shift[6:0] != (sd_scb.cmd_reserved_response ? 7'h3F : {1'b0, sd_scb.cmd_index})) begin
                                sd_scb.cmd_error <= 1'b1;
                            end
                        end
                        if (sd_scb.cmd_long_response) begin
                            if (counter >= 8'd8 && counter < 8'd128) begin
                                crc_enable <= 1'b1;
                            end
                            if (counter[2:0] == 3'd0) begin
                                sd_scb.cmd_rsp <= {sd_scb.cmd_rsp[119:0], data_shift};
                            end
                            if (!sd_scb.cmd_ignore_crc && counter == 8'd136) begin
                                if (data_shift[7:1] != crc_result) begin
                                    sd_scb.cmd_error <= 1'b1;
                                end
                            end
                        end else begin
                            if (counter < 8'd40) begin
                                crc_enable <= 1'b1;
                            end
                            if (counter <= 8'd40 && counter[2:0] == 3'd0) begin
                                sd_scb.cmd_rsp <= {sd_scb.cmd_rsp[119:0], data_shift};
                            end
                            if (!sd_scb.cmd_ignore_crc && counter == 8'd48) begin
                                if (data_shift[7:1] != crc_result) begin
                                    sd_scb.cmd_error <= 1'b1;
                                end
                            end
                        end
                    end
                end
            endcase
        end
    end

endmodule
