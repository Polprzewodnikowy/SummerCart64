module sd_crc_7 (
    input i_clk,
    input i_crc_reset,
    input i_crc_shift,
    input i_crc_input,
    output reg [6:0] o_crc_output
);

    wire w_crc_inv = o_crc_output[6] ^ i_crc_input;

    always @(posedge i_clk) begin
        if (i_crc_reset) begin
            o_crc_output <= 7'd0;
        end else if (i_crc_shift) begin
            o_crc_output <= {
                o_crc_output[5:3],
                o_crc_output[2] ^ w_crc_inv,
                o_crc_output[1:0],
                w_crc_inv
            };
        end
    end

endmodule
