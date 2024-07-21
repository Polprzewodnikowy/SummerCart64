module fifo_bus_fifo_mock #(
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

    logic rx_full;
    logic rx_write;
    logic [7:0] rx_wdata;

    logic [PTR_BITS:0] rx_count;

    fifo_mock #(
        .DEPTH(DEPTH)
    ) fifo_rx (
        .clk(clk),
        .reset(reset),

        .empty(fifo_bus.rx_empty),
        .read(fifo_bus.rx_read),
        .rdata(fifo_bus.rx_rdata),

        .full(rx_full),
        .write(rx_write),
        .wdata(rx_wdata),

        .count(rx_count)
    );

    localparam int FILL_BITS = $clog2(FILL_RATE);
    logic [FILL_BITS:0] fill_counter;
    logic rx_fill;

    always_ff @(posedge clk) begin
        rx_fill <= rx_fill_enabled;
    end

    generate;
        if (FILL_RATE == 0) begin
            always_comb begin
                rx_write = rx_fill && !rx_full;
            end
        end else begin
            always_comb begin
                rx_write = rx_fill && !rx_full && (fill_counter == (FILL_BITS + 1)'(FILL_RATE));
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

    logic tx_empty;
    logic tx_read;
    logic [7:0] tx_rdata;

    logic [PTR_BITS:0] tx_count;

    fifo_mock #(
        .DEPTH(DEPTH)
    ) fifo_tx (
        .clk(clk),
        .reset(reset),

        .empty(tx_empty),
        .read(tx_read),
        .rdata(tx_rdata),

        .full(fifo_bus.tx_full),
        .write(fifo_bus.tx_write),
        .wdata(fifo_bus.tx_wdata),

        .count(tx_count)
    );

    localparam int DRAIN_BITS = $clog2(DRAIN_RATE);
    logic [DRAIN_BITS:0] drain_counter;
    logic tx_drain;

    always_ff @(posedge clk) begin
        tx_drain <= tx_drain_enabled;
    end

    generate;
        if (DRAIN_RATE == 0) begin
            always_comb begin
                tx_read = tx_drain && !tx_empty;
            end
        end else begin
            always_comb begin
                tx_read = tx_drain && !tx_empty && (drain_counter == (DRAIN_BITS + 1)'(DRAIN_RATE));
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
