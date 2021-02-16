module sd_fifo (
    input i_clk,
    input i_reset,

    input i_fifo_flush,
    input i_fifo_push,
    input i_fifo_pop,
    output o_fifo_empty,
    output o_fifo_full,
    output reg o_fifo_underrun,
    output reg o_fifo_overrun,
    output [8:0] o_fifo_items,
    input [31:0] i_fifo_data,
    output [31:0] o_fifo_data
);

    wire [7:0] w_fifo_items;

    assign o_fifo_items = {o_fifo_full, w_fifo_items};

    fifo_sd fifo_sd_inst (
        .clock(i_clk),
        .sclr(i_reset || i_fifo_flush),
        .wrreq(i_fifo_push),
        .rdreq(i_fifo_pop),
        .empty(o_fifo_empty),
        .full(o_fifo_full),
        .usedw(w_fifo_items),
        .data(i_fifo_data),
        .q(o_fifo_data)
    );

    always @(posedge i_clk) begin
        if (i_reset || i_fifo_flush) begin
            o_fifo_underrun <= 1'b0;
            o_fifo_overrun <= 1'b0;
        end else begin
            if (o_fifo_empty && i_fifo_pop) begin
                o_fifo_underrun <= 1'b1;
            end
            if (o_fifo_full && i_fifo_push) begin
                o_fifo_overrun <= 1'b1;
            end
        end
    end

endmodule
