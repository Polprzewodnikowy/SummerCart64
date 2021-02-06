module sd_fifo (
    input i_clk,
    input i_reset,

    input i_fifo_flush,
    input i_fifo_push,
    input i_fifo_pop,
    output o_fifo_empty,
    output o_fifo_full,
    output [7:0] o_fifo_items,
    output reg o_fifo_underrun,
    output reg o_fifo_overrun,
    input [31:0] i_fifo_data,
    output [31:0] o_fifo_data
);

    reg [31:0] r_fifo_mem [0:127];

    reg [7:0] r_fifo_wrptr;
    reg [7:0] r_fifo_rdptr;

    assign o_fifo_data = r_fifo_mem[r_fifo_rdptr[6:0]];

    wire w_empty = r_fifo_wrptr[7] == r_fifo_rdptr[7];
    wire w_full_or_empty = r_fifo_wrptr[6:0] == r_fifo_rdptr[6:0];

    assign o_fifo_empty = w_empty && w_full_or_empty;
    assign o_fifo_full = !w_empty && w_full_or_empty;
    assign o_fifo_items = r_fifo_wrptr - r_fifo_rdptr;

    always @(posedge i_clk) begin
        if (i_reset) begin
            r_fifo_wrptr <= 8'd0;
            r_fifo_rdptr <= 8'd0;
            o_fifo_underrun <= 1'b0;
            o_fifo_overrun <= 1'b0;
        end else begin
            if (i_fifo_flush) begin
                r_fifo_wrptr <= 8'd0;
                r_fifo_rdptr <= 8'd0;
                o_fifo_underrun <= 1'b0;
                o_fifo_overrun <= 1'b0;
            end
            if (i_fifo_push) begin
                o_fifo_overrun <= o_fifo_overrun ? 1'b1 : o_fifo_full;
                r_fifo_mem[r_fifo_wrptr[6:0]] <= i_fifo_data;
                r_fifo_wrptr <= r_fifo_wrptr + 1'd1;
            end
            if (i_fifo_pop) begin
                o_fifo_underrun <= o_fifo_underrun ? 1'b1 : o_fifo_empty;
                r_fifo_rdptr <= r_fifo_rdptr + 1'd1;
            end
        end
    end

endmodule
