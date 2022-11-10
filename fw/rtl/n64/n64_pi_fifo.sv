module n64_pi_fifo (
    input clk,
    input reset,

    input flush,

    output full,
    input write,
    input [15:0] wdata,

    output empty,
    input read,
    output [15:0] rdata
);

    logic [15:0] fifo_mem [0:3];
    logic [2:0] fifo_wr_ptr;
    logic [2:0] fifo_rd_ptr;

    logic empty_or_full;

    assign rdata = fifo_mem[fifo_rd_ptr[1:0]];
    assign empty_or_full = fifo_wr_ptr[1:0] == fifo_rd_ptr[1:0];
    assign empty = empty_or_full && fifo_wr_ptr[2] == fifo_rd_ptr[2];
    assign full = empty_or_full && fifo_wr_ptr[2] != fifo_rd_ptr[2];

    always_ff @(posedge clk) begin
        if (reset || flush) begin
            fifo_wr_ptr <= 3'd0;
            fifo_rd_ptr <= 3'd0;
        end else begin
            if (write) begin
                fifo_mem[fifo_wr_ptr[1:0]] <= wdata;
                fifo_wr_ptr <= fifo_wr_ptr + 1'd1;
            end
            if (read) begin
                fifo_rd_ptr <= fifo_rd_ptr + 1'd1;
            end
        end
    end

endmodule
