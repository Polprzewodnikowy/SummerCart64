module n64_si (
    input i_clk,
    input i_reset,

    input i_n64_reset,
    input i_n64_si_clk,
    inout io_n64_si_dq,

    input i_request,
    input i_write,
    output o_busy,
    output reg o_ack,
    input [8:0] i_address,
    input [31:0] i_data,
    output [31:0] o_data,

    input i_eeprom_enable,
    input i_eeprom_16k_mode
);

    // Input synchronization

    reg r_reset_ff1, r_reset_ff2;
    reg r_si_clk_ff1, r_si_clk_ff2;
    reg r_si_dq_ff1, r_si_dq_ff2;

    always @(posedge i_clk) begin
        {r_reset_ff2, r_reset_ff1} <= {r_reset_ff1, i_n64_reset};
        {r_si_clk_ff2, r_si_clk_ff1} <= {r_si_clk_ff1, i_n64_si_clk};
        {r_si_dq_ff2, r_si_dq_ff1} <= {r_si_dq_ff1, io_n64_si_dq};
    end

    reg r_si_dq_o;

    assign io_n64_si_dq = r_si_dq_o ? 1'bZ : 1'b0;


    // SI commands

    localparam CMD_EEPROM_STATUS    = 8'h00;
    localparam CMD_EEPROM_READ      = 8'h04;
    localparam CMD_EEPROM_WRITE     = 8'h05;

    localparam EEPROM_4K_ID         = 8'h80;
    localparam EEPROM_16K_ID        = 8'hC0;


    // Event signal generation

    reg r_last_si_clk;
    reg r_last_si_dq;

    wire w_si_clk_falling_edge = !i_reset && r_reset_ff2 && r_last_si_clk && !r_si_clk_ff2;
    wire w_si_clk_rising_edge = !i_reset && r_reset_ff2 && !r_last_si_clk && r_si_clk_ff2;
    wire w_si_dq_falling_edge = r_last_si_dq && !r_si_dq_ff2;
    wire w_si_dq_rising_edge = !r_last_si_dq && r_si_dq_ff2;

    always @(posedge i_clk) begin
        r_last_si_clk <= r_si_clk_ff2;
        if (w_si_clk_rising_edge) r_last_si_dq <= r_si_dq_ff2;
    end


    // RX module

    reg r_rx_enabled;
    reg [2:0] r_rx_sub_bit_counter;
    reg [2:0] r_rx_bit_counter;
    reg [3:0] r_rx_byte_counter;
    reg [7:0] r_rx_buffer;
    reg r_rx_byte_ready;
    reg r_rx_finished, r_last_rx_finished;

    wire w_rx_sub_bit_counter_timeout = &r_rx_sub_bit_counter;
    wire w_rx_bit_value = r_rx_sub_bit_counter <= 3'd4;
    wire w_rx_start = r_last_rx_finished && !r_rx_finished;
    wire w_rx_finish = !r_last_rx_finished && r_rx_finished;

    always @(posedge i_clk) begin
        r_rx_byte_ready <= 1'b0;
        r_last_rx_finished <= r_rx_finished;

        if (r_rx_enabled && w_si_clk_rising_edge) begin
            if (w_rx_sub_bit_counter_timeout) r_rx_finished <= 1'b1;
            else r_rx_sub_bit_counter <= r_rx_sub_bit_counter + 3'd1;

            if (w_si_dq_falling_edge) begin
                r_rx_sub_bit_counter <= 3'd0;
                if (r_rx_finished) begin
                    r_rx_bit_counter <= 3'd0;
                    r_rx_byte_counter <= 4'b1111;
                end
                r_rx_finished <= 1'b0;
            end

            if (w_si_dq_rising_edge) begin
                r_rx_bit_counter <= r_rx_bit_counter + 3'd1;
                r_rx_buffer <= {r_rx_buffer[6:0], w_rx_bit_value};
                if (&r_rx_bit_counter) begin
                    r_rx_byte_counter <= r_rx_byte_counter + 4'd1;
                    r_rx_byte_ready <= 1'b1;
                end
            end
        end
    end

    reg r_tx_finished;

    reg r_cmd_eeprom_status;
    reg r_cmd_eeprom_read;
    reg r_cmd_eeprom_write;

    wire w_cmd_valid = r_cmd_eeprom_status || r_cmd_eeprom_read || r_cmd_eeprom_write;
    wire w_cmd_op = r_rx_byte_ready && r_rx_byte_counter == 4'd0;

    always @(posedge i_clk) begin
        if (i_reset || !r_reset_ff2 || w_rx_start || r_tx_finished) begin
            r_cmd_eeprom_status <= 1'b0;
            r_cmd_eeprom_read <= 1'b0;
            r_cmd_eeprom_write <= 1'b0;
        end else if (w_cmd_op) begin
            if (i_eeprom_enable) begin
                r_cmd_eeprom_status <= r_rx_buffer == CMD_EEPROM_STATUS;
                r_cmd_eeprom_read <= r_rx_buffer == CMD_EEPROM_READ;
                r_cmd_eeprom_write <= r_rx_buffer == CMD_EEPROM_WRITE;
            end
        end
    end

    reg r_eeprom_read_rq;
    reg [10:0] r_eeprom_address;

    wire w_eeprom_write_op = r_rx_byte_ready && r_rx_byte_counter >= 4'd2 && r_cmd_eeprom_write;
    wire w_eeprom_address_op = r_rx_byte_ready && r_rx_byte_counter == 4'd1 && (r_cmd_eeprom_read || r_cmd_eeprom_write);
    wire w_eeprom_address_next_op = r_eeprom_read_rq || w_eeprom_write_op;

    always @(posedge i_clk) begin
        if (w_eeprom_address_op) r_eeprom_address <= {r_rx_buffer, 3'b000};
        if (w_eeprom_address_next_op) r_eeprom_address[2:0] <= r_eeprom_address[2:0] + 3'd1;
    end


    // TX module

    reg [2:0] r_tx_sub_bit_counter;
    reg [2:0] r_tx_bit_counter;
    reg [3:0] r_tx_byte_counter;
    reg [3:0] r_tx_bytes_to_send;
    reg [7:0] r_tx_data;

    wire [7:0] w_eeprom_o_data;

    wire w_tx_current_bit = r_tx_data[3'd7 - r_tx_bit_counter];
    wire w_tx_stop_bit = r_tx_byte_counter == r_tx_bytes_to_send;

    always @(*) begin
        r_tx_data = 8'h00;
        if (r_cmd_eeprom_status && r_tx_byte_counter == 4'd1) r_tx_data = i_eeprom_16k_mode ? EEPROM_16K_ID : EEPROM_4K_ID;
        if (r_cmd_eeprom_read) r_tx_data = w_eeprom_o_data;
    end

    always @(posedge i_clk) begin
        if (i_reset || !r_reset_ff2) begin
            r_si_dq_o <= 1'b1;
            r_rx_enabled <= 1'b1;
            r_tx_finished <= 1'b0;
            r_eeprom_read_rq <= 1'b0;
        end else begin
            r_tx_finished <= 1'b0;
            r_eeprom_read_rq <= 1'b0;

            if (w_rx_finish && w_cmd_valid) begin
                r_rx_enabled <= 1'b0;
                r_tx_sub_bit_counter <= 3'd0;
                r_tx_bit_counter <= 3'd0;
                r_tx_byte_counter <= 4'd0;
                r_tx_bytes_to_send <= 4'd0;
                if (r_cmd_eeprom_status) r_tx_bytes_to_send <= 4'd3;
                if (r_cmd_eeprom_read) r_tx_bytes_to_send <= 4'd8;
                if (r_cmd_eeprom_write) r_tx_bytes_to_send <= 4'd1;
            end

            if (!r_rx_enabled) begin
                if (w_si_clk_falling_edge) begin
                    r_tx_sub_bit_counter <= r_tx_sub_bit_counter + 3'd1;

                    if (r_tx_sub_bit_counter == 3'd0) r_si_dq_o <= 1'b0;

                    if ((w_tx_current_bit && r_tx_sub_bit_counter == 3'd2) ||
                        (!w_tx_current_bit && r_tx_sub_bit_counter == 3'd6) ||
                        (w_tx_stop_bit && r_tx_sub_bit_counter == 3'd4)) begin
                        r_si_dq_o <= 1'b1;
                    end

                    if (&r_tx_sub_bit_counter) begin
                        if (w_tx_stop_bit) begin
                            r_rx_enabled <= 1'b1;
                            r_tx_finished <= 1'b1;
                        end
                        r_tx_bit_counter <= r_tx_bit_counter + 3'd1;
                        if (&r_tx_bit_counter) begin
                            r_eeprom_read_rq <= 1'b1;
                            r_tx_byte_counter <= r_tx_byte_counter + 4'd1;
                        end
                    end
                end
            end
        end
    end


    // Block RAM

    ram_n64_eeprom ram_n64_eeprom_inst (
        .clock(i_clk),

        .address_a(r_eeprom_address),
        .data_a(r_rx_buffer),
        .wren_a(w_eeprom_write_op),
        .q_a(w_eeprom_o_data),

        .address_b(i_address),
        .data_b({i_data[7:0], i_data[15:8], i_data[23:16], i_data[31:24]}),
        .wren_b(!i_reset && i_request && i_write),
        .q_b({o_data[7:0], o_data[15:8], o_data[23:16], o_data[31:24]})
    );


    // Bus logic

    assign o_busy = 1'b0;

    always @(posedge i_clk) begin
        o_ack <= !i_reset && i_request && !i_write;
    end

endmodule
