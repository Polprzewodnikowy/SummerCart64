module fifo_8kb (
    input clk,
    input reset,

    output empty,
    output almost_empty,
    input read,
    output [7:0] rdata,

    output full,
    output almost_full,
    input write,
    input [7:0] wdata
);

    fifo_8kb_lattice_generated fifo_8kb_lattice_generated_inst (
        .Data(wdata),
        .WrClock(clk),
        .RdClock(clk),
        .WrEn(write),
        .RdEn(read),
        .Reset(reset),
        .RPReset(reset),
        .Q(rdata),
        .Empty(empty),
        .Full(full), 
        .AlmostEmpty(almost_empty),
        .AlmostFull(almost_full)
    );

endmodule
