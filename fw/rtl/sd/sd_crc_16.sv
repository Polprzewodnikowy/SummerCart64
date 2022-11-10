module sd_crc_16 (
    input clk,
    input reset,

    input enable,
    input shift,
    input data,

    output logic [15:0] result
);

    logic crc_inv;

    assign crc_inv = result[15] ^ data;

    always_ff @(posedge clk) begin
        if (reset) begin
            result <= 16'd0;
        end else if (enable) begin
            result <= {
                result[14:12],
                result[11] ^ crc_inv,
                result[10:5],
                result[4] ^ crc_inv,
                result[3:0],
                crc_inv
            };
        end else if (shift) begin
            result <= {result[14:0], 1'b1};
        end
    end

endmodule
