module sd_interface (
    input i_clk,
    input i_reset,

    output reg o_sd_clk,
    output reg o_sd_cs,
    output reg o_sd_mosi,
    input i_sd_miso,

    input i_request,
    input i_write,
    output o_busy,
    output reg o_ack,
    input [1:0] i_address,
    output [31:0] o_data,
    input [31:0] i_data
);

    // Register offsets

    localparam [1:0] REG_SD_SCR = 2'd0;
    localparam [1:0] REG_SD_CS  = 2'd1;
    localparam [1:0] REG_SD_DR  = 2'd2;


    // Bus controller

    assign o_busy = 1'b0;

    always @(posedge i_clk) begin
        o_ack <= !i_reset && i_request && !i_write && !o_busy;
    end


    // Bus <-> peripheral interface registers

    reg [2:0] r_spi_clk_div;
    reg r_spi_start;
    reg r_spi_busy;
    reg [7:0] r_spi_tx_data;
    reg [7:0] r_spi_rx_data;


    // Write logic

    always @(posedge i_clk) begin
        r_spi_start <= 1'b0;

        if (i_reset) begin
            o_sd_cs <= 1'b1;
            r_spi_clk_div <= 3'b111;
        end else if (i_request && i_write && !o_busy) begin
            case (i_address[1:0])
                REG_SD_SCR: begin
                    r_spi_clk_div <= i_data[3:1];
                end
                REG_SD_CS: begin
                    o_sd_cs <= i_data[0];
                end
                REG_SD_DR: begin
                    r_spi_start <= 1'b1;
                    r_spi_tx_data <= i_data[7:0];
                end
                default: begin
                end
            endcase
        end
    end


    // Read logic

    always @(posedge i_clk) begin
        o_data <= 32'h0000_0000;

        if (i_request && !i_write && !o_busy) begin
            case (i_address[1:0])
                REG_SD_SCR: begin
                    o_data[3:0] <= {r_spi_clk_div, r_spi_busy};
                end
                REG_SD_DR: begin
                    o_data[7:0] <= r_spi_rx_data;
                end
                default: begin
                end
            endcase
        end
    end


    // Clock divider

    reg [7:0] r_spi_clk_div_counter;
    reg r_spi_clk_prev_value;
    reg r_spi_clk_strobe;

    wire w_spi_clk_div_selected = r_spi_clk_div_counter[r_spi_clk_div];

    always @(posedge i_clk) begin
        r_spi_clk_div_counter <= r_spi_clk_div_counter + 1'd1;
        r_spi_clk_prev_value <= w_spi_clk_div_selected;
        r_spi_clk_strobe <= w_spi_clk_div_selected != r_spi_clk_prev_value;
    end


    // Actual SPI stuff

    reg r_spi_first_bit;
    reg [3:0] r_spi_bit_clk;
    reg [6:0] r_spi_tx_shift;

    always @(posedge i_clk) begin
        if (i_reset) begin
            o_sd_clk <= 1'b0;
        end else begin
            if (r_spi_start) begin
                r_spi_busy <= 1'b1;
                r_spi_first_bit <= 1'b1;
            end
            if (r_spi_clk_strobe && r_spi_busy) begin
                if (r_spi_first_bit) begin
                    {o_sd_mosi, r_spi_tx_shift} <= r_spi_tx_data;
                    r_spi_first_bit <= 1'b0;
                    r_spi_bit_clk <= 4'd8;
                end else if (!o_sd_clk) begin
                    o_sd_clk <= 1'b1;
                    r_spi_rx_data <= {r_spi_rx_data[6:0], i_sd_miso};
                    r_spi_bit_clk <= r_spi_bit_clk - 1'd1;
                end else begin
                    o_sd_clk <= 1'b0;
                    {o_sd_mosi, r_spi_tx_shift} <= {r_spi_tx_shift, 1'b0};
                    if (r_spi_bit_clk == 4'd0) begin
                        r_spi_busy <= 1'b0;
                    end
                end
            end
        end
    end

endmodule
