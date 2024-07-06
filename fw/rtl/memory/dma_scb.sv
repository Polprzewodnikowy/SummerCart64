interface dma_scb ();

    logic start;
    logic stop;
    logic busy;
    logic direction;
    logic byte_swap;
    logic [26:0] starting_address;
    logic [26:0] transfer_length;

    modport controller (
        output start,
        output stop,
        input busy,
        output direction,
        output byte_swap,
        output starting_address,
        output transfer_length
    );

    modport dma (
        input start,
        input stop,
        output busy,
        input direction,
        input byte_swap,
        input starting_address,
        input transfer_length
    );

endinterface
