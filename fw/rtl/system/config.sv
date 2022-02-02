interface if_config ();

    logic cpu_ready;
    logic cpu_busy;
    logic cmd_error;
    logic cmd_request;
    logic [7:0] cmd;
    logic [31:0] data [0:1];
    logic [1:0] data_write;
    logic [31:0] wdata;
    logic sdram_switch;
    logic sdram_writable;
    logic dd_enabled;
    logic sram_enabled;
    logic sram_banked;
    logic flashram_enabled;
    logic flashram_read_mode;
    logic isv_enabled;
    logic [25:0] ddipl_offset;
    logic [25:0] save_offset;
    logic [25:0] isv_offset;
    logic [15:0] isv_rd_ptr;
    logic flash_erase_start;
    logic flash_erase_busy;
    logic flash_wp_enable;
    logic flash_wp_disable;

    modport pi (
        input sdram_switch,
        input sdram_writable,
        input dd_enabled,
        input sram_enabled,
        input sram_banked,
        input flashram_enabled,
        input flashram_read_mode,
        input isv_enabled,
        input ddipl_offset,
        input save_offset,
        input isv_offset
    );

    modport flashram (
        output flashram_read_mode
    );

    modport flash (
        input flash_erase_start,
        output flash_erase_busy,
        input flash_wp_enable,
        input flash_wp_disable
    );

    modport n64 (
        input cpu_ready,
        input cpu_busy,
        input cmd_error,
        output cmd_request,
        output cmd,
        output data,
        input data_write,
        input wdata,
        output isv_rd_ptr
    );

    modport cpu (
        output cpu_ready,
        output cpu_busy,
        output cmd_error,
        input cmd_request,
        input cmd,
        input data,
        output data_write,
        output wdata,
        output sdram_switch,
        output sdram_writable,
        output dd_enabled,
        output sram_enabled,
        output sram_banked,
        output flashram_enabled,
        output isv_enabled,
        output ddipl_offset,
        output save_offset,
        output isv_offset,
        input isv_rd_ptr,
        output flash_erase_start,
        input flash_erase_busy,
        output flash_wp_enable,
        output flash_wp_disable
    );

endinterface
