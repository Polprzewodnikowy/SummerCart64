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

    // DMA start/stop control

    logic dma_start;
    logic dma_stop;

    always_comb begin
        dma_start = dma_scb.start && !dma_scb.stop && !dma_scb.busy;
        dma_stop = dma_scb.stop;
    end


    // Remaining counter and FIFO enable

    logic [26:0] remaining;
    logic trx_enabled;

    always_comb begin
        trx_enabled = remaining > 27'd0;
    end


    // RX FIFO controller

    logic rx_rdata_pop;
    logic rx_rdata_shift;
    logic rx_rdata_valid;
    logic [15:0] rx_buffer;
    logic rx_buffer_valid;
    logic [1:0] rx_buffer_counter;
    logic [1:0] rx_buffer_valid_counter;

    always_comb begin
        rx_buffer_valid = rx_buffer_valid_counter == 2'd2;
    end

    always_ff @(posedge clk) begin
        rx_rdata_pop <= (
            !rx_rdata_pop &&
            trx_enabled &&
            rx_buffer_counter < 2'd2 &&
            !fifo_bus.rx_empty &&
            mem_bus.write
        );
        rx_rdata_shift <= 1'b0;
        fifo_bus.rx_read <= rx_rdata_pop;
        rx_rdata_valid <= fifo_bus.rx_read;

        if (dma_start) begin
            if (dma_scb.starting_address[0]) begin
                mem_bus.wmask <= 2'b01;
                rx_buffer_counter <= 2'd1;
                rx_buffer_valid_counter <= 2'd1;
            end else begin
                mem_bus.wmask <= 2'b11;
                rx_buffer_counter <= 2'd0;
                rx_buffer_valid_counter <= 2'd0;
            end
        end

        if (rx_rdata_pop) begin
            rx_buffer_counter <= rx_buffer_counter + 1'd1;
        end

        if (rx_rdata_shift || rx_rdata_valid) begin
            rx_buffer <= {rx_buffer[7:0], fifo_bus.rx_rdata};
            rx_buffer_valid_counter <= rx_buffer_valid_counter + 1'd1;
            if (remaining == 27'd0 && rx_buffer_counter == 2'd1) begin
                mem_bus.wmask <= 2'b10;
                rx_rdata_shift <= 1'b1;
                rx_buffer_counter <= rx_buffer_counter + 1'd1;
            end
        end

        if (rx_buffer_valid && !mem_bus.request) begin
            rx_buffer_counter <= 2'd0;
            rx_buffer_valid_counter <= 2'd0;
        end
    end


    // TX FIFO controller

    logic tx_wdata_push;
    logic tx_wdata_first_push;
    logic [7:0] tx_buffer;
    logic tx_buffer_counter;
    logic tx_buffer_ready;
    logic tx_buffer_valid;

    always_comb begin
        fifo_bus.tx_write = tx_wdata_push;
    end

    always_ff @(posedge clk) begin
        tx_wdata_push <= (
            !tx_wdata_push &&
            trx_enabled &&
            tx_buffer_valid &&
            !fifo_bus.tx_full &&
            !mem_bus.write
        );

        if (reset || dma_stop) begin
            tx_buffer_ready <= 1'b0;
            tx_buffer_valid <= 1'b0;
        end

        if (dma_start) begin
            tx_wdata_first_push <= 1'b1;
            tx_buffer_ready <= 1'b1;
            tx_buffer_valid <= 1'b0;
        end

        if (tx_buffer_ready && mem_bus.request) begin
            tx_buffer_ready <= 1'b0;
        end

        if (mem_bus.ack) begin
            tx_wdata_first_push <= 1'b0;
            tx_buffer_counter <= 1'd1;
            tx_buffer_valid <= 1'b1;
            {fifo_bus.tx_wdata, tx_buffer} <= mem_bus.rdata;
            if (tx_wdata_first_push && dma_scb.starting_address[0]) begin
                fifo_bus.tx_wdata <= mem_bus.rdata[7:0];
                tx_buffer_counter <= 1'd0;
            end
        end

        if (tx_wdata_push) begin
            tx_buffer_counter <= tx_buffer_counter - 1'd1;
            fifo_bus.tx_wdata <= tx_buffer;
            if (tx_buffer_counter == 1'd0) begin
                tx_buffer_ready <= 1'b1;
                tx_buffer_valid <= 1'b0;
            end
        end
    end


    // Remaining counter controller

    always_ff @(posedge clk) begin
        if (reset || dma_stop) begin
            remaining <= 27'd0;
        end else begin
            if (dma_start) begin
                remaining <= dma_scb.transfer_length;
            end

            if ((mem_bus.write && rx_rdata_pop) || (!mem_bus.write && tx_wdata_push)) begin
                remaining <= remaining - 1'd1;
            end
        end
    end


    // Mem bus controller

    always_ff @(posedge clk) begin
        dma_scb.busy <= mem_bus.request || trx_enabled;
    end

    always_ff @(posedge clk) begin
        if (reset) begin
            mem_bus.request <= 1'b0;
        end else begin
            if (!mem_bus.request) begin
                if (mem_bus.write) begin
                    if (rx_buffer_valid) begin
                        mem_bus.request <= 1'b1;
                        mem_bus.wdata <= rx_buffer;
                    end
                end else begin
                    if (tx_buffer_ready) begin
                        mem_bus.request <= 1'b1;
                    end
                end
            end
        end

        if (mem_bus.ack) begin
            mem_bus.request <= 1'b0;
        end
    end

    always_ff @(posedge clk) begin
        if (dma_start) begin
            mem_bus.write <= dma_scb.direction;
        end
    end

    always_ff @(posedge clk) begin
        if (dma_start) begin
            mem_bus.address <= {dma_scb.starting_address[26:1], 1'b0};
        end

        if (mem_bus.ack) begin
            mem_bus.address <= mem_bus.address + 2'd2;
        end
    end

endmodule
