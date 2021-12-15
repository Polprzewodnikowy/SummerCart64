interface if_dd (
    output dd_interrupt
);

    // Sector buffer regs

    logic [6:0] n64_sector_address;
    logic n64_sector_address_valid;
    logic n64_sector_write;
    logic [15:0] n64_sector_wdata;

    logic [5:0] cpu_sector_address;
    logic cpu_sector_address_valid;
    logic cpu_sector_write;
    logic [31:0] cpu_sector_wdata;

    logic [31:0] sector_rdata;


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
    logic bm_ready;
    logic disk_inserted;
    logic disk_changed;
    logic index_lock;
    logic [12:0] head_track;
    logic [2:0] drive_id;


    always_comb begin
        dd_interrupt = cmd_interrupt || bm_interrupt;
    end

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
        input bm_ready,
        input disk_inserted,
        input disk_changed,
        input index_lock,
        input head_track,
        input drive_id,

        output .sector_address(n64_sector_address),
        output .sector_address_valid(n64_sector_address_valid),
        output .sector_write(n64_sector_write),
        output .sector_wdata(n64_sector_wdata),
        input sector_rdata
    );

    modport cpu (
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
        output disk_inserted,
        output disk_changed,
        output index_lock,
        output head_track,
        output drive_id,

        output .sector_address(cpu_sector_address),
        output .sector_address_valid(cpu_sector_address_valid),
        output .sector_write(cpu_sector_write),
        output .sector_wdata(cpu_sector_wdata),
        input sector_rdata
    );

    modport sector_buffer (
        input n64_sector_address,
        input n64_sector_address_valid,
        input n64_sector_write,
        input n64_sector_wdata,

        input cpu_sector_address,
        input cpu_sector_address_valid,
        input cpu_sector_write,
        input cpu_sector_wdata,

        output sector_rdata
    );

endinterface


module n64_dd (
    if_system.sys sys,
    if_n64_bus bus,
    if_dd.dd dd
);

    const bit [31:0] M_BASE             = 32'h0500_0000;
    const bit [31:0] M_C2_BUFFER        = M_BASE + 11'h000;
    const bit [31:0] M_SECTOR_BUFFER    = M_BASE + 11'h400;

    typedef enum bit [10:0] {
        R_DATA      = 11'h500,
        R_CMD_SR    = 11'h508,
        R_TRK_CUR   = 11'h50C,
        R_BM_SCR    = 11'h510,
        R_RESET     = 11'h520,
        R_SEC_SIZ   = 11'h528,
        R_SEC_INFO  = 11'h530,
        R_ID        = 11'h540
    } e_reg_id;

    typedef enum bit [3:0] {
        BM_CONTROL_START_BUFFER_MANAGER     = 4'd15,
        BM_CONTROL_BUFFER_MANAGER_MODE      = 4'd14,
        BM_CONTROL_BUFFER_MANAGER_RESET     = 4'd12,
        BM_CONTROL_BLOCK_TRANSFER           = 4'd9,
        BM_CONTROL_MECHANIC_INTERRUPT_RESET = 4'd8
    } e_bm_control_id;

    typedef enum bit [0:0] {
        S_IDLE,
        S_WAIT
    } e_state;

    e_state state;

    always_comb begin
        dd.sector_address = bus.address[7:1];
        dd.sector_address_valid = bus.request && bus.address[11:8] == M_SECTOR_BUFFER[11:8];
        dd.sector_write = bus.write && dd.sector_address_valid;
        dd.sector_wdata = bus.wdata;
    end

    always_comb begin
        bus.rdata = 16'd0;
        if (bus.ack) begin
            if (bus.address[10:8] == M_SECTOR_BUFFER[10:8]) begin
                if (bus.address[1]) begin
                    bus.rdata = dd.sector_rdata[15:0];
                end else begin
                    bus.rdata = dd.sector_rdata[31:16];
                end
            end else begin
                case (bus.address[10:0])
                    R_DATA: bus.rdata = dd.data;
                    R_CMD_SR: bus.rdata = {
                        1'b0,
                        dd.bm_transfer_data,
                        1'b0,
                        dd.bm_transfer_c2,
                        1'b0,
                        dd.bm_interrupt,
                        dd.cmd_interrupt,
                        dd.disk_inserted,
                        dd.cmd_pending,
                        dd.hard_reset,
                        1'b0,
                        1'b0,
                        1'b0,
                        1'b0,
                        1'b0,
                        dd.disk_changed
                    };
                    R_TRK_CUR: bus.rdata = {1'd0, {2{dd.index_lock}}, dd.head_track};
                    R_BM_SCR: bus.rdata = {6'd0, dd.bm_micro_error, 9'd0};
                    R_ID: bus.rdata = {13'd0, dd.drive_id};
                    default: bus.rdata = 16'd0;
                endcase
            end
        end
    end

    always_ff @(posedge sys.clk) begin
        bus.ack <= 1'b0;
        dd.bm_interrupt_ack <= 1'b0;

        if (dd.hard_reset_clear) begin
            dd.hard_reset <= 1'b0;
        end
        if (dd.cmd_ready) begin
            dd.data <= dd.cmd_data;
            dd.cmd_pending <= 1'b0;
            dd.cmd_interrupt <= 1'b1;
        end
        if (dd.bm_start_clear) begin
            dd.bm_start_pending <= 1'b0;
        end
        if (dd.bm_stop_clear) begin
            dd.bm_stop_pending <= 1'b0;
        end
        if (dd.bm_ready) begin
            dd.bm_pending <= 1'b0;
            dd.bm_interrupt <= 1'b1;
        end
        if (bus.real_address == (M_C2_BUFFER + ({dd.sector_size[7:1], 1'b0} * 3'd4)) && bus.read_op) begin
            dd.bm_pending <= 1'b1;
        end
        if (bus.real_address == (M_SECTOR_BUFFER + {dd.sector_size[7:1], 1'b0}) && (bus.read_op || bus.write_op)) begin
            dd.bm_pending <= 1'b1;
        end
        if (bus.real_address == (M_BASE + R_CMD_SR) && bus.read_op) begin
            dd.bm_interrupt <= 1'b0;
            dd.bm_interrupt_ack <= 1'b1;
        end

        if (sys.reset) begin
            dd.hard_reset <= 1'b1;
            dd.cmd_pending <= 1'b0;
            dd.cmd_interrupt <= 1'b0;
            dd.bm_start_pending <= 1'b0;
            dd.bm_stop_pending <= 1'b0;
            dd.bm_pending <= 1'b0;
            dd.bm_interrupt <= 1'b0;
            state <= S_IDLE;
        end else begin
            case (state)
                S_IDLE: begin
                    if (bus.request) begin
                        state <= S_WAIT;
                        bus.ack <= 1'b1;
                        if (bus.write) begin
                            case (bus.address[10:0])
                                R_DATA: begin
                                    dd.data <= bus.wdata;
                                end

                                R_CMD_SR: begin
                                    dd.cmd <= bus.wdata[7:0];
                                    dd.cmd_pending <= 1'b1;
                                end

                                R_BM_SCR: begin
                                    dd.sector_num <= bus.wdata[7:0];
                                    if (bus.wdata[BM_CONTROL_START_BUFFER_MANAGER]) begin
                                        dd.bm_start_pending <= 1'b1;
                                        dd.bm_stop_pending <= 1'b0;
                                        dd.bm_transfer_mode <= bus.wdata[BM_CONTROL_BUFFER_MANAGER_MODE];
                                        dd.bm_transfer_blocks <= bus.wdata[BM_CONTROL_BLOCK_TRANSFER];
                                    end
                                    if (bus.wdata[BM_CONTROL_BUFFER_MANAGER_RESET]) begin
                                        dd.bm_start_pending <= 1'b0;
                                        dd.bm_stop_pending <= 1'b1;
                                        dd.bm_transfer_mode <= 1'b0;
                                        dd.bm_transfer_blocks <= 1'b0;
                                        dd.bm_pending <= 1'b0;
                                        dd.bm_interrupt <= 1'b0;
                                    end
                                    if (bus.wdata[BM_CONTROL_MECHANIC_INTERRUPT_RESET]) begin
                                        dd.cmd_interrupt <= 1'b0;
                                    end
                                end

                                R_RESET: begin
                                    if (bus.wdata == 16'hAAAA) begin
                                        dd.hard_reset <= 1'b1;
                                        dd.cmd_pending <= 1'b0;
                                        dd.cmd_interrupt <= 1'b0;
                                        dd.bm_start_pending <= 1'b0;
                                        dd.bm_stop_pending <= 1'b0;
                                        dd.bm_pending <= 1'b0;
                                        dd.bm_interrupt <= 1'b0;
                                    end
                                end

                                R_SEC_SIZ: begin
                                    dd.sector_size <= bus.wdata[7:0];
                                end

                                R_SEC_INFO: begin
                                    dd.sectors_in_block <= bus.wdata[15:8];
                                    dd.sector_size_full <= bus.wdata[7:0];
                                end
                            endcase
                        end
                    end
                end

                S_WAIT: begin
                    state <= S_IDLE;
                end
            endcase
        end
    end

endmodule


module n64_dd_sector_buffer (
    if_system.sys sys,
    if_dd.sector_buffer dd
);

    logic [5:0] sector_address;
    logic [31:0] sector_buffer [0:63];
    logic [15:0] sector_high_buffer;
    logic sector_write;
    logic [31:0] sector_wdata;

    always_comb begin
        sector_address = 6'd0;
        sector_write = 1'b0;
        sector_wdata = 32'd0;

        if (dd.n64_sector_address_valid) begin
            sector_address = dd.n64_sector_address[6:1];
        end else if (dd.cpu_sector_address_valid) begin
            sector_address = dd.cpu_sector_address;
        end

        if (dd.n64_sector_write && dd.n64_sector_address[0]) begin
            sector_write = 1'b1;
            sector_wdata = {sector_high_buffer, dd.n64_sector_wdata};
        end else if (dd.cpu_sector_write) begin
            sector_write = 1'b1;
            sector_wdata = dd.cpu_sector_wdata;
        end
    end

    always_ff @(posedge sys.clk) begin
        if (dd.n64_sector_write && !dd.n64_sector_address[0]) begin
            sector_high_buffer <= dd.n64_sector_wdata;
        end
    end

    always_ff @(posedge sys.clk) begin
        dd.sector_rdata <= sector_buffer[sector_address];
        if (sector_write) begin
            sector_buffer[sector_address] <= sector_wdata;
        end
    end

endmodule
