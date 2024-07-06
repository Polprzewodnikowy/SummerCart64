module memory_dma (
    input clk,
    input reset,

    dma_scb.dma dma_scb,

    fifo_bus.controller fifo_bus,
    mem_bus.controller mem_bus
);

    // logic stop_requested;
    // logic dma_done;

    // always_ff @(posedge clk) begin
    //     if (reset) begin
    //         dma_scb.busy <= 1'b0;
    //         stop_requested <= 1'b0;
    //     end else begin
    //         if (dma_scb.start) begin
    //             dma_scb.busy <= 1'b1;
    //         end

    //         if (dma_scb.busy && dma_scb.stop) begin
    //             stop_requested <= 1'b1;
    //         end

    //         if (dma_done) begin
    //             dma_scb.busy <= 1'b0;
    //             stop_requested <= 1'b0;
    //         end
    //     end
    // end

    // logic unaligned_start;
    // logic unaligned_end;

    // always_ff @(posedge clk) begin
    //     if (dma_scb.start) begin
    //         unaligned_start <= dma_scb.starting_address[0];
    //         // unaligned_end <= ;
    //     end
    // end

    // logic [26:0] bytes_remaining;

    // always_ff @(posedge clk) begin
    //     bytes_remaining <= bytes_remaining - 27'd1;
    //     if (dma_scb.start) begin
    //         bytes_remaining <= dma_scb.transfer_length;
    //     end
    // end




    // logic mem_transfer_request;
    // logic mem_wdata_buffer_ready;
    // logic mem_rdata_buffer_ready;
    // logic [15:0] mem_wdata_buffer;
    // logic [15:0] mem_rdata_buffer;
    // logic [1:0] mem_wdata_buffer_valid_bytes;
    // logic mem_rdata_buffer_valid;

    // always_comb begin
    //     mem_transfer_request = (
    //         !mem_bus.request || mem_bus.ack
    //     ) && (
    //         mem_wdata_buffer_ready || mem_rdata_buffer_ready
    //     );
    // end

    // always_ff @(posedge clk) begin
    //     if (dma_scb.start) begin
    //         mem_bus.write <= dma_scb.direction;
    //         mem_bus.address <= {dma_scb.starting_address[26:1], 1'b0};
    //         mem_rdata_buffer_valid <= 1'b0;
    //     end

    //     if (mem_bus.ack) begin
    //         mem_bus.request <= 1'b0;
    //         mem_bus.address <= mem_bus.address + 27'd2;
    //         mem_rdata_buffer <= mem_bus.rdata;
    //         mem_rdata_buffer_valid <= 1'b1;
    //     end

    //     if (reset) begin
    //         mem_bus.request <= 1'b0;
    //     end else if (mem_transfer_request) begin
    //         mem_bus.request <= 1'b1;
    //         mem_bus.wmask <= mem_wdata_buffer_valid_bytes;
    //     end

    //     if (!mem_bus.request || mem_bus.ack) begin
    //         if (mem_wdata_buffer_ready) begin
    //             if (dma_scb.byte_swap) begin
    //                 mem_bus.wdata[15:8] <= mem_wdata_buffer[7:0];
    //                 mem_bus.wdata[7:0] <= mem_wdata_buffer[15:8];
    //             end else begin
    //                 mem_bus.wdata <= mem_wdata_buffer;
    //             end
    //         end
    //     end
    // end








    // // always_ff @(posedge clk) begin
    // //     mem_wdata_buffer_ready <= dma_scb.busy;
    // //     mem_wdata_buffer_valid_bytes <= 2'b10;
    // // end






    // logic [1:0] rx_fifo_bytes_available;
    // logic [1:0] tx_fifo_bytes_available;

    // always_comb begin
    //     rx_fifo_bytes_available = 2'd2;
    //     if (fifo_bus.rx_almost_empty) begin
    //         rx_fifo_bytes_available = 2'd1;
    //     end
    //     if (fifo_bus.rx_empty) begin
    //         rx_fifo_bytes_available = 2'd0;
    //     end

    //     tx_fifo_bytes_available = 2'd2;
    //     if (fifo_bus.tx_almost_full) begin
    //         tx_fifo_bytes_available = 2'd1;
    //     end
    //     if (fifo_bus.tx_full) begin
    //         tx_fifo_bytes_available = 2'd0;
    //     end
    // end

    // always_ff @(posedge clk) begin
    //     if (dma_scb.busy) begin
    //         if (!dma_scb.direction) begin
    //             // RX FIFO handling
    //         end
    //     end
    // end

    // always_ff @(posedge clk) begin
    //     if (dma_scb.busy) begin
    //         if (dma_scb.direction) begin
    //             // TX FIFO handling
    //         end
    //     end
    // end



//XDDDAWDWD














    // always_ff @(posedge clk) begin
    //     dma_done <= 1'b0;

    //     if (dma_scb.busy && bytes_remaining == 27'd0) begin
    //         // dma_done <= 1'b1;
    //     end
    // end





    // typedef enum bit [1:0] {

    // } e_rx_fifo_state;

    // typedef enum bit [1:0] {

    // } e_tx_fifo_state;

    // typedef enum bit [1:0] {
    //     MEM_BUS_IDLE = 2'b00,
    //     MEM_BUS_WAIT = 2'b01,
    //     MEM_BUS_TRANSFERRING = 2'b10,
    // } e_mem_bus_state;

    // e_mem_bus_state mem_bus_state;
    // e_mem_bus_state next_mem_bus_state;

    // always_ff @(posedge clk) begin
    //     mem_bus_state <= next_mem_bus_state;
    //     if (reset) begin
    //         mem_bus_state <= MEM_BUS_IDLE;
    //     end
    // end

    // always_comb begin
    //     next_mem_bus_state = mem_bus_state;

    //     case (mem_bus_state)
    //         MEM_BUS_IDLE: begin
    //             if (dma_scb.start) begin
    //                 next_mem_bus_state = MEM_BUS_WAIT;
    //             end
    //         end

    //         MEM_BUS_WAIT: begin
    //             i
    //         end

    //         MEM_BUS_TRANSFERRING: begin

    //         end
    //     endcase
    // end


    // DMA start/stop control

    logic dma_start;
    logic dma_stop;
    logic dma_stop_requested;

    always_comb begin
        dma_start = dma_scb.start && !dma_scb.stop && !dma_scb.busy;
        dma_stop = dma_scb.stop;
    end

    always_ff @(posedge clk) begin
        if (reset) begin
            dma_stop_requested <= 1'b0;
        end else begin
            if (dma_stop) begin
                dma_stop_requested <= 1'b1;
            end
            if (dma_start) begin
                dma_stop_requested <= 1'b0;
            end
        end
    end


    // Remaining counter and FIFO enable

    logic [26:0] remaining;
    logic trx_enabled;

    always_comb begin
        trx_enabled = remaining > 27'd0;
    end


    // RX FIFO controller

    logic [1:0] rx_wmask;
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
            !fifo_bus.rx_read &&
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
                rx_wmask <= 2'b01;
                rx_buffer_counter <= 2'd1;
                rx_buffer_valid_counter <= 2'd1;
            end else begin
                rx_wmask <= 2'b11;
                rx_buffer_counter <= 2'd0;
                rx_buffer_valid_counter <= 2'd0;
            end
        end

        if (rx_rdata_pop) begin
            rx_buffer_counter <= rx_buffer_counter + 1'd1;
        end

        if (rx_rdata_shift || rx_rdata_valid) begin
            if (dma_scb.byte_swap) begin
                rx_buffer <= {fifo_bus.rx_rdata, rx_buffer[15:8]};
            end else begin
                rx_buffer <= {rx_buffer[7:0], fifo_bus.rx_rdata};
            end
            rx_buffer_valid_counter <= rx_buffer_valid_counter + 1'd1;
            if (remaining == 27'd0 && rx_buffer_counter == 2'd1) begin
                rx_wmask <= 2'b10;
                rx_rdata_shift <= 1'b1;
                rx_buffer_counter <= rx_buffer_counter + 1'd1;
            end
        end

        if (rx_buffer_valid && !mem_bus.request) begin
            rx_wmask <= 2'b11;
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

            if (!dma_stop_requested && ((mem_bus.write && rx_rdata_pop) || (!mem_bus.write && tx_wdata_push))) begin
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
                        mem_bus.wmask <= rx_wmask;
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
            mem_bus.address <= mem_bus.address + 27'd2;
        end
    end

endmodule
