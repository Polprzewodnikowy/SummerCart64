interface dd_scb ();

    // N64 controlled regs

    logic hard_reset;
    logic [15:0] data;
    logic [7:0] cmd;
    logic cmd_pending;
    logic cmd_interrupt;
    logic bm_start_pending;
    logic bm_stop_pending;
    logic bm_transfer_mode;
    logic bm_transfer_blocks;
    logic bm_pending;
    logic bm_interrupt;
    logic bm_interrupt_ack;
    logic [7:0] sector_num;
    logic [7:0] sector_size;
    logic [7:0] sector_size_full;
    logic [7:0] sectors_in_block;


    // CPU controlled regs

    logic hard_reset_clear;
    logic [15:0] cmd_data;
    logic cmd_ready;
    logic bm_start_clear;
    logic bm_stop_clear;
    logic bm_transfer_c2;
    logic bm_transfer_data;
    logic bm_micro_error;
    logic bm_clear;
    logic bm_ready;
    logic disk_inserted;
    logic disk_changed;
    logic index_lock;
    logic [12:0] head_track;
    logic [15:0] drive_id;

    modport controller (
        input hard_reset,
        input data,
        input cmd,
        input cmd_pending,
        input bm_start_pending,
        input bm_stop_pending,
        input bm_transfer_mode,
        input bm_transfer_blocks,
        input bm_pending,
        input bm_interrupt_ack,
        input sector_num,
        input sector_size,
        input sector_size_full,
        input sectors_in_block,

        output hard_reset_clear,
        output cmd_data,
        output cmd_ready,
        output bm_start_clear,
        output bm_stop_clear,
        output bm_transfer_c2,
        output bm_transfer_data,
        output bm_micro_error,
        output bm_ready,
        output bm_clear,
        output disk_inserted,
        output disk_changed,
        output index_lock,
        output head_track,
        output drive_id
    );

    modport dd (
        output hard_reset,
        output data,
        output cmd,
        output cmd_pending,
        output cmd_interrupt,
        output bm_start_pending,
        output bm_stop_pending,
        output bm_transfer_mode,
        output bm_transfer_blocks,
        output bm_pending,
        output bm_interrupt,
        output bm_interrupt_ack,
        output sector_num,
        output sector_size,
        output sector_size_full,
        output sectors_in_block,

        input hard_reset_clear,
        input cmd_data,
        input cmd_ready,
        input bm_start_clear,
        input bm_stop_clear,
        input bm_transfer_c2,
        input bm_transfer_data,
        input bm_micro_error,
        input bm_clear,
        input bm_ready,
        input disk_inserted,
        input disk_changed,
        input index_lock,
        input head_track,
        input drive_id
    );

endinterface


module n64_dd (
    input clk,
    input reset,

    n64_reg_bus.dd reg_bus,

    n64_scb.dd n64_scb,
    dd_scb.dd dd_scb,

    output logic irq
);

    const bit [10:0] MEM_C2_BUFFER      = 11'h000;
    const bit [10:0] MEM_SECTOR_BUFFER  = 11'h400;

    typedef enum bit [10:0] {
        REG_DATA        = 11'h500,
        REG_CMD_SR      = 11'h508,
        REG_TRK_CUR     = 11'h50C,
        REG_BM_SCR      = 11'h510,
        REG_RESET       = 11'h520,
        REG_SEC_SIZ     = 11'h528,
        REG_SEC_INFO    = 11'h530,
        REG_ID          = 11'h540
    } e_reg_id;

    typedef enum bit [3:0] {
        BM_CONTROL_START_BUFFER_MANAGER     = 4'd15,
        BM_CONTROL_BUFFER_MANAGER_MODE      = 4'd14,
        BM_CONTROL_BUFFER_MANAGER_RESET     = 4'd12,
        BM_CONTROL_BLOCK_TRANSFER           = 4'd9,
        BM_CONTROL_MECHANIC_INTERRUPT_RESET = 4'd8
    } e_bm_control_id;

    always_comb begin
        reg_bus.rdata = 16'd0;
        if (reg_bus.address[10:8] == MEM_SECTOR_BUFFER[10:8]) begin
            reg_bus.rdata = n64_scb.dd_rdata;
        end else begin
            case (reg_bus.address[10:0])
                REG_DATA: reg_bus.rdata = dd_scb.data;
                REG_CMD_SR: reg_bus.rdata = {
                    1'b0,
                    dd_scb.bm_transfer_data,
                    1'b0,
                    dd_scb.bm_transfer_c2,
                    1'b0,
                    dd_scb.bm_interrupt,
                    dd_scb.cmd_interrupt,
                    dd_scb.disk_inserted,
                    dd_scb.cmd_pending,
                    dd_scb.hard_reset,
                    1'b0,
                    1'b0,
                    1'b0,
                    1'b0,
                    1'b0,
                    dd_scb.disk_changed
                };
                REG_TRK_CUR: reg_bus.rdata = {1'd0, {2{dd_scb.index_lock}}, dd_scb.head_track};
                REG_BM_SCR: reg_bus.rdata = {6'd0, dd_scb.bm_micro_error, 9'd0};
                REG_ID: reg_bus.rdata = dd_scb.drive_id;
            endcase
        end
    end

    always_ff @(posedge clk) begin
        dd_scb.bm_interrupt_ack <= 1'b0;

        if (dd_scb.hard_reset_clear) begin
            dd_scb.hard_reset <= 1'b0;
        end
        if (dd_scb.cmd_ready) begin
            dd_scb.data <= dd_scb.cmd_data;
            dd_scb.cmd_pending <= 1'b0;
            dd_scb.cmd_interrupt <= 1'b1;
        end
        if (dd_scb.bm_start_clear) begin
            dd_scb.bm_start_pending <= 1'b0;
        end
        if (dd_scb.bm_stop_clear) begin
            dd_scb.bm_stop_pending <= 1'b0;
        end
        if (dd_scb.bm_clear) begin
            dd_scb.bm_pending <= 1'b0;
        end
        if (dd_scb.bm_ready) begin
            dd_scb.bm_interrupt <= 1'b1;
        end
        if (reg_bus.address[10:0] == (MEM_C2_BUFFER + ({dd_scb.sector_size[7:1], 1'b0} * 3'd4)) && reg_bus.read) begin
            dd_scb.bm_pending <= 1'b1;
        end
        if (reg_bus.address[10:0] == (MEM_SECTOR_BUFFER + {dd_scb.sector_size[7:1], 1'b0}) && (reg_bus.read || reg_bus.write)) begin
            dd_scb.bm_pending <= 1'b1;
        end
        if (reg_bus.address[10:0] == REG_CMD_SR && reg_bus.read) begin
            dd_scb.bm_interrupt <= 1'b0;
            dd_scb.bm_interrupt_ack <= 1'b1;
        end

        if (reset || n64_scb.n64_reset) begin
            dd_scb.hard_reset <= 1'b1;
            dd_scb.cmd_pending <= 1'b0;
            dd_scb.cmd_interrupt <= 1'b0;
            dd_scb.bm_start_pending <= 1'b0;
            dd_scb.bm_stop_pending <= 1'b0;
            dd_scb.bm_pending <= 1'b0;
            dd_scb.bm_interrupt <= 1'b0;
        end else if (reg_bus.write) begin
            case (reg_bus.address[10:0])
                REG_DATA: begin
                    dd_scb.data <= reg_bus.wdata;
                end

                REG_CMD_SR: begin
                    dd_scb.cmd <= reg_bus.wdata[7:0];
                    dd_scb.cmd_pending <= 1'b1;
                end

                REG_BM_SCR: begin
                    dd_scb.sector_num <= reg_bus.wdata[7:0];
                    if (reg_bus.wdata[BM_CONTROL_START_BUFFER_MANAGER]) begin
                        dd_scb.bm_start_pending <= 1'b1;
                        dd_scb.bm_stop_pending <= 1'b0;
                        dd_scb.bm_transfer_mode <= reg_bus.wdata[BM_CONTROL_BUFFER_MANAGER_MODE];
                        dd_scb.bm_transfer_blocks <= reg_bus.wdata[BM_CONTROL_BLOCK_TRANSFER];
                    end
                    if (reg_bus.wdata[BM_CONTROL_BUFFER_MANAGER_RESET]) begin
                        dd_scb.bm_start_pending <= 1'b0;
                        dd_scb.bm_stop_pending <= 1'b1;
                        dd_scb.bm_transfer_mode <= 1'b0;
                        dd_scb.bm_transfer_blocks <= 1'b0;
                        dd_scb.bm_pending <= 1'b0;
                        dd_scb.bm_interrupt <= 1'b0;
                    end
                    if (reg_bus.wdata[BM_CONTROL_MECHANIC_INTERRUPT_RESET]) begin
                        dd_scb.cmd_interrupt <= 1'b0;
                    end
                end

                REG_RESET: begin
                    if (reg_bus.wdata == 16'hAAAA) begin
                        dd_scb.hard_reset <= 1'b1;
                        dd_scb.cmd_pending <= 1'b0;
                        dd_scb.cmd_interrupt <= 1'b0;
                        dd_scb.bm_start_pending <= 1'b0;
                        dd_scb.bm_stop_pending <= 1'b0;
                        dd_scb.bm_pending <= 1'b0;
                        dd_scb.bm_interrupt <= 1'b0;
                    end
                end

                REG_SEC_SIZ: begin
                    dd_scb.sector_size <= reg_bus.wdata[7:0];
                end

                REG_SEC_INFO: begin
                    dd_scb.sectors_in_block <= reg_bus.wdata[15:8];
                    dd_scb.sector_size_full <= reg_bus.wdata[7:0];
                end
            endcase
        end
    end

    always_comb begin
        irq = dd_scb.cmd_interrupt || dd_scb.bm_interrupt;
    end

    always_comb begin
        n64_scb.dd_write = reg_bus.write && reg_bus.address[10:8] == MEM_SECTOR_BUFFER[10:8];
        n64_scb.dd_address = reg_bus.address[7:1];
        n64_scb.dd_wdata = reg_bus.wdata;
    end

endmodule
