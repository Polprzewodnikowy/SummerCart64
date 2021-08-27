interface if_config ();

    logic cpu_ready;
    logic cpu_busy;
    logic cmd_request;
    logic [7:0] cmd;
    logic [31:0] data [0:1];
    logic [1:0] data_write;
    logic [31:0] wdata;
    logic sdram_switch;
    logic sdram_writable;
    logic dd_enabled;
    logic sram_enabled;
    logic flashram_enabled;
    logic flashram_read_mode;
    logic [25:0] dd_offset;
    logic [25:0] save_offset;

    modport pi (
        input sdram_switch,
        input sdram_writable,
        input dd_enabled,
        input sram_enabled,
        input flashram_enabled,
        input flashram_read_mode,
        input dd_offset,
        input save_offset
    );

    modport flashram (
        output flashram_read_mode
    );

    modport n64 (
        input cpu_ready,
        input cpu_busy,
        output cmd_request,
        output cmd,
        output data,
        input data_write,
        input wdata
    );

    modport cpu (
        output cpu_ready,
        output cpu_busy,
        input cmd_request,
        input cmd,
        input data,
        output data_write,
        output wdata,
        output sdram_switch,
        output sdram_writable,
        output dd_enabled,
        output sram_enabled,
        output flashram_enabled,
        output dd_offset,
        output save_offset
    );

endinterface
