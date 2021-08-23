interface if_config ();

    logic cpu_bootstrapped;
    logic cpu_busy;
    logic request;
    logic [7:0] command;
    logic [31:0] arg [0:1];
    logic [31:0] response;
    logic boot_write;
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
        input cpu_bootstrapped,
        input cpu_busy,
        output request,
        output command,
        output arg,
        input response,
        output boot_write
    );

    modport cpu (
        output cpu_bootstrapped,
        output cpu_busy,
        input request,
        input command,
        input arg,
        output response,
        input boot_write,
        output sdram_switch,
        output sdram_writable,
        output dd_enabled,
        output sram_enabled,
        output flashram_enabled,
        output dd_offset,
        output save_offset
    );

endinterface
