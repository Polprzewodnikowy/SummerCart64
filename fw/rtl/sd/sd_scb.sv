interface sd_scb ();

    logic [1:0] clock_mode;

    logic card_busy;

    logic [10:0] rx_count;
    logic [10:0] tx_count;

    logic [5:0] cmd_index;
    logic [31:0] cmd_arg;
    logic [127:0] cmd_rsp;
    logic cmd_start;
    logic cmd_skip_response;
    logic cmd_reserved_response;
    logic cmd_long_response;
    logic cmd_ignore_crc;
    logic cmd_busy;
    logic cmd_error;

    logic dat_fifo_flush;
    logic dat_start_write;
    logic dat_start_read;
    logic dat_stop;
    logic [7:0] dat_blocks;
    logic dat_busy;
    logic dat_error;

    modport controller (
        output clock_mode,

        input card_busy,

        input rx_count,
        input tx_count,

        output cmd_index,
        output cmd_arg,
        input cmd_rsp,
        output cmd_start,
        output cmd_skip_response,
        output cmd_reserved_response,
        output cmd_long_response,
        output cmd_ignore_crc,
        input cmd_busy,
        input cmd_error,

        output dat_fifo_flush,
        output dat_start_write,
        output dat_start_read,
        output dat_stop,
        output dat_blocks,
        input dat_busy,
        input dat_error
    );

    modport clk (
        input clock_mode
    );

    modport cmd (
        input cmd_index,
        input cmd_arg,
        output cmd_rsp,
        input cmd_start,
        input cmd_skip_response,
        input cmd_reserved_response,
        input cmd_long_response,
        input cmd_ignore_crc,
        output cmd_busy,
        output cmd_error
    );

    modport dat (
        output card_busy,

        output rx_count,
        output tx_count,

        input dat_fifo_flush,
        input dat_start_write,
        input dat_start_read,
        input dat_stop,
        input dat_blocks,
        output dat_busy,
        output dat_error
    );

endinterface
