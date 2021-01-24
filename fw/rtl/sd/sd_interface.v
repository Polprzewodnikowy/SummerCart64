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
    input [7:0] i_address,
    output [31:0] o_data,
    input [31:0] i_data
);

    // Register offsets

    localparam [1:0] REG_SD_SCR         = 2'd0;
    localparam [1:0] REG_SD_CS          = 2'd1;
    localparam [1:0] REG_SD_DR          = 2'd2;
    localparam [1:0] REG_SD_MULTI       = 2'd3;

    localparam [7:0] MEM_SD_BUFFER_BASE = 8'h80;


    // Bus controller

    reg [7:0] r_o_data;

    wire w_address_in_buffers = i_address >= MEM_SD_BUFFER_BASE;
    wire [31:0] w_sd_buffer_rx_o_data;

    assign o_busy = 1'b0;
    assign o_data = w_address_in_buffers ? w_sd_buffer_rx_o_data : {24'd0, r_o_data};

    always @(posedge i_clk) begin
        o_ack <= !i_reset && i_request && !i_write && !o_busy;
    end


    // Bus <-> peripheral interface registers

    reg r_spi_busy;

    reg [2:0] r_spi_clk_div;
    reg r_spi_start;
    reg [7:0] r_spi_tx_data;
    reg [7:0] r_spi_rx_data;
    reg r_spi_rx_only;

    reg r_spi_start_multi;
    reg [8:0] r_spi_multi_length;


    // Write logic

    always @(posedge i_clk) begin
        r_spi_start <= 1'b0;
        r_spi_start_multi <= 1'b0;

        if (i_reset) begin
            o_sd_cs <= 1'b1;
            r_spi_clk_div <= 3'b111;
        end else if (i_request && i_write && !o_busy && !w_address_in_buffers) begin
            case (i_address[1:0])
                REG_SD_SCR: begin
                    r_spi_clk_div <= i_data[3:1];
                end
                REG_SD_CS: if (!r_spi_busy) begin
                    o_sd_cs <= i_data[0];
                end
                REG_SD_DR: if (!r_spi_busy) begin
                    r_spi_start <= 1'b1;
                    r_spi_tx_data <= i_data[7:0];
                    r_spi_rx_only <= 1'b0;
                    r_spi_multi_length <= 9'd0;
                end
                REG_SD_MULTI: if (!r_spi_busy) begin
                    r_spi_start_multi <= 1'b1;
                    {r_spi_rx_only, r_spi_multi_length} <= i_data[9:0];
                end
            endcase
        end
    end


    // Read logic

    always @(posedge i_clk) begin
        if (!i_reset && i_request && !i_write && !o_busy) begin
            if (!w_address_in_buffers) begin
                case (i_address[1:0])
                    REG_SD_SCR: begin
                        r_o_data[3:0] <= {r_spi_clk_div, r_spi_busy};
                    end
                    REG_SD_DR: begin
                        r_o_data[7:0] <= r_spi_rx_data;
                    end
                    default: begin end
                endcase
            end
            // else begin
            //     o_data <= w_sd_buffer_rx_o_data;
            // end
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


    // Buffers

    reg [8:0] r_spi_multi_current_byte;
    reg r_spi_multi_byte_write;

    wire [7:0] w_spi_multi_tx_data;

    ram_sd_buffer ram_sd_buffer_tx_inst (
        .clock(i_clk),

        .address_a(r_spi_multi_current_byte),
        .q_a(w_spi_multi_tx_data),

        .wren_b(!i_reset && i_request && i_write && !o_busy && w_address_in_buffers),
        .address_b(i_address[6:0]),
        .data_b({i_data[7:0], i_data[15:8], i_data[23:16], i_data[31:24]})
    );

    ram_sd_buffer ram_sd_buffer_rx_inst (
        .clock(i_clk),

        .wren_a(r_spi_multi_byte_write),
        .address_a(r_spi_multi_current_byte - 1'd1),
        .data_a(r_spi_rx_data),

        .address_b(i_address[6:0]),
        .q_b({w_sd_buffer_rx_o_data[7:0], w_sd_buffer_rx_o_data[15:8], w_sd_buffer_rx_o_data[23:16], w_sd_buffer_rx_o_data[31:24]})
    );


    // Actual SPI stuff

    reg r_spi_multi;
    reg r_spi_first_bit;
    reg [3:0] r_spi_bit_clk;
    reg [6:0] r_spi_tx_shift;

    wire [7:0] w_next_spi_tx_data = r_spi_rx_only ? 8'hFF : (r_spi_multi ? w_spi_multi_tx_data : r_spi_tx_data);

    always @(posedge i_clk) begin
        r_spi_multi_byte_write <= 1'b0;

        if (i_reset) begin
            o_sd_clk <= 1'b0;
            r_spi_bit_clk <= 4'd8;
            r_spi_multi_current_byte <= 9'd0;
        end else begin
            if (!r_spi_busy && (r_spi_start || r_spi_start_multi)) begin
                r_spi_busy <= 1'b1;
                r_spi_multi <= r_spi_start_multi;
                r_spi_first_bit <= 1'b1;
                // r_spi_multi_current_byte <= 9'd0;
            end else if (r_spi_busy && r_spi_clk_strobe) begin
                if (r_spi_first_bit) begin
                    r_spi_first_bit <= 1'b0;
                    {o_sd_mosi, r_spi_tx_shift} <= w_next_spi_tx_data;
                end else if (!o_sd_clk) begin
                    o_sd_clk <= 1'b1;
                    r_spi_rx_data <= {r_spi_rx_data[6:0], i_sd_miso};
                    r_spi_bit_clk <= r_spi_bit_clk - 1'd1;
                    if (r_spi_bit_clk == 4'd1) begin
                        r_spi_multi_current_byte <= r_spi_multi_current_byte + 1'd1;
                        r_spi_multi_byte_write <= 1'b1;
                    end
                end else begin
                    o_sd_clk <= 1'b0;
                    if (r_spi_bit_clk == 4'd0) begin
                        r_spi_bit_clk <= 4'd8;
                        {o_sd_mosi, r_spi_tx_shift} <= w_next_spi_tx_data;
                        if (r_spi_multi_current_byte == 9'(r_spi_multi_length + 1'd1)) begin
                            r_spi_busy <= 1'b0;
                            r_spi_multi_current_byte <= 9'd0;
                        end
                    end else begin
                        {o_sd_mosi, r_spi_tx_shift} <= {r_spi_tx_shift, 1'b0};
                    end
                end
            end
        end
    end

endmodule
