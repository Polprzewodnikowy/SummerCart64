module memory_dma (
    input clk,
    input reset,

    dma_scb.dma dma_scb,

    fifo_bus.controller fifo_bus,
    mem_bus.controller mem_bus
);

    // Start/stop logic

    logic dma_start;
    logic dma_stop;

    always_ff @(posedge clk) begin
        dma_start <= 1'b0;
        if (dma_scb.stop) begin
            dma_stop <= 1'b1;
        end else if (dma_scb.start) begin
            dma_start <= 1'b1;
            dma_stop <= 1'b0;
        end
    end


    // Memory bus controller

    typedef enum bit [1:0] {
        MEM_BUS_STATE_IDLE,
        MEM_BUS_STATE_WAIT,
        MEM_BUS_STATE_TRANSFER
    } e_mem_bus_state;

    e_mem_bus_state mem_bus_state;
    e_mem_bus_state next_mem_bus_state;

    logic mem_bus_wdata_ready;
    logic mem_bus_wdata_empty;
    logic mem_bus_wdata_end;
    logic [1:0] mem_bus_wdata_valid;
    logic [15:0] mem_bus_wdata_buffer;

    logic mem_bus_rdata_ready;
    logic mem_bus_rdata_ack;
    logic mem_bus_rdata_end;
    logic [1:0] mem_bus_rdata_valid;
    logic [15:0] mem_bus_rdata_buffer;

    logic [26:0] mem_bus_remaining_bytes;
    logic mem_bus_last_transfer;
    logic mem_bus_almost_last_transfer;
    logic mem_bus_unaligned_start;
    logic mem_bus_unaligned_end;

    always_ff @(posedge clk) begin
        if (reset) begin
            mem_bus_state <= MEM_BUS_STATE_IDLE;
        end else begin
            mem_bus_state <= next_mem_bus_state;
        end
    end

    always_comb begin
        next_mem_bus_state = mem_bus_state;

        mem_bus_last_transfer = (mem_bus_remaining_bytes == 27'd0);
        mem_bus_almost_last_transfer = (mem_bus_remaining_bytes <= 27'd2);

        mem_bus_wdata_end = mem_bus_last_transfer;
        mem_bus_wdata_valid = (mem_bus_unaligned_end && mem_bus_almost_last_transfer) ? 2'b10 :
                              mem_bus_unaligned_start ? 2'b01 :
                              2'b11;

        case (mem_bus_state)
            MEM_BUS_STATE_IDLE: begin
                if (dma_start) begin
                    next_mem_bus_state = MEM_BUS_STATE_WAIT;
                end
            end

            MEM_BUS_STATE_WAIT: begin
                if (dma_stop || mem_bus_last_transfer) begin
                    next_mem_bus_state = MEM_BUS_STATE_IDLE;
                end else if (mem_bus_wdata_ready || !mem_bus_wdata_empty || mem_bus_rdata_ready) begin
                    next_mem_bus_state = MEM_BUS_STATE_TRANSFER;
                end
            end

            MEM_BUS_STATE_TRANSFER: begin
                if (mem_bus.ack) begin
                    if (dma_stop || mem_bus_last_transfer) begin
                        next_mem_bus_state = MEM_BUS_STATE_IDLE;
                    end else if (!(mem_bus_wdata_ready || !mem_bus_wdata_empty || mem_bus_rdata_ready)) begin
                        next_mem_bus_state = MEM_BUS_STATE_WAIT;
                    end
                end
            end

            default: begin
                next_mem_bus_state = MEM_BUS_STATE_IDLE;
            end
        endcase
    end

    always_ff @(posedge clk) begin
        mem_bus_rdata_ack <= 1'b0;

        if (mem_bus.ack) begin
            mem_bus.request <= 1'b0;
            mem_bus.address <= (mem_bus.address + 26'd2);
            mem_bus_rdata_ack <= 1'b1;
            mem_bus_rdata_end <= mem_bus_last_transfer;
            mem_bus_rdata_valid <= mem_bus.wmask;
            mem_bus_rdata_buffer <= mem_bus.rdata;
        end

        if (mem_bus_wdata_ready) begin
            mem_bus_wdata_empty <= 1'b0;
        end

        case (mem_bus_state)
            MEM_BUS_STATE_IDLE: begin
                mem_bus.request <= 1'b0;
                mem_bus_rdata_end <= 1'b1;
                if (dma_start) begin
                    mem_bus.write <= dma_scb.direction;
                    mem_bus.address <= {dma_scb.starting_address[26:1], 1'b0};
                    mem_bus_remaining_bytes <= dma_scb.transfer_length;
                    mem_bus_unaligned_start <= dma_scb.starting_address[0];
                    mem_bus_unaligned_end <= (dma_scb.starting_address[0] ^ dma_scb.transfer_length[0]);
                    mem_bus_rdata_end <= 1'b0;
                    mem_bus_wdata_empty <= 1'b1;
                end
            end

            MEM_BUS_STATE_WAIT: begin
                if (!dma_stop && !mem_bus_last_transfer) begin
                    if (mem_bus_wdata_ready || !mem_bus_wdata_empty || mem_bus_rdata_ready) begin
                        mem_bus.request <= 1'b1;
                        mem_bus_unaligned_start <= 1'b0;
                        mem_bus.wdata <= (dma_scb.byte_swap ? {mem_bus_wdata_buffer[7:0], mem_bus_wdata_buffer[15:8]} : mem_bus_wdata_buffer);
                        mem_bus.wmask <= 2'b11;
                        mem_bus_remaining_bytes <= (mem_bus_remaining_bytes - 27'd2);
                        if (mem_bus_unaligned_end && mem_bus_almost_last_transfer) begin
                            mem_bus.wmask <= 2'b10;
                            mem_bus_remaining_bytes <= (mem_bus_remaining_bytes - 27'd1);
                        end
                        if (mem_bus_unaligned_start) begin
                            mem_bus.wmask <= 2'b01;
                            mem_bus_remaining_bytes <= (mem_bus_remaining_bytes - 27'd1);
                        end
                        mem_bus_wdata_empty <= 1'b1;
                    end
                end
            end

            MEM_BUS_STATE_TRANSFER: begin
                if (!dma_stop && !mem_bus_last_transfer) begin
                    if (mem_bus.ack && (mem_bus_wdata_ready || !mem_bus_wdata_empty || mem_bus_rdata_ready)) begin
                        mem_bus.request <= 1'b1;
                        mem_bus.wdata <= (dma_scb.byte_swap ? {mem_bus_wdata_buffer[7:0], mem_bus_wdata_buffer[15:8]} : mem_bus_wdata_buffer);
                        mem_bus.wmask <= 2'b11;
                        mem_bus_remaining_bytes <= (mem_bus_remaining_bytes - 27'd2);
                        if (mem_bus_unaligned_end && mem_bus_almost_last_transfer) begin
                            mem_bus.wmask <= 2'b10;
                            mem_bus_remaining_bytes <= (mem_bus_remaining_bytes - 27'd1);
                        end
                        mem_bus_wdata_empty <= 1'b1;
                    end
                end
            end

            default: begin end
        endcase
    end


    // RX FIFO controller

    typedef enum bit [2:0] {
        RX_FIFO_BUS_STATE_IDLE,
        RX_FIFO_BUS_STATE_WAIT,
        RX_FIFO_BUS_STATE_TRANSFER_1,
        RX_FIFO_BUS_STATE_TRANSFER_2,
        RX_FIFO_BUS_STATE_ACK
    } e_rx_fifo_bus_state;

    e_rx_fifo_bus_state rx_fifo_bus_state;
    e_rx_fifo_bus_state next_rx_fifo_bus_state;

    logic rx_fifo_shift;
    logic rx_fifo_shift_delayed;
    logic [1:0] rx_fifo_valid;

    always_ff @(posedge clk) begin
        if (reset || dma_stop) begin
            rx_fifo_bus_state <= RX_FIFO_BUS_STATE_IDLE;
        end else begin
            rx_fifo_bus_state <= next_rx_fifo_bus_state;
        end
    end

    always_comb begin
        next_rx_fifo_bus_state = rx_fifo_bus_state;

        rx_fifo_shift = 1'b0;

        fifo_bus.rx_read = 1'b0;

        case (rx_fifo_bus_state)
            RX_FIFO_BUS_STATE_IDLE: begin
                if (dma_start && dma_scb.direction) begin
                    next_rx_fifo_bus_state = RX_FIFO_BUS_STATE_WAIT;
                end
            end

            RX_FIFO_BUS_STATE_WAIT: begin
                if (mem_bus_wdata_end) begin
                    next_rx_fifo_bus_state = RX_FIFO_BUS_STATE_IDLE;
                end else if (mem_bus_wdata_empty) begin
                    next_rx_fifo_bus_state = RX_FIFO_BUS_STATE_TRANSFER_1;
                end
            end

            RX_FIFO_BUS_STATE_TRANSFER_1: begin
                fifo_bus.rx_read = (!fifo_bus.rx_empty && rx_fifo_valid[1]);
                if (!fifo_bus.rx_empty || !rx_fifo_valid[1]) begin
                    next_rx_fifo_bus_state = RX_FIFO_BUS_STATE_TRANSFER_2;
                    rx_fifo_shift = 1'b1;
                end
            end

            RX_FIFO_BUS_STATE_TRANSFER_2: begin
                fifo_bus.rx_read = (!fifo_bus.rx_empty && rx_fifo_valid[1]);
                if (!fifo_bus.rx_empty || !rx_fifo_valid[1]) begin
                    next_rx_fifo_bus_state = RX_FIFO_BUS_STATE_ACK;
                    rx_fifo_shift = 1'b1;
                end
            end

            RX_FIFO_BUS_STATE_ACK: begin
                if (mem_bus_wdata_ready) begin
                    next_rx_fifo_bus_state = RX_FIFO_BUS_STATE_WAIT;
                end
            end

            default: begin
                next_rx_fifo_bus_state = RX_FIFO_BUS_STATE_IDLE;
            end
        endcase
    end

    always_ff @(posedge clk) begin
        mem_bus_wdata_ready <= 1'b0;
        rx_fifo_shift_delayed <= rx_fifo_shift;

        if (rx_fifo_shift) begin
            rx_fifo_valid <= {rx_fifo_valid[0], 1'bX};
        end

        if (rx_fifo_shift_delayed) begin
            if (rx_fifo_bus_state == RX_FIFO_BUS_STATE_ACK) begin
                mem_bus_wdata_ready <= 1'b1;
            end
            mem_bus_wdata_buffer <= {mem_bus_wdata_buffer[7:0], fifo_bus.rx_rdata};
        end

        case (rx_fifo_bus_state)
            RX_FIFO_BUS_STATE_WAIT: begin
                if (mem_bus_wdata_empty) begin
                    rx_fifo_valid <= mem_bus_wdata_valid;
                end
            end

            default: begin end
        endcase
    end


    // TX FIFO controller

    typedef enum bit [1:0] {
        TX_FIFO_BUS_STATE_IDLE,
        TX_FIFO_BUS_STATE_WAIT,
        TX_FIFO_BUS_STATE_TRANSFER_1,
        TX_FIFO_BUS_STATE_TRANSFER_2
    } e_tx_fifo_bus_state;

    e_tx_fifo_bus_state tx_fifo_bus_state;
    e_tx_fifo_bus_state next_tx_fifo_bus_state;

    logic tx_fifo_shift;
    logic tx_fifo_waiting;
    logic [1:0] tx_fifo_valid;
    logic [15:0] tx_fifo_buffer;

    always_ff @(posedge clk) begin
        if (reset || dma_stop) begin
            tx_fifo_bus_state <= TX_FIFO_BUS_STATE_IDLE;
        end else begin
            tx_fifo_bus_state <= next_tx_fifo_bus_state;
        end
    end

    always_comb begin
        next_tx_fifo_bus_state = tx_fifo_bus_state;

        tx_fifo_shift = 1'b0;

        fifo_bus.tx_write = 1'b0;
        fifo_bus.tx_wdata = tx_fifo_buffer[15:8];

        case (tx_fifo_bus_state)
            TX_FIFO_BUS_STATE_IDLE: begin
                if (dma_start && !dma_scb.direction) begin
                    next_tx_fifo_bus_state = TX_FIFO_BUS_STATE_WAIT;
                end
            end

            TX_FIFO_BUS_STATE_WAIT: begin
                if (mem_bus_rdata_ack || tx_fifo_waiting) begin
                    next_tx_fifo_bus_state = TX_FIFO_BUS_STATE_TRANSFER_1;
                end else if (mem_bus_rdata_end) begin
                    next_tx_fifo_bus_state = TX_FIFO_BUS_STATE_IDLE;
                end
            end

            TX_FIFO_BUS_STATE_TRANSFER_1: begin
                fifo_bus.tx_write = (!fifo_bus.tx_full && tx_fifo_valid[1]);
                if (!fifo_bus.tx_full || !tx_fifo_valid[1]) begin
                    next_tx_fifo_bus_state = TX_FIFO_BUS_STATE_TRANSFER_2;
                    tx_fifo_shift = 1'b1;
                end
            end

            TX_FIFO_BUS_STATE_TRANSFER_2: begin
                fifo_bus.tx_write = (!fifo_bus.tx_full && tx_fifo_valid[1]);
                if (!fifo_bus.tx_full || !tx_fifo_valid[1]) begin
                    next_tx_fifo_bus_state = TX_FIFO_BUS_STATE_WAIT;
                    tx_fifo_shift = 1'b1;
                    if (!mem_bus_rdata_ack && !tx_fifo_waiting && mem_bus_rdata_end) begin
                        next_tx_fifo_bus_state = TX_FIFO_BUS_STATE_IDLE;
                    end
                end
            end

            default: begin
                next_tx_fifo_bus_state = TX_FIFO_BUS_STATE_IDLE;
            end
        endcase
    end

    always_ff @(posedge clk) begin
        if (tx_fifo_shift) begin
            tx_fifo_valid <= {tx_fifo_valid[0], 1'bX};
            tx_fifo_buffer <= {tx_fifo_buffer[7:0], 8'hXX};
        end

        case (tx_fifo_bus_state)
            TX_FIFO_BUS_STATE_IDLE: begin
                mem_bus_rdata_ready <= 1'b0;
                tx_fifo_waiting <= 1'b0;
                if (dma_start) begin
                    mem_bus_rdata_ready <= !dma_scb.direction;
                end
            end

            TX_FIFO_BUS_STATE_WAIT: begin
                if (mem_bus_rdata_ack || tx_fifo_waiting) begin
                    mem_bus_rdata_ready <= 1'b0;
                    tx_fifo_waiting <= 1'b0;
                    tx_fifo_valid <= mem_bus_rdata_valid;
                    tx_fifo_buffer <= mem_bus_rdata_buffer;
                end
            end

            TX_FIFO_BUS_STATE_TRANSFER_1: begin
                if (mem_bus_rdata_ack) begin
                    tx_fifo_waiting <= 1'b1;
                end
            end

            TX_FIFO_BUS_STATE_TRANSFER_2: begin
                if (mem_bus_rdata_ack) begin
                    tx_fifo_waiting <= 1'b1;
                end
                if (fifo_bus.tx_write || !tx_fifo_valid[1]) begin
                    mem_bus_rdata_ready <= !mem_bus_rdata_end;
                end
            end

            default: begin end
        endcase
    end


    // DMA busy indicator

    always_ff @(posedge clk) begin
        dma_scb.busy <= (
            (dma_scb.start && !dma_scb.stop) ||
            dma_start ||
            (mem_bus_state != MEM_BUS_STATE_IDLE) ||
            (rx_fifo_bus_state != RX_FIFO_BUS_STATE_IDLE) ||
            (tx_fifo_bus_state != TX_FIFO_BUS_STATE_IDLE)
        );
    end

endmodule
