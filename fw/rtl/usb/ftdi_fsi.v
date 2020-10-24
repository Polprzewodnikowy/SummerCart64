module ftdi_fsi (
    input i_clk,
    input i_reset,

    output reg o_ftdi_clk,
    output reg o_ftdi_si,
    input i_ftdi_so,
    input i_ftdi_cts,

    input i_rx_ready,
    output reg o_rx_valid,
    output reg o_rx_channel,
    output reg [7:0] o_rx_data,

    output reg o_tx_busy,
    input i_tx_valid,
    input i_tx_channel,
    input [7:0] i_tx_data
);

    always @(posedge i_clk) begin
        if (i_reset || !i_rx_ready) begin
            o_ftdi_clk <= 1'b1;
        end else begin
            o_ftdi_clk <= ~o_ftdi_clk;
        end
    end

    reg r_rx_in_progress;
    reg [3:0] r_rx_bit_counter;
    reg r_tx_start_bit;
    reg r_rx_tx_contention;

    always @(posedge i_clk) begin
        o_rx_valid <= 1'b0;

        if (i_reset) begin
            r_rx_in_progress <= 1'b0;
        end else begin
            if (!o_ftdi_clk) begin
                if (!r_rx_in_progress) begin
                    r_rx_in_progress <= !i_ftdi_so;
                    r_rx_bit_counter <= 4'd0;
                    r_rx_tx_contention <= r_tx_start_bit;
                end else begin
                    r_rx_bit_counter <= r_rx_bit_counter + 4'd1;

                    if (!r_rx_bit_counter[3]) begin
                        o_rx_data <= {i_ftdi_so, o_rx_data[7:1]};
                    end else begin
                        r_rx_in_progress <= 1'b0;
                        o_rx_valid <= !r_rx_tx_contention;
                        o_rx_channel <= i_ftdi_so;
                    end
                end
            end
        end
    end

    reg r_tx_pending;
    reg [3:0] r_tx_bit_counter;
    reg [7:0] r_tx_data;
    reg r_tx_channel;

    wire w_tx_request_op = i_tx_valid && !o_tx_busy;
    wire w_tx_pending_op = !o_ftdi_clk || !i_ftdi_cts || !i_rx_ready || r_rx_in_progress;
    wire w_tx_reset_output_op = o_ftdi_clk && !o_tx_busy;
    wire w_tx_start_op = (w_tx_request_op || r_tx_pending) && !w_tx_pending_op;
    wire w_tx_shift_op = o_ftdi_clk && o_tx_busy && !r_tx_pending;

    always @(posedge i_clk) begin
        r_tx_start_bit <= 1'b0;

        if (i_reset) begin
            o_ftdi_si <= 1'b1;
            o_tx_busy <= 1'b0;
            r_tx_pending <= 1'b0;
        end else begin
            if (w_tx_request_op) begin
                o_tx_busy <= 1'b1;
                r_tx_data <= i_tx_data;
                r_tx_channel <= i_tx_channel;
                r_tx_pending <= w_tx_pending_op;
            end

            if (w_tx_reset_output_op) begin
                o_ftdi_si <= 1'b1;
            end

            if (w_tx_start_op) begin
                o_ftdi_si <= 1'b0;
                r_tx_start_bit <= 1'b1;
                r_tx_pending <= 1'b0;
                r_tx_bit_counter <= 4'd0;
            end

            if (w_tx_shift_op) begin
                r_tx_bit_counter <= r_tx_bit_counter + 4'd1;
                {r_tx_data[6:0], o_ftdi_si} <= r_tx_data;
                if (r_tx_bit_counter[3]) begin
                    o_ftdi_si <= r_tx_channel;
                    o_tx_busy <= 1'b0;
                end
            end
        end
    end

endmodule
