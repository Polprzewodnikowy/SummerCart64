module memory_dma_tb;

    logic clk;
    logic reset;

    dma_scb dma_scb ();
    fifo_bus fifo_bus ();
    mem_bus mem_bus ();

    logic start;
    logic stop;
    logic direction;
    logic byte_swap;
    logic [26:0] starting_address;
    logic [26:0] transfer_length;

    logic flush;
    logic rx_fill_enabled;
    logic tx_drain_enabled;

    memory_dma memory_dma (
        .clk(clk),
        .reset(reset),

        .dma_scb(dma_scb),
        .fifo_bus(fifo_bus),
        .mem_bus(mem_bus)
    );

    dma_controller_mock dma_controller_mock (
        .clk(clk),
        .reset(reset),

        .dma_scb(dma_scb),

        .start(start),
        .stop(stop),
        .direction(direction),
        .byte_swap(byte_swap),
        .starting_address(starting_address),
        .transfer_length(transfer_length)
    );

    fifo_bus_mock #(
        .DEPTH(16),
        .FILL_RATE(16),
        .DRAIN_RATE(16)
    ) fifo_bus_mock (
        .clk(clk),
        .reset(reset),

        .fifo_bus(fifo_bus),

        .flush(flush),

        .rx_fill_enabled(rx_fill_enabled),
        .tx_drain_enabled(tx_drain_enabled)
    );

    memory_sdram_mock memory_sdram_mock (
        .clk(clk),
        .reset(reset),

        .mem_bus(mem_bus)
    );

    initial begin
        clk = 1'b0;
        forever begin
            clk = ~clk; #0.5;
        end
    end

    initial begin
        reset = 1'b0;
        #10;
        reset = 1'b1;
        #10;
        reset = 1'b0;
    end

    initial begin
        $dumpfile("traces/memory_dma_tb.vcd");

        #10000;

        $dumpvars();

        #100;
        start = 1'b1;
        direction = 1'b0;
        byte_swap = 1'b0;
        starting_address = 27'hFFF0;
        transfer_length = 27'd32;
        #1;
        start = 1'b0;

        #9;
        tx_drain_enabled = 1'b1;

        #490;
        stop = 1'b1;
        #1;
        stop = 1'b0;

        #99;

        start = 1'b1;
        direction = 1'b1;
        #1;
        start = 1'b0;

        #9;
        rx_fill_enabled = 1'b1;

        #490;
        stop = 1'b1;
        #1;
        stop = 1'b0;

        #99;

        $finish;
    end

endmodule
