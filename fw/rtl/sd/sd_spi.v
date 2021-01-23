module sd_spi (
    input i_clk,
    input i_reset,

    output o_sd_clk,
    output o_sd_cs,
    output o_sd_mosi,
    input i_sd_miso,

    input i_request,
    input i_write,
    output o_busy,
    output reg o_ack,
    input [8:0] i_address,
    output [31:0] o_data,
    input [31:0] i_data
);

    localparam [0:0] REG_SD_SCR     = 1'd0;
    localparam [0:0] REG_SD_CLK_DIV = 1'd1;

    localparam [8:0] MEM_SD_BUFFER_BASE = 9'h100;
    localparam [8:0] MEM_SD_BUFFER_END  = 9'h1FF;

    wire w_i_address_in_sd_buffer = (i_address >= MEM_SD_BUFFER_BASE) && (i_address <= MEM_SD_BUFFER_END);

    wire w_sd_buffer_wren_b = w_i_address_in_sd_buffer && i_request && i_write && !o_busy;
    wire [31:0] w_sd_buffer_o_data;

    assign o_data = w_i_address_in_sd_buffer ? w_sd_buffer_o_data : 32'd0;

    ram_sd_buffer ram_sd_buffer_inst (
        .clock(i_clk),

        // .address_a(),
        // .data_a(),
        // .wren_a(),
        // .q_a(),

        .address_b(i_address[7:0]),
        .data_b(i_data),
        .wren_b(w_sd_buffer_wren_b),
        .q_b(w_sd_buffer_o_data)
    );


    // Bus controller

    assign o_busy = 1'b0;

    always @(posedge i_clk) begin
        if (i_reset) begin
            o_ack <= 1'b0;
        end else begin
            o_ack <= i_request && !i_write && !o_busy;
        end
    end


    // Write logic

    always @(posedge i_clk) begin
        if (i_reset) begin
        end else begin
            if (i_request && !i_write && !o_busy && !w_i_address_in_sd_buffer) begin
                case (i_address[0])
                    REG_SD_SCR: begin end
                    REG_SD_CLK_DIV: begin end
                    default: begin end
                endcase
            end
        end
    end


    // Read logic

    always @(posedge i_clk) begin
        // o_data <= 32'h0000_0000;

        // if (i_request && !i_write && !o_busy) begin
        //     if (w_i_address_in_sd_buffer) begin
        //         o_data <= w_sd_buffer_o_data;
        //     end
        // end
    end

endmodule
