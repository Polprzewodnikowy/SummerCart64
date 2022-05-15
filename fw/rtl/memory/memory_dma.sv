interface dma_scb ();

    logic start;
    logic stop;
    logic busy;
    logic direction;
    logic [26:0] starting_address;
    logic [26:0] transfer_length;

    modport controller (
        output start,
        output stop,
        input busy,
        output direction,
        output starting_address,
        output transfer_length
    );

    modport dma (
        input start,
        input stop,
        output busy,
        input direction,
        input starting_address,
        input transfer_length
    );

endinterface


module memory_dma (
    input clk,
    input reset,

    dma_scb.dma dma_scb,

    fifo_bus.controller fifo_bus,
    mem_bus.controller mem_bus
);

    typedef enum bit [0:0] { 
        STATE_FETCH,
        STATE_TRANSFER
    } e_state;

    // logic [31:0] remaining;
    logic [26:0] end_address;
    logic [15:0] data_buffer;
    logic byte_counter;
    e_state state;
    logic rx_delay;

    always_ff @(posedge clk) begin
        fifo_bus.rx_read <= 1'b0;
        fifo_bus.tx_write <= 1'b0;
        rx_delay <= fifo_bus.rx_read;

        if (rx_delay) begin
            // if (dma.address[0] || (remaining == 32'd1)) begin
                // dma.wdata <= {dma.rx_rdata, dma.rx_rdata};
            // end else begin
                mem_bus.wdata <= {mem_bus.wdata[7:0], fifo_bus.rx_rdata};
            // end
        end

        if (reset) begin
            dma_scb.busy <= 1'b0;
            mem_bus.request <= 1'b0;
        end else begin
            if (!dma_scb.busy) begin
                if (dma_scb.start) begin
                    dma_scb.busy <= 1'b1;
                    mem_bus.write <= dma_scb.direction;
                    mem_bus.address <= dma_scb.starting_address;
                    end_address <= dma_scb.starting_address + dma_scb.transfer_length;
                    // remaining <= dma.transfer_length;
                    byte_counter <= 1'd0;
                    state <= STATE_FETCH;
                end
            end else begin
                if (dma_scb.stop) begin
                    dma_scb.busy <= 1'b0;
                    mem_bus.request <= 1'b0;
                end else if (mem_bus.address != end_address/* remaining != 32'd0*/) begin
                    if (mem_bus.write) begin
                        case (state)
                            STATE_FETCH: begin
                                if (!fifo_bus.rx_empty && !(fifo_bus.rx_read && fifo_bus.rx_almost_empty)) begin
                                    fifo_bus.rx_read <= 1'b1;
                                    // if (dma.address[0]) begin
                                        // dma.wmask <= 2'b01;
                                        // state <= STATE_TRANSFER;
                                    // end else if (dma.starting_address[0] remaining == 32'd1) begin
                                        // dma.wmask <= 2'b10;
                                        // state <= STATE_TRANSFER;
                                    // end else begin
                                        byte_counter <= byte_counter + 1'd1;
                                        if (byte_counter) begin
                                            mem_bus.wmask <= 2'b11;
                                            state <= STATE_TRANSFER;
                                        end
                                    // end
                                end
                            end

                            STATE_TRANSFER: begin
                                if (!fifo_bus.rx_read) begin
                                    mem_bus.request <= 1'b1;
                                end
                                if (mem_bus.ack) begin
                                    mem_bus.request <= 1'b0;
                                    // if (dma.wmask != 2'b11) begin
                                        // dma.address <= dma.address + 1'd1;
                                        // remaining <= remaining - 1'd1;
                                    // end else begin
                                        mem_bus.address <= mem_bus.address + 2'd2;
                                        // remaining <= remaining - 2'd2;
                                    // end
                                    state <= STATE_FETCH;
                                end
                            end
                        endcase
                    end else begin
                        case (state)
                            STATE_FETCH: begin
                                mem_bus.request <= 1'b1;
                                if (mem_bus.ack) begin
                                    mem_bus.request <= 1'b0;
                                    data_buffer <= mem_bus.rdata;
                                    state <= STATE_TRANSFER;
                                end
                            end

                            STATE_TRANSFER: begin
                              if (!fifo_bus.tx_full && !(fifo_bus.tx_write && fifo_bus.tx_almost_full)) begin
                                    fifo_bus.tx_write <= 1'b1;
                                    // if (dma.address[0]) begin
                                    //     dma.address <= dma.address + 1'd1;
                                    //     // remaining <= remaining - 1'd1;
                                    //     dma.dma_tx_wdata <= data_buffer[7:0];
                                    //     state <= STATE_FETCH;
                                    // end else if (remaining == 32'd1) begin
                                    //     dma.address <= dma.address + 1'd1;
                                    //     // remaining <= remaining - 1'd1;
                                    //     dma.dma_tx_wdata <= data_buffer[15:8];
                                    //     state <= STATE_FETCH;
                                    // end else begin
                                        fifo_bus.tx_wdata <= byte_counter ? data_buffer[7:0] : data_buffer[15:8];
                                        byte_counter <= byte_counter + 1'd1;
                                        if (byte_counter) begin
                                            mem_bus.address <= mem_bus.address + 2'd2;
                                            // remaining <= remaining - 2'd2;
                                            state <= STATE_FETCH;
                                        end
                                    // end
                                end
                            end
                        endcase
                    end
                end else begin
                    dma_scb.busy <= 1'b0;
                end
            end
        end
    end

endmodule
