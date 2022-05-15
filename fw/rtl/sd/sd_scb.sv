interface sd_scb ();

    logic [1:0] clock_mode;

    logic [5:0] index;

    modport controller (
        output clock_mode,

        output index
    );

    modport clk (
        input clock_mode
    );

    modport cmd (
        input index
    );

endinterface
