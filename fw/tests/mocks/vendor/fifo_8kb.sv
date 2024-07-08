module fifo_8kb (
    input clk,
    input reset,

    output empty,
    input read,
    output [7:0] rdata,

    output full,
    input write,
    input [7:0] wdata,

    output logic [10:0] count
);

    fifo_mock #(
        .DEPTH(1024)
    ) fifo_8kb (
        .clk(clk),
        .reset(reset),

        .empty(empty),
        .read(read),
        .rdata(rdata),

        .full(full),
        .write(write),
        .wdata(wdata),

        .count(count)
    );

endmodule
