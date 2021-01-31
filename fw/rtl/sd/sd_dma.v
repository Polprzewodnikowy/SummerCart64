module sd_dma (
    input i_clk,
    input i_reset,

    input i_fifo_flush,
    input i_fifo_push,
    output o_fifo_full,
    output o_fifo_empty,
    input [31:0] i_fifo_data,

    output reg o_request,
    output reg o_write,
    input i_busy,
    output reg [31:0] o_data
);

    reg [31:0] r_dma_fifo_mem [0:127];
    
    reg [6:0] r_dma_fifo_wrptr;
    reg [6:0] r_dma_fifo_rdptr;

    assign o_fifo_full = (r_dma_fifo_wrptr + 1'd1) == r_dma_fifo_rdptr;
    assign o_fifo_empty = r_dma_fifo_wrptr == r_dma_fifo_rdptr;

    wire [31:0] w_rddata = r_dma_fifo_mem[r_dma_fifo_rdptr];

    wire w_request_successful = o_request && !i_busy;

    always @(posedge i_clk) begin
        if (i_reset || i_fifo_flush) begin
            r_dma_fifo_wrptr <= 7'd0;
        end else begin
            if (i_fifo_push) begin
                r_dma_fifo_wrptr <= r_dma_fifo_wrptr + 1'd1;
                r_dma_fifo_mem[r_dma_fifo_wrptr] <= i_fifo_data;
            end
        end
    end

    always @(posedge i_clk) begin
        if (i_reset || i_fifo_flush) begin
            o_request <= 1'b0;
            o_write <= 1'b1;
            r_dma_fifo_rdptr <= 7'd0;
        end else begin
            if (!o_request && !o_fifo_empty) begin
                o_request <= 1'b1;
                o_data <= w_rddata;
                r_dma_fifo_rdptr <= r_dma_fifo_rdptr + 1'd1;
            end
            if (w_request_successful) begin
                if (o_fifo_empty) begin
                    o_request <= 1'b0;
                end else begin
                    r_dma_fifo_rdptr <= r_dma_fifo_rdptr + 1'd1;
                    o_data <= w_rddata;
                end
            end
        end
    end

endmodule
