module sd_crc_7 (
    input clk,
    input reset,

    input enable,
    input data,

    output logic [6:0] result
);

    logic crc_inv;

    assign crc_inv = result[6] ^ data;

    always_ff @(posedge clk) begin
        if (reset) begin
            result <= 7'd0;
        end else if (enable) begin
            result <= {
                result[5:3],
                result[2] ^ crc_inv,
                result[1:0],
                crc_inv
            };
        end
    end

endmodule
