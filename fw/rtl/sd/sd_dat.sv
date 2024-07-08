module sd_dat (
    input clk,
    input reset,

    sd_scb.dat sd_scb,

    fifo_bus.fifo fifo_bus,

    input sd_clk_rising,
    input sd_clk_falling,

    inout [3:0] sd_dat
);

    // Input and output data sampling

    logic sd_dat_oe;
    logic [3:0] sd_dat_out;
    logic [3:0] sd_dat_in;
    logic sd_dat_oe_data;
    logic [3:0] sd_dat_data;

    assign sd_dat = sd_dat_oe ? sd_dat_out : 4'hZ;

    always_ff @(posedge clk) begin
        sd_dat_oe <= sd_dat_oe_data;
        sd_dat_out <= sd_dat_data;
        sd_dat_in <= sd_dat;
    end

    always_ff @(posedge clk) begin
        sd_scb.card_busy <= !sd_dat_in[0];
    end


    // FIFO

    logic rx_full;
    logic rx_write;
    logic [7:0] rx_wdata;

    logic tx_empty;
    logic tx_read;
    logic [7:0] tx_rdata;

    fifo_8kb fifo_8kb_rx_inst (
        .clk(clk),
        .reset(reset || sd_scb.dat_fifo_flush),

        .empty(fifo_bus.rx_empty),
        .read(fifo_bus.rx_read),
        .rdata(fifo_bus.rx_rdata),

        .full(rx_full),
        .write(rx_write),
        .wdata(rx_wdata),

        .count(sd_scb.rx_count)
    );

    fifo_8kb fifo_8kb_tx_inst (
        .clk(clk),
        .reset(reset || sd_scb.dat_fifo_flush),

        .empty(tx_empty),
        .read(tx_read),
        .rdata(tx_rdata),

        .full(fifo_bus.tx_full),
        .write(fifo_bus.tx_write),
        .wdata(fifo_bus.tx_wdata),

        .count(sd_scb.tx_count)
    );


    // DAT state

    typedef enum bit [2:0] {
        STATE_IDLE,
        STATE_RX_WAIT,
        STATE_RX,
        STATE_TX_WAIT,
        STATE_TX,
        STATE_TX_STATUS_WAIT,
        STATE_TX_STATUS
    } e_state;

    e_state state;
    e_state next_state;

    always_ff @(posedge clk) begin
        if (reset || sd_scb.dat_stop) begin
            state <= STATE_IDLE;
        end else begin
            state <= next_state;
        end
    end

    assign sd_scb.dat_busy = (state != STATE_IDLE);

    logic [10:0] counter;
    logic [7:0] blocks_remaining;

    always_comb begin
        next_state = state;

        case (state)
            STATE_IDLE: begin
                if (sd_scb.dat_start_read) begin
                    next_state = STATE_RX_WAIT;
                end
                if (sd_scb.dat_start_write) begin
                    next_state = STATE_TX_WAIT;
                end
            end

            STATE_RX_WAIT: begin
                if (sd_clk_rising) begin
                    if (!sd_dat_in[0]) begin
                        next_state = STATE_RX;
                    end
                end
            end

            STATE_RX: begin
                if (sd_clk_rising) begin
                    if (counter == 11'd1041) begin
                        if (blocks_remaining == 8'd0) begin
                            next_state = STATE_IDLE;
                        end else begin
                            next_state = STATE_RX_WAIT;
                        end
                    end
                end
            end

            STATE_TX_WAIT: begin
                if (sd_clk_falling) begin
                    if (sd_scb.tx_count >= 11'd512) begin
                        next_state = STATE_TX;
                    end
                end
            end

            STATE_TX: begin
                if (sd_clk_falling) begin
                    if (counter == 11'd1042) begin
                        next_state = STATE_TX_STATUS_WAIT;
                    end
                end
            end

            STATE_TX_STATUS_WAIT: begin
                if (sd_clk_rising) begin
                    if (counter == 11'd8) begin
                        next_state = STATE_IDLE;
                    end else if (!sd_dat_in[0]) begin
                        next_state = STATE_TX_STATUS;
                    end
                end
            end

            STATE_TX_STATUS: begin
                if (sd_clk_rising) begin
                    if (counter == 11'd5) begin
                        if (sd_dat_in[0]) begin
                            if (blocks_remaining == 8'd0) begin
                                next_state = STATE_IDLE;
                            end else begin
                                next_state = STATE_TX_WAIT;
                            end
                        end
                    end
                end
            end
        endcase
    end


    // CRC16 units

    logic crc_reset;
    logic crc_enable;
    logic crc_shift;
    logic [3:0] crc_data;
    logic [15:0] crc_result [0:3];

    sd_crc_16 sd_crc_16_inst_0 (
        .clk(clk),
        .reset(crc_reset),
        .enable(crc_enable),
        .shift(crc_shift),
        .data(crc_data[0]),
        .result(crc_result[0])
    );

    sd_crc_16 sd_crc_16_inst_1 (
        .clk(clk),
        .reset(crc_reset),
        .enable(crc_enable),
        .shift(crc_shift),
        .data(crc_data[1]),
        .result(crc_result[1])
    );

    sd_crc_16 sd_crc_16_inst_2 (
        .clk(clk),
        .reset(crc_reset),
        .enable(crc_enable),
        .shift(crc_shift),
        .data(crc_data[2]),
        .result(crc_result[2])
    );

    sd_crc_16 sd_crc_16_inst_3 (
        .clk(clk),
        .reset(crc_reset),
        .enable(crc_enable),
        .shift(crc_shift),
        .data(crc_data[3]),
        .result(crc_result[3])
    );


    // Data shifting

    logic [7:0] data_shift;
    logic tx_rdata_valid;

    assign crc_data = (state == STATE_RX) ? rx_wdata[3:0] : sd_dat_data;

    always_comb begin
        tx_read = (state == STATE_TX) && sd_clk_falling && (counter < 11'd1024) && (!counter[0]);
    end

    always_ff @(posedge clk) begin
        rx_write <= 1'b0;
        tx_rdata_valid <= tx_read;
        crc_reset <= 1'b0;
        crc_enable <= 1'b0;
        crc_shift <= 1'b0;

        if (reset || sd_scb.dat_stop) begin
            sd_scb.clock_stop <= 1'b0;
            sd_dat_oe_data <= 1'b0;
            sd_dat_data <= 4'hF;
        end else begin
            case (state)
                STATE_IDLE: begin
                    if (sd_scb.dat_start_read || sd_scb.dat_start_write) begin
                        sd_scb.dat_error <= 1'b0;
                        blocks_remaining <= sd_scb.dat_blocks;
                    end
                end

                STATE_RX_WAIT: begin
                    if (sd_scb.rx_count <= 11'd512) begin
                        sd_scb.clock_stop <= 1'b0;
                    end
                    if (sd_clk_rising) begin
                        if (!sd_dat_in[0]) begin
                            counter <= 11'd1;
                            crc_reset <= 1'b1;
                        end
                    end
                end

                STATE_RX: begin
                    if (sd_clk_rising) begin
                        counter <= counter + 1'd1;
                        rx_wdata <= {rx_wdata[3:0], sd_dat_in};
                        if (counter <= 11'd1024) begin
                            crc_enable <= 1'b1;
                            if (!counter[0]) begin
                                if (rx_full) begin
                                    sd_scb.dat_error <= 1'b1;
                                end else begin
                                    rx_write <= 1'b1;
                                end
                            end
                        end else begin
                            crc_shift <= 1'b1;
                            if ({crc_result[3][15], crc_result[2][15], crc_result[1][15], crc_result[0][15]} != sd_dat_in) begin
                                sd_scb.dat_error <= 1'b1;
                            end
                        end
                        if (counter == 11'd1041) begin
                            if ((blocks_remaining > 8'd0) && (sd_scb.rx_count > 11'd512)) begin
                                sd_scb.clock_stop <= 1'b1;
                            end
                            blocks_remaining <= blocks_remaining - 1'd1;
                        end
                    end
                end

                STATE_TX_WAIT: begin
                    if (sd_clk_falling) begin
                        if (sd_scb.tx_count >= 11'd512) begin
                            counter <= 11'd0;
                        end
                    end
                end

                STATE_TX: begin
                    if (sd_clk_falling) begin
                        counter <= counter + 1'd1;
                        if (counter == 11'd0) begin
                            crc_reset <= 1'b1;
                            sd_dat_oe_data <= 1'b1;
                            sd_dat_data <= 4'h0;
                        end else if (counter <= 11'd1024) begin
                            crc_enable <= 1'b1;
                            {sd_dat_data, data_shift} <= {data_shift, 4'h0};
                        end else begin
                            crc_shift <= 1'b1;
                            sd_dat_data <= {crc_result[3][15], crc_result[2][15], crc_result[1][15], crc_result[0][15]};
                        end
                        if (counter == 11'd1042) begin
                            sd_dat_oe_data <= 1'b0;
                            counter <= 11'd0;
                        end
                    end
                end

                STATE_TX_STATUS_WAIT: begin
                    if (sd_clk_rising) begin
                        counter <= counter + 1'd1;
                        if (counter == 11'd8) begin
                            sd_scb.dat_error <= 1'b1;
                        end else if (!sd_dat_in[0]) begin
                            counter <= 11'd1;
                        end
                    end
                end

                STATE_TX_STATUS: begin
                    if (sd_clk_rising) begin
                        if (counter < 11'd5) begin
                            counter <= counter + 1'd1;
                        end
                        if ((counter == 11'd1) && (sd_dat_in[0] != 1'b0)) begin
                            sd_scb.dat_error <= 1'b1;
                        end
                        if ((counter == 11'd2) && (sd_dat_in[0] != 1'b1)) begin
                            sd_scb.dat_error <= 1'b1;
                        end
                        if ((counter == 11'd3) && (sd_dat_in[0] != 1'b0)) begin
                            sd_scb.dat_error <= 1'b1;
                        end
                        if ((counter == 11'd4) && (sd_dat_in[0] != 1'b1)) begin
                            sd_scb.dat_error <= 1'b1;
                        end
                        if ((counter == 11'd5) && (sd_dat_in[0] == 1'b1)) begin
                            blocks_remaining <= blocks_remaining - 1'd1;
                        end
                    end
                end
            endcase
        end

        if (tx_rdata_valid) begin
            data_shift <= tx_rdata;
        end
    end

endmodule
