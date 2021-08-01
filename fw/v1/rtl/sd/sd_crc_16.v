module sd_crc_16 (
    input i_clk,
    input i_crc_reset,
    input i_crc_shift,
    input i_crc_input,
    output reg [15:0] o_crc_output
);

    wire w_crc_inv = o_crc_output[15] ^ i_crc_input;

    always @(posedge i_clk) begin
        if (i_crc_reset) begin
            o_crc_output <= 16'd0;
        end else if (i_crc_shift) begin
            o_crc_output <= {
                o_crc_output[14:12],
                o_crc_output[11] ^ w_crc_inv,
                o_crc_output[10:5],
                o_crc_output[4] ^ w_crc_inv,
                o_crc_output[3:0],
                w_crc_inv
            };
        end
    end

endmodule
