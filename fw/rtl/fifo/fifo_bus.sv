interface fifo_bus ();

    logic rx_empty;
    logic rx_almost_empty;
    logic rx_read;
    logic [7:0] rx_rdata;

    logic tx_full;
    logic tx_almost_full;
    logic tx_write;
    logic [7:0] tx_wdata;

    modport controller (
        input rx_empty,
        input rx_almost_empty,
        output rx_read,
        input rx_rdata,

        input tx_full,
        input tx_almost_full,
        output tx_write,
        output tx_wdata
    );

    modport fifo (
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
