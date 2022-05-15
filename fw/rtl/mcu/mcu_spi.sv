module mcu_spi (
    input clk,
    input reset,

    output logic frame_start,
    output logic data_ready,
    output logic [7:0] rx_data,
    input [7:0] tx_data,

    input mcu_clk,
    input mcu_cs,
    input mcu_mosi,
    output logic mcu_miso
);

    logic [2:0] mcu_clk_ff;
    logic [2:0] mcu_cs_ff;

    always_ff @(posedge clk) begin
        mcu_clk_ff <= {mcu_clk_ff[1:0], mcu_clk};
        mcu_cs_ff <= {mcu_cs_ff[1:0], mcu_cs};
    end

    logic mcu_clk_falling;
    logic mcu_clk_rising;
    logic mcu_cs_falling;
    logic mcu_cs_rising;

    always_comb begin
        mcu_clk_falling = mcu_clk_ff[2] && !mcu_clk_ff[1];
        mcu_clk_rising = !mcu_clk_ff[2] && mcu_clk_ff[1];
        mcu_cs_falling = mcu_cs_ff[2] && !mcu_cs_ff[1];
        mcu_cs_rising = !mcu_cs_ff[2] && mcu_cs_ff[1];
    end

    logic mcu_dq_in;
    logic mcu_dq_out;
    logic mcu_miso_out;

    assign mcu_miso = mcu_cs_ff[1] ? 1'bZ : mcu_miso_out;

    always_ff @(posedge clk) begin
        mcu_dq_in <= mcu_mosi;
        mcu_miso_out <= mcu_dq_out;
    end

    logic [7:0] spi_tx_shift;

    assign mcu_dq_out = spi_tx_shift[7];

    logic spi_enabled;
    logic [2:0] spi_bit_counter;

    always_ff @(posedge clk) begin
        frame_start <= 1'b0;
        data_ready <= 1'b0;

        if (reset) begin
            spi_enabled <= 1'b0;
            spi_bit_counter <= 3'd0;
        end else begin
            if (mcu_cs_falling) begin
                spi_enabled <= 1'b1;
                spi_bit_counter <= 3'd0;
                frame_start <= 1'b1;
            end

            if (mcu_cs_rising) begin
                spi_enabled <= 1'b0;
            end

            if (spi_enabled) begin
                if (mcu_clk_rising) begin
                    if (spi_bit_counter == 3'd0) begin
                        spi_tx_shift <= tx_data;
                    end else begin
                        spi_tx_shift <= {spi_tx_shift[6:0], 1'b0};
                    end
                end

                if (mcu_clk_falling) begin
                    spi_bit_counter <= spi_bit_counter + 1'd1;
                    rx_data <= {rx_data[6:0], mcu_dq_in};
                    if (spi_bit_counter == 3'd7) begin
                        data_ready <= 1'b1;
                    end
                end
            end
        end
    end

endmodule
