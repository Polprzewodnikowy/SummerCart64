module dma_controller_mock (
    input clk,
    input reset,

    dma_scb.controller dma_scb,

    input start,
    input stop,
    input direction,
    input byte_swap,
    input [26:0] starting_address,
    input [26:0] transfer_length
);

    always_ff @(posedge clk) begin
        dma_scb.start <= 1'b0;
        dma_scb.stop <= 1'b0;

        if (reset) begin
            dma_scb.direction <= 1'b0;
            dma_scb.byte_swap <= 1'b0;
            dma_scb.starting_address <= 27'd0;
            dma_scb.transfer_length <= 27'd0;
        end else begin
            if (start) begin
                dma_scb.start <= 1'b1;
                dma_scb.direction <= direction;
                dma_scb.byte_swap <= byte_swap;
                dma_scb.starting_address <= starting_address;
                dma_scb.transfer_length <= transfer_length;
            end

            if (stop) begin
                dma_scb.stop <= 1'b1;
            end
        end
    end

endmodule
