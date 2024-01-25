module n64_si (
    input clk,
    input reset,

    n64_scb.si n64_scb,

    input n64_reset,
    input n64_si_clk,
    inout n64_si_dq
);

    // Input/output synchronization

    logic [1:0] n64_reset_ff;
    logic [1:0] n64_si_clk_ff;

    always_ff @(posedge clk) begin
        n64_reset_ff <= {n64_reset_ff[0], n64_reset};
        n64_si_clk_ff <= {n64_si_clk_ff[0], n64_si_clk};
    end

    logic si_reset;
    logic si_clk;

    always_comb begin
        si_reset = n64_reset_ff[1];
        si_clk = n64_si_clk_ff[1];
    end

    logic si_dq_oe;
    logic si_dq_out;
    logic si_dq_in;

    assign n64_si_dq = si_dq_oe ? 1'b0 : 1'bZ;

    always_ff @(posedge clk) begin
        si_dq_oe <= ~si_dq_out;
        si_dq_in <= n64_si_dq;
    end


    // Clock falling/rising event generator

    logic last_si_clk;

    always_ff @(posedge clk) begin
        last_si_clk <= si_clk;
    end

    logic si_clk_falling_edge;
    logic si_clk_rising_edge;

    always_comb begin
        si_clk_falling_edge = si_reset && last_si_clk && !si_clk;
        si_clk_rising_edge = si_reset && !last_si_clk && si_clk;
    end


    // Data falling/rising event generator

    logic last_si_dq_in;
    logic si_dq_in_inhibit;

    always_ff @(posedge clk) begin
        if (si_clk_rising_edge) begin
            last_si_dq_in <= si_dq_in;
        end
    end

    logic si_dq_falling_edge;
    logic si_dq_rising_edge;

    always_comb begin
        si_dq_falling_edge = si_clk_rising_edge && last_si_dq_in && !si_dq_in && !si_dq_in_inhibit;
        si_dq_rising_edge = si_clk_rising_edge && !last_si_dq_in && si_dq_in && !si_dq_in_inhibit;
    end


    // RX bit generator

    logic [4:0] rx_sub_bit_counter;
    logic rx_timeout;
    logic rx_bit_valid;
    logic rx_bit_data;

    always_ff @(posedge clk) begin
        if (si_clk_rising_edge && !(&rx_sub_bit_counter)) begin
            rx_sub_bit_counter <= rx_sub_bit_counter + 1'd1;
        end
        if (si_dq_falling_edge) begin
            rx_sub_bit_counter <= 4'd0;
        end
    end

    always_comb begin
        rx_timeout = si_clk_rising_edge && si_dq_in && (&rx_sub_bit_counter);
        rx_bit_valid = si_dq_rising_edge;
        rx_bit_data = (rx_sub_bit_counter >= 5'd4) ? 1'b0 : 1'b1;
    end


    // RX byte generator

    logic [2:0] rx_bit_counter;
    logic rx_byte_valid;
    logic [7:0] rx_byte_data;

    always_ff @(posedge clk) begin
        rx_byte_valid <= 1'b0;
        if (rx_timeout) begin
            rx_bit_counter <= 3'd0;
        end
        if (rx_bit_valid) begin
            rx_bit_counter <= rx_bit_counter + 1'd1;
            rx_byte_data <= {rx_byte_data[6:0], rx_bit_data};
            if (&rx_bit_counter) begin
                rx_byte_valid <= 1'b1;
            end
        end
    end


    // RX stop generator

    logic rx_stop;

    always_comb begin
        rx_stop = si_clk_rising_edge && si_dq_in && (rx_sub_bit_counter == 5'd15) && (rx_bit_counter == 3'd1);
    end


    // TX byte/stop generator

    logic tx_busy;
    logic [2:0] tx_sub_bit_counter;
    logic [2:0] tx_bit_counter;
    logic [7:0] tx_shift;
    logic tx_start;
    logic tx_stop;
    logic tx_byte_valid;
    logic [7:0] tx_byte_data;

    always_ff @(posedge clk) begin
        if (reset) begin
            si_dq_out <= 1'b1;
            tx_busy <= 1'b0;
        end else begin
            if (tx_busy) begin
                if (si_clk_falling_edge) begin
                    tx_sub_bit_counter <= tx_sub_bit_counter + 1'd1;
                    if (&tx_sub_bit_counter) begin
                        tx_bit_counter <= tx_bit_counter + 1'd1;
                        tx_shift <= {tx_shift[6:0], 1'bX};
                        if (&tx_bit_counter) begin
                            tx_busy <= 1'b0;
                        end
                    end
                    if (tx_shift[7]) begin
                        si_dq_out <= !(tx_sub_bit_counter < 3'd2);
                    end else begin
                        si_dq_out <= !(tx_sub_bit_counter < 3'd6);
                    end
                end
            end else begin
                if (tx_byte_valid) begin
                    tx_busy <= 1'b1;
                    tx_sub_bit_counter <= 3'd0;
                    tx_bit_counter <= 3'd0;
                    tx_shift <= tx_byte_data;
                end else if (tx_stop) begin
                    tx_busy <= 1'b1;
                    tx_sub_bit_counter <= 3'd0;
                    tx_bit_counter <= 3'd7;
                    tx_shift <= 8'hFF;
                end
            end
        end
    end


    // Joybus CMDs

    typedef enum bit [7:0] {
        CMD_EEPROM_STATUS    = 8'h00,
        CMD_EEPROM_READ      = 8'h04,
        CMD_EEPROM_WRITE     = 8'h05,
        CMD_RTC_STATUS       = 8'h06,
        CMD_RTC_READ         = 8'h07,
        CMD_RTC_WRITE        = 8'h08
    } e_cmd;

    e_cmd cmd;


    // RX path

    typedef enum bit [1:0] {
        RX_STATE_IDLE,
        RX_STATE_DATA,
        RX_STATE_IGNORE
    } e_rx_state;

    e_rx_state rx_state;
    logic [3:0] rx_byte_counter;
    logic rx_data_valid;

    always_comb begin
        rx_data_valid = rx_byte_valid && (rx_state == RX_STATE_DATA);
    end

    always_ff @(posedge clk) begin
        tx_start <= 1'b0;

        if (rx_byte_valid) begin
            rx_byte_counter <= rx_byte_counter + 1'd1;
        end

        if (reset || rx_timeout) begin
            rx_state <= RX_STATE_IDLE;
        end else begin
            case (rx_state)
                RX_STATE_IDLE: begin
                    if (rx_byte_valid) begin
                        cmd <= e_cmd'(rx_byte_data);
                        rx_byte_counter <= 4'd0;
                        rx_state <= RX_STATE_IGNORE;
                        case (rx_byte_data)
                            CMD_EEPROM_STATUS,
                            CMD_EEPROM_READ,
                            CMD_EEPROM_WRITE: begin
                                rx_state <= n64_scb.eeprom_enabled ? RX_STATE_DATA : RX_STATE_IGNORE;
                            end
                            CMD_RTC_STATUS,
                            CMD_RTC_READ,
                            CMD_RTC_WRITE: begin
                                rx_state <= RX_STATE_DATA;
                            end
                        endcase
                    end
                end

                RX_STATE_DATA: begin
                    if (rx_stop) begin
                        tx_start <= 1'b1;
                        rx_state <= RX_STATE_IGNORE;
                    end
                end

                RX_STATE_IGNORE: begin
                    if (rx_stop) begin
                        rx_state <= RX_STATE_IDLE;
                    end
                end
            endcase
        end
    end


    // TX path

    typedef enum bit [1:0] {
        TX_STATE_IDLE,
        TX_STATE_DATA,
        TX_STATE_STOP,
        TX_STATE_STOP_WAIT
    } e_tx_state;

    e_tx_state tx_state;

    logic [3:0] tx_byte_counter;
    logic [3:0] tx_length;

    always_ff @(posedge clk) begin
        tx_byte_valid <= 1'b0;
        tx_stop <= 1'b0;

        if (!tx_busy && tx_byte_valid) begin
            tx_byte_counter <= tx_byte_counter + 1'd1;
        end

        if (reset) begin
            tx_state <= TX_STATE_IDLE;
            si_dq_in_inhibit <= 1'b0;
        end else begin
            case (tx_state)
                TX_STATE_IDLE: begin
                    if (tx_start) begin
                        tx_byte_counter <= 4'd0;
                        tx_state <= TX_STATE_DATA;
                        si_dq_in_inhibit <= 1'b1;
                    end
                end

                TX_STATE_DATA: begin
                    tx_byte_valid <= 1'b1;
                    if (!tx_busy && tx_byte_valid) begin
                        if (tx_byte_counter == tx_length) begin
                            tx_state <= TX_STATE_STOP;
                        end
                    end
                end

                TX_STATE_STOP: begin
                    tx_stop <= 1'b1;
                    if (!tx_busy && tx_stop) begin
                        tx_state <= TX_STATE_STOP_WAIT;
                    end
                end

                TX_STATE_STOP_WAIT: begin
                    if (!tx_busy) begin
                        tx_state <= TX_STATE_IDLE;
                        si_dq_in_inhibit <= 1'b0;
                    end
                end
            endcase
        end
    end


    // Joybus address latching

    logic [7:0] joybus_address;
    logic [2:0] joybus_subaddress;
    logic [10:0] joybus_full_address;

    always_comb begin
        joybus_full_address = {joybus_address, joybus_subaddress};
    end

    always_ff @(posedge clk) begin
        if (rx_data_valid || (!tx_busy && tx_byte_valid)) begin
            joybus_subaddress <= joybus_subaddress + 1'd1;
        end
        if (rx_data_valid) begin
            if (rx_byte_counter == 4'd0) begin
                joybus_address <= rx_byte_data;
                joybus_subaddress <= 3'd0;
            end
        end
    end


    // EEPROM controller

    always_comb begin
        n64_scb.eeprom_write = rx_data_valid && (cmd == CMD_EEPROM_WRITE) && rx_byte_counter > 4'd0;
        n64_scb.eeprom_address = joybus_full_address;
        n64_scb.eeprom_wdata = rx_byte_data;
    end


    // RTC controller

    logic rtc_backup_wp;
    logic rtc_time_wp;
    logic [1:0] rtc_stopped;
    logic [6:0] rtc_time_second;
    logic [6:0] rtc_time_minute;
    logic [5:0] rtc_time_hour;
    logic [5:0] rtc_time_day;
    logic [2:0] rtc_time_weekday;
    logic [4:0] rtc_time_month;
    logic [7:0] rtc_time_year;

    always_ff @(posedge clk) begin
        if (reset) begin
            rtc_backup_wp <= 1'b1;
            rtc_time_wp <= 1'b1;
            rtc_stopped <= 2'b00;
            n64_scb.rtc_pending <= 1'b0;
        end

        if (n64_scb.rtc_done) begin
            n64_scb.rtc_pending <= 1'b0;
        end

        if (!(|rtc_stopped) && !n64_scb.rtc_pending && n64_scb.rtc_wdata_valid && (tx_state != TX_STATE_DATA)) begin
            {
                rtc_time_year,
                rtc_time_month,
                rtc_time_weekday,
                rtc_time_day,
                rtc_time_hour,
                rtc_time_minute,
                rtc_time_second
            } <= n64_scb.rtc_wdata;
        end

        if (rx_data_valid && (cmd == CMD_RTC_WRITE)) begin
            if (joybus_address[1:0] == 2'd0) begin
                case (rx_byte_counter)
                    4'd1: {rtc_time_wp, rtc_backup_wp} <= rx_byte_data[1:0];
                    4'd2: begin
                        rtc_stopped <= rx_byte_data[2:1];
                        if ((|rtc_stopped) && (rx_byte_data[2:1] == 2'b00)) begin
                            n64_scb.rtc_pending <= 1'b1;
                        end
                    end
                endcase
            end
            if ((joybus_address[1:0] == 2'd2) && !rtc_time_wp) begin
                case (rx_byte_counter)
                    4'd1: rtc_time_second <= rx_byte_data[6:0];
                    4'd2: rtc_time_minute <= rx_byte_data[6:0];
                    4'd3: rtc_time_hour <= rx_byte_data[5:0];
                    4'd4: rtc_time_day <= rx_byte_data[5:0];
                    4'd5: rtc_time_weekday <= rx_byte_data[2:0];
                    4'd6: rtc_time_month <= rx_byte_data[4:0];
                    4'd7: rtc_time_year <= rx_byte_data;
                endcase
            end
        end
    end

    always_comb begin
        n64_scb.rtc_rdata = {
            rtc_time_year,
            rtc_time_month,
            rtc_time_weekday,
            rtc_time_day,
            rtc_time_hour,
            rtc_time_minute,
            rtc_time_second
        };
    end


    // TX data multiplexer

    always_comb begin
        tx_length = 4'd0;
        tx_byte_data = 8'h00;
        case (cmd)
            CMD_EEPROM_STATUS: begin
                tx_length = 4'd2;
                case (tx_byte_counter)
                    4'd1: tx_byte_data = {1'b1, n64_scb.eeprom_16k_mode, 6'd0};
                endcase
            end
            CMD_EEPROM_READ: begin
                tx_length = 4'd7;
                tx_byte_data = n64_scb.eeprom_rdata;
            end
            CMD_EEPROM_WRITE: begin
                tx_length = 4'd0;
            end
            CMD_RTC_STATUS: begin
                tx_length = 4'd2;
                case (tx_byte_counter)
                    4'd1: tx_byte_data = 8'h10;
                    4'd2: tx_byte_data = {(|rtc_stopped), 7'd0};
                endcase
            end
            CMD_RTC_READ: begin
                tx_length = 4'd8;
                if (joybus_address[1:0] == 2'd0) begin
                    case (tx_byte_counter)
                        4'd0: tx_byte_data = {6'd0, rtc_time_wp, rtc_backup_wp};
                        4'd1: tx_byte_data = {5'd0, rtc_stopped, 1'b0};
                        4'd8: tx_byte_data = {(|rtc_stopped), 7'd0};
                    endcase
                end else if (joybus_address[1:0] == 2'd2) begin
                    case (tx_byte_counter)
                        4'd0: tx_byte_data = {1'd0, rtc_time_second};
                        4'd1: tx_byte_data = {1'd0, rtc_time_minute};
                        4'd2: tx_byte_data = {2'b10, rtc_time_hour};
                        4'd3: tx_byte_data = {2'd0, rtc_time_day};
                        4'd4: tx_byte_data = {5'd0, rtc_time_weekday};
                        4'd5: tx_byte_data = {3'd0, rtc_time_month};
                        4'd6: tx_byte_data = rtc_time_year;
                        4'd7: tx_byte_data = 8'h01;
                        4'd8: tx_byte_data = {(|rtc_stopped), 7'd0};
                    endcase
                end
            end
            CMD_RTC_WRITE: begin
                tx_length = 4'd0;
                tx_byte_data = {(|rtc_stopped), 7'd0};
            end
        endcase
    end

endmodule
