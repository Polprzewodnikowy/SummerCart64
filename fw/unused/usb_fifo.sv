module usb_fifo #(
    parameter FIFO_LENGTH = 1024,
    parameter FIFO_WIDTH = 8
) (
    if_system.sys system_if,

    input flush,

    output full,
    input write,
    input [(FIFO_WIDTH - 1):0] wdata,

    output empty,
    input read,
    output reg [(FIFO_WIDTH - 1):0] rdata
);

    localparam FIFO_ADDRESS_WIDTH = $clog2(FIFO_LENGTH);

    reg [(FIFO_WIDTH - 1):0] r_fifo_mem [0:(FIFO_LENGTH - 1)];

    reg [FIFO_ADDRESS_WIDTH:0] r_fifo_wrptr;
    reg [FIFO_ADDRESS_WIDTH:0] r_fifo_rdptr;

    wire w_full_or_empty = r_fifo_wrptr[(FIFO_ADDRESS_WIDTH - 1):0] == r_fifo_rdptr[(FIFO_ADDRESS_WIDTH - 1):0];

    assign empty = r_fifo_wrptr == r_fifo_rdptr;
    assign full = !empty && w_full_or_empty;

    always_ff @(posedge system_if.clk) begin
        rdata <= r_fifo_mem[r_fifo_rdptr[(FIFO_ADDRESS_WIDTH - 1):0]];
        if (system_if.reset || flush) begin
            r_fifo_wrptr <= {(FIFO_ADDRESS_WIDTH + 1){1'b0}};
            r_fifo_rdptr <= {(FIFO_ADDRESS_WIDTH + 1){1'b0}};
        end else begin
            if (write) begin
                r_fifo_mem[r_fifo_wrptr[(FIFO_ADDRESS_WIDTH - 1):0]] <= wdata;
                r_fifo_wrptr <= r_fifo_wrptr + 1'd1;
            end
            if (read) begin
                r_fifo_rdptr <= r_fifo_rdptr + 1'd1;
            end
        end
    end

endmodule
