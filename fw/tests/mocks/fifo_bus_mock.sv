module fifo_bus_mock #(
    parameter int DEPTH = 1024,
    parameter int FILL_RATE = 3,
    parameter int DRAIN_RATE = 3
) (
    input clk,
    input reset,

    fifo_bus.fifo fifo_bus,

    input flush,

    input rx_fill_enabled,
    input tx_drain_enabled
);

    localparam int PTR_BITS = $clog2(DEPTH);


    // RX FIFO mock

    logic [7:0] rx_fifo_mem [0:(DEPTH - 1)];
    logic [(PTR_BITS - 1):0] rx_fifo_rptr;
    logic [(PTR_BITS - 1):0] rx_fifo_wptr;
    logic [PTR_BITS:0] rx_available;
    logic rx_fifo_full;
    logic rx_fifo_almost_full;
    logic rx_write;
    logic [7:0] rx_wdata;

    always_comb begin
        rx_fifo_full = rx_available >= (PTR_BITS + 1)'(DEPTH);
        rx_fifo_almost_full = rx_available >= (PTR_BITS + 1)'(DEPTH - 1);
        fifo_bus.rx_empty = rx_available == (PTR_BITS + 1)'('d0);
        fifo_bus.rx_almost_empty = rx_available <= (PTR_BITS + 1)'('d1);
    end

    always_ff @(posedge clk) begin
        if (fifo_bus.rx_read) begin
            fifo_bus.rx_rdata <= rx_fifo_mem[rx_fifo_rptr];
            rx_fifo_rptr <= rx_fifo_rptr + PTR_BITS'('d1);
            rx_available <= rx_available - (PTR_BITS + 1)'('d1);
        end
        if (rx_write) begin
            rx_fifo_mem[rx_fifo_wptr] <= rx_wdata;
            rx_fifo_wptr <= rx_fifo_wptr + PTR_BITS'('d1);
            rx_available <= rx_available + (PTR_BITS + 1)'('d1);
        end
        if (fifo_bus.rx_read && rx_write) begin
            rx_available <= rx_available;
        end
        if (reset || flush) begin
            rx_available <= (PTR_BITS + 1)'('d0);
            rx_fifo_rptr <= PTR_BITS'('d0);
            rx_fifo_wptr <= PTR_BITS'('d0);
        end
    end

    localparam int FILL_BITS = $clog2(FILL_RATE);
    logic [FILL_BITS:0] fill_counter;
    logic rx_fill;

    always_ff @(posedge clk) begin
        rx_fill <= rx_fill_enabled;
    end

    generate;
        if (FILL_RATE == 0) begin
            always_comb begin
                rx_write = rx_fill && !rx_fifo_full;
            end
        end else begin
            always_comb begin
                rx_write = rx_fill && !rx_fifo_full && (fill_counter == (FILL_BITS + 1)'(FILL_RATE));
            end
            always_ff @(posedge clk) begin
                if (fill_counter < (FILL_BITS + 1)'(FILL_RATE)) begin
                    fill_counter <= fill_counter + (FILL_BITS + 1)'('d1);
                end
                if (reset) begin
                    fill_counter <= (FILL_BITS + 1)'('d0);
                end else begin
                    if (rx_write) begin
                        fill_counter <= (FILL_BITS + 1)'('d0);
                    end
                end
            end
        end
    endgenerate

    always_ff @(posedge clk) begin
        if (reset) begin
            rx_wdata <= 8'h01;
        end else begin
            if (rx_write) begin
                rx_wdata <= rx_wdata + 8'h01;
            end
        end
    end


    // TX FIFO mock

    logic [7:0] tx_fifo_mem [0:(DEPTH - 1)];
    logic [(PTR_BITS - 1):0] tx_fifo_rptr;
    logic [(PTR_BITS - 1):0] tx_fifo_wptr;
    logic [PTR_BITS:0] tx_available;
    logic tx_fifo_empty;
    logic tx_fifo_almost_empty;
    logic tx_read;
    logic [7:0] tx_rdata;

    always_comb begin
        tx_fifo_empty = tx_available == (PTR_BITS + 1)'('d0);
        tx_fifo_almost_empty = tx_available <= (PTR_BITS + 1)'('d1);
        fifo_bus.tx_full = tx_available >= (PTR_BITS + 1)'(DEPTH);
        fifo_bus.tx_almost_full = tx_available >= (PTR_BITS + 1)'(DEPTH - 1);
    end

    always_ff @(posedge clk) begin
        if (fifo_bus.tx_write) begin
            tx_fifo_mem[tx_fifo_wptr] <= fifo_bus.tx_wdata;
            tx_fifo_wptr <= tx_fifo_wptr + PTR_BITS'('d1);
            tx_available <= tx_available + (PTR_BITS + 1)'('d1);
        end
        if (tx_read) begin
            tx_rdata <= tx_fifo_mem[tx_fifo_rptr];
            tx_fifo_rptr <= tx_fifo_rptr + PTR_BITS'('d1);
            tx_available <= tx_available - (PTR_BITS + 1)'('d1);
        end
        if (fifo_bus.tx_write && tx_read) begin
            tx_available <= tx_available;
        end
        if (reset || flush) begin
            tx_available <= (PTR_BITS + 1)'('d0);
            tx_fifo_rptr <= PTR_BITS'('d0);
            tx_fifo_wptr <= PTR_BITS'('d0);
        end
    end

    localparam int DRAIN_BITS = $clog2(DRAIN_RATE);
    logic [DRAIN_BITS:0] drain_counter;
    logic tx_drain;

    always_ff @(posedge clk) begin
        tx_drain <= tx_drain_enabled;
    end

    generate;
        if (DRAIN_RATE == 0) begin
            always_comb begin
                tx_read = tx_drain && !tx_fifo_empty;
            end
        end else begin
            always_comb begin
                tx_read = tx_drain && !tx_fifo_empty && (drain_counter == (DRAIN_BITS + 1)'(DRAIN_RATE));
            end
            always_ff @(posedge clk) begin
                if (drain_counter < (DRAIN_BITS + 1)'(DRAIN_RATE)) begin
                    drain_counter <= drain_counter + (DRAIN_BITS + 1)'('d1);
                end
                if (reset) begin
                    drain_counter <= (DRAIN_BITS + 1)'('d0);
                end else begin
                    if (tx_read) begin
                        drain_counter <= (DRAIN_BITS + 1)'('d0);
                    end
                end
            end
        end
    endgenerate

endmodule
