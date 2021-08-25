module n64_pi_fifo (
    if_system.sys sys,

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

    always_comb begin
        rdata = fifo_mem[fifo_rd_ptr[1:0]];
        empty_or_full = fifo_wr_ptr[1:0] == fifo_rd_ptr[1:0];
        empty = empty_or_full && fifo_wr_ptr[2] == fifo_rd_ptr[2];
        full = empty_or_full && fifo_wr_ptr[2] != fifo_rd_ptr[2];
    end

    always_ff @(posedge sys.clk) begin
        if (sys.reset || flush) begin
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
