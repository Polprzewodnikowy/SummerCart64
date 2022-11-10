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
    input [7:0] wdata,
    
    output logic [10:0] count
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

    always_ff @(posedge clk) begin
        if (reset) begin
            count <= 11'd0;
        end else begin
            if (write && read) begin
                count <= count;
            end else if (write) begin
                count <= count + 1'd1;
            end else if (read) begin
                count <= count - 1'd1;
            end
        end
    end

endmodule
