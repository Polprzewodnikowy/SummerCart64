module fifo_mock #(
    parameter int DEPTH = 1024,
    localparam int PTR_BITS = $clog2(DEPTH)
) (
    input clk,
    input reset,

    output logic empty,
    input read,
    output [7:0] rdata,

    output logic full,
    input write,
    input [7:0] wdata,

    output logic [PTR_BITS:0] count
);

    logic [7:0] fifo_mem [0:(DEPTH - 1)];
    logic [(PTR_BITS - 1):0] fifo_rptr;
    logic [(PTR_BITS - 1):0] fifo_wptr;

    always_comb begin
        full = count >= (PTR_BITS + 1)'(DEPTH);
        empty = count == (PTR_BITS + 1)'('d0);
    end

    always_ff @(posedge clk) begin
        if (read) begin
            rdata <= fifo_mem[fifo_rptr];
            fifo_rptr <= fifo_rptr + PTR_BITS'('d1);
            count <= count - (PTR_BITS + 1)'('d1);
        end
        if (write) begin
            fifo_mem[fifo_wptr] <= wdata;
            fifo_wptr <= fifo_wptr + PTR_BITS'('d1);
            count <= count + (PTR_BITS + 1)'('d1);
        end
        if (read && write) begin
            count <= count;
        end
        if (reset) begin
            count <= (PTR_BITS + 1)'('d0);
            fifo_rptr <= PTR_BITS'('d0);
            fifo_wptr <= PTR_BITS'('d0);
        end
    end

endmodule
