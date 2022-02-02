interface if_memory_dma ();

    logic request;
    logic ack;
    logic write;
    logic [1:0] wmask;
    logic [31:0] address;
    logic [15:0] rdata;
    logic [15:0] wdata;

    logic start;
    logic stop;
    logic busy;
    logic direction;
    logic [31:0] transfer_length;
    logic [31:0] starting_address;

    logic dma_rx_read;
    logic dma_tx_write;
    logic [7:0] dma_tx_wdata;

    logic cpu_rx_read;
    logic cpu_tx_write;
    logic [7:0] cpu_tx_wdata;

    logic rx_empty;
    logic rx_almost_empty;
    logic rx_read;
    logic [7:0] rx_rdata;

    logic tx_full;
    logic tx_almost_full;
    logic tx_write;
    logic [7:0] tx_wdata;

    always_comb begin
        rx_read = dma_rx_read || cpu_rx_read;
        tx_write = dma_tx_write || cpu_tx_write;
        tx_wdata = cpu_tx_write ? cpu_tx_wdata : dma_tx_wdata;
    end

    modport dma (
        input start,
        input stop,
        output busy,
        input direction,
        input transfer_length,
        input starting_address,

        output request,
        input ack,
        output write,
        output wmask,
        output address,
        input rdata,
        output wdata,

        input rx_empty,
        input rx_almost_empty,
        output dma_rx_read,
        input rx_rdata,

        input tx_full,
        input tx_almost_full,
        output dma_tx_write,
        output dma_tx_wdata
    );

    modport cpu (
        output start,
        output stop,
        input busy,
        output direction,
        output transfer_length,
        output starting_address,

        input rx_empty,
        output cpu_rx_read,
        input rx_rdata,

        input tx_full,
        output cpu_tx_write,
        output cpu_tx_wdata
    );

    modport memory (
        input request,
        output ack,
        input write,
        input wmask,
        input address,
        output rdata,
        input wdata
    );

    modport device (
        output rx_empty,
        output rx_almost_empty,
        input rx_read,
        output rx_rdata,

        output tx_full,
        output tx_almost_full,
        input tx_write,
        input tx_wdata
    );

endinterface


module memory_dma (
    input clk,
    input reset,
    if_memory_dma.dma dma
);

    typedef enum bit [0:0] { 
        STATE_FETCH,
        STATE_TRANSFER
    } e_state;

    logic [31:0] remaining;
    logic [15:0] data_buffer;
    logic byte_counter;
    e_state state;

    always_ff @(posedge clk) begin
        dma.dma_rx_read <= 1'b0;
        dma.dma_tx_write <= 1'b0;

        if (dma.dma_rx_read) begin
            if (dma.address[0] || (remaining == 32'd1)) begin
                dma.wdata <= {dma.rx_rdata, dma.rx_rdata};
            end else begin
                dma.wdata <= {dma.wdata[7:0], dma.rx_rdata};
            end
        end

        if (reset) begin
            dma.busy <= 1'b0;
            dma.request <= 1'b0;
        end else begin
            if (!dma.busy) begin
                if (dma.start) begin
                    dma.busy <= 1'b1;
                    dma.write <= dma.direction;
                    dma.address <= dma.starting_address;
                    remaining <= dma.transfer_length;
                    byte_counter <= 1'd0;
                    state <= STATE_FETCH;
                end
            end else begin
                if (dma.stop) begin
                    dma.busy <= 1'b0;
                    dma.request <= 1'b0;
                end else if (remaining != 32'd0) begin
                    if (dma.write) begin
                        case (state)
                            STATE_FETCH: begin
                                if (!dma.rx_empty && !(dma.dma_rx_read && dma.rx_almost_empty)) begin
                                    dma.dma_rx_read <= 1'b1;
                                    if (dma.address[0]) begin
                                        dma.wmask <= 2'b01;
                                        state <= STATE_TRANSFER;
                                    end else if (remaining == 32'd1) begin
                                        dma.wmask <= 2'b10;
                                        state <= STATE_TRANSFER;
                                    end else begin
                                        byte_counter <= byte_counter + 1'd1;
                                        if (byte_counter) begin
                                            dma.wmask <= 2'b11;
                                            state <= STATE_TRANSFER;
                                        end
                                    end
                                end
                            end

                            STATE_TRANSFER: begin
                                dma.request <= 1'b1;
                                if (dma.ack) begin
                                    dma.request <= 1'b0;
                                    if (dma.wmask != 2'b11) begin
                                        dma.address <= dma.address + 1'd1;
                                        remaining <= remaining - 1'd1;
                                    end else begin
                                        dma.address <= dma.address + 2'd2;
                                        remaining <= remaining - 2'd2;
                                    end
                                    state <= STATE_FETCH;
                                end
                            end
                        endcase
                    end else begin
                        case (state)
                            STATE_FETCH: begin
                                dma.request <= 1'b1;
                                if (dma.ack) begin
                                    dma.request <= 1'b0;
                                    data_buffer <= dma.rdata;
                                    state <= STATE_TRANSFER;
                                end
                            end

                            STATE_TRANSFER: begin
                              if (!dma.tx_full && !(dma.dma_tx_write && dma.tx_almost_full)) begin
                                    dma.dma_tx_write <= 1'b1;
                                    if (dma.address[0]) begin
                                        dma.address <= dma.address + 1'd1;
                                        remaining <= remaining - 1'd1;
                                        dma.dma_tx_wdata <= data_buffer[7:0];
                                        state <= STATE_FETCH;
                                    end else if (remaining == 32'd1) begin
                                        dma.address <= dma.address + 1'd1;
                                        remaining <= remaining - 1'd1;
                                        dma.dma_tx_wdata <= data_buffer[15:8];
                                        state <= STATE_FETCH;
                                    end else begin
                                        dma.dma_tx_wdata <= byte_counter ? data_buffer[7:0] : data_buffer[15:8];
                                        byte_counter <= byte_counter + 1'd1;
                                        if (byte_counter) begin
                                            dma.address <= dma.address + 2'd2;
                                            remaining <= remaining - 2'd2;
                                            state <= STATE_FETCH;
                                        end
                                    end
                                end
                            end
                        endcase
                    end
                end else begin
                    dma.busy <= 1'b0;
                end
            end
        end
    end

endmodule
