module sd_dma (
    input i_clk,
    input i_reset,

    input [3:0] i_dma_bank,
    input [23:0] i_dma_address,
    input [14:0] i_dma_length,
    output reg [14:0] o_dma_left,
    input i_dma_load_bank_address,
    input i_dma_load_length,
    input i_dma_direction,
    input i_dma_start,
    input i_dma_stop,
    output reg o_dma_busy,

    output o_rx_fifo_pop,
    input i_rx_fifo_empty,
    input [31:0] i_rx_fifo_data,

    output reg o_tx_fifo_push,
    input i_tx_fifo_full,
    output [31:0] o_tx_fifo_data,

    output o_request,
    output reg o_write,
    input i_busy,
    input i_ack,
    output reg [3:0] o_bank,
    output reg [23:0] o_address,
    input [31:0] i_data,
    output [31:0] o_data
);

    wire w_request_successful = o_request && !i_busy;

    always @(posedge i_clk) begin
        if (i_dma_load_length && !o_dma_busy) begin
            o_dma_left <= i_dma_length;
        end else if (w_request_successful && o_dma_left > 15'd0) begin
            o_dma_left <= o_dma_left - 1'd1;
        end
    end

    always @(posedge i_clk) begin
        if (i_reset) begin
            o_dma_busy <= 1'b0;
        end else begin
            if (i_dma_start && !o_dma_busy) begin
                o_dma_busy <= 1'b1;
            end
            if (i_dma_stop || (w_request_successful && o_dma_left == 15'd0)) begin
                o_dma_busy <= 1'b0;
            end
        end
    end

    assign o_rx_fifo_pop = o_dma_busy && o_write && w_request_successful;

    assign o_tx_fifo_push = o_dma_busy && !o_write && i_ack;
    assign o_tx_fifo_data = i_data;

    reg r_pending_ack;

    always @(posedge i_clk) begin
        if (i_reset || i_dma_stop) begin
            r_pending_ack <= 1'b0;
        end else if (o_dma_busy && !o_write) begin
            if (w_request_successful) begin
                r_pending_ack <= 1'b1;
            end else if (i_ack) begin
                r_pending_ack <= 1'b0;
            end
        end
    end

    assign o_request = o_dma_busy && (o_write ? (
        !i_rx_fifo_empty
    ) : (
        !r_pending_ack && !i_tx_fifo_full
    ));

    always @(posedge i_clk) begin
        if (i_dma_start && !o_dma_busy) begin
            o_write <= i_dma_direction;
        end
    end

    always @(posedge i_clk) begin
        if (i_dma_load_bank_address && !o_dma_busy) begin
            o_bank <= i_dma_bank;
        end
    end

    always @(posedge i_clk) begin
        if (i_dma_load_bank_address && !o_dma_busy) begin
            o_address <= i_dma_address;
        end else if (w_request_successful) begin
            o_address <= o_address + 1'd1;
        end
    end

    assign o_data = i_rx_fifo_data;

endmodule
