interface usb_scb ();

    logic fifo_flush;
    logic fifo_flush_busy;
    logic write_buffer_flush;
    logic [10:0] rx_count;
    logic [10:0] tx_count;
    logic pwrsav;
    logic reset_state;
    logic reset_on_ack;
    logic reset_off_ack;

    modport controller (
        output fifo_flush,
        input fifo_flush_busy,
        output write_buffer_flush,
        input rx_count,
        input tx_count,
        input pwrsav,
        input reset_state,
        output reset_on_ack,
        output reset_off_ack
    );

    modport usb (
        input fifo_flush,
        output fifo_flush_busy,
        input write_buffer_flush,
        output rx_count,
        output tx_count,
        output pwrsav,
        output reset_state,
        input reset_on_ack,
        input reset_off_ack
    );

endinterface
