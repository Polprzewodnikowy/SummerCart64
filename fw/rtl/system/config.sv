interface if_config ();

    logic sdram_switch;
    logic sdram_writable;
    logic dd_enabled;
    logic sram_enabled;
    logic flashram_enabled;
    logic flashram_read_mode;
    logic [25:0] dd_offset;
    logic [25:0] save_offset;

    always_comb begin
        sdram_switch = 1'b0;
        sdram_writable = 1'b0;
        dd_enabled = 1'b1;
        sram_enabled = 1'b1;
        flashram_enabled = 1'b1;
        flashram_read_mode = 1'b1;
        dd_offset = 26'h3BE_0000;
        save_offset = 26'h3FE_0000;
    end

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

endinterface
