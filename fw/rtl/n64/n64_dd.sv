module n64_dd (
    input clk,
    input reset,

    n64_reg_bus.dd reg_bus,

    n64_scb.dd n64_scb,

    output logic irq
);

    assign irq = 1'b0;

    // const bit [10:0] M_C2_BUFFER        = 11'h000;
    // const bit [10:0] M_SECTOR_BUFFER    = 11'h400;

    // typedef enum bit [10:0] {
    //     R_DATA      = 11'h500,
    //     R_CMD_SR    = 11'h508,
    //     R_TRK_CUR   = 11'h50C,
    //     R_BM_SCR    = 11'h510,
    //     R_RESET     = 11'h520,
    //     R_SEC_SIZ   = 11'h528,
    //     R_SEC_INFO  = 11'h530,
    //     R_ID        = 11'h540
    // } e_reg_id;

    // typedef enum bit [3:0] {
    //     BM_CONTROL_START_BUFFER_MANAGER     = 4'd15,
    //     BM_CONTROL_BUFFER_MANAGER_MODE      = 4'd14,
    //     BM_CONTROL_BUFFER_MANAGER_RESET     = 4'd12,
    //     BM_CONTROL_BLOCK_TRANSFER           = 4'd9,
    //     BM_CONTROL_MECHANIC_INTERRUPT_RESET = 4'd8
    // } e_bm_control_id;

    // // typedef enum bit [0:0] {
    // //     S_IDLE,
    // //     S_WAIT
    // // } e_state;

    // // e_state state;

    // // always_comb begin
    // //     dd.sector_address = bus.address[7:1];
    // //     dd.sector_address_valid = bus.request && bus.address[11:8] == M_SECTOR_BUFFER[11:8];
    // //     dd.sector_write = bus.write && dd.sector_address_valid;
    // //     dd.sector_wdata = bus.wdata;
    // // end

    // always_comb begin
    //     bus.rdata = 16'd0;
    //     if (bus.ack) begin
    //         if (bus.address[10:8] == M_SECTOR_BUFFER[10:8]) begin
    //             if (bus.address[1]) begin
    //                 bus.rdata = dd.sector_rdata[15:0];
    //             end else begin
    //                 bus.rdata = dd.sector_rdata[31:16];
    //             end
    //         end else begin
    //             case (bus.address[10:0])
    //                 R_DATA: bus.rdata = dd.data;
    //                 R_CMD_SR: bus.rdata = {
    //                     1'b0,
    //                     dd.bm_transfer_data,
    //                     1'b0,
    //                     dd.bm_transfer_c2,
    //                     1'b0,
    //                     dd.bm_interrupt,
    //                     dd.cmd_interrupt,
    //                     dd.disk_inserted,
    //                     dd.cmd_pending,
    //                     dd.hard_reset,
    //                     1'b0,
    //                     1'b0,
    //                     1'b0,
    //                     1'b0,
    //                     1'b0,
    //                     dd.disk_changed
    //                 };
    //                 R_TRK_CUR: bus.rdata = {1'd0, {2{dd.index_lock}}, dd.head_track};
    //                 R_BM_SCR: bus.rdata = {6'd0, dd.bm_micro_error, 9'd0};
    //                 R_ID: bus.rdata = {dd.drive_id};
    //                 default: bus.rdata = 16'd0;
    //             endcase
    //         end
    //     end
    // end

    // always_comb begin
    //     reg_bus.rdata = 16'd0;
    //     if (reg_bus.address[10:8] == M_SECTOR_BUFFER[10:8]) begin
    //     end else begin
    //         case (reg_bus.address[10:0])
    //             R_DATA: reg_bus.rdata = dd.data;
    //             R_CMD_SR: reg_bus.rdata = {
    //                 1'b0,
    //                 dd.bm_transfer_data,
    //                 1'b0,
    //                 dd.bm_transfer_c2,
    //                 1'b0,
    //                 dd.bm_interrupt,
    //                 dd.cmd_interrupt,
    //                 dd.disk_inserted,
    //                 dd.cmd_pending,
    //                 dd.hard_reset,
    //                 1'b0,
    //                 1'b0,
    //                 1'b0,
    //                 1'b0,
    //                 1'b0,
    //                 dd.disk_changed
    //             };
    //             R_TRK_CUR: reg_bus.rdata = {1'd0, {2{dd.index_lock}}, dd.head_track};
    //             R_BM_SCR: reg_bus.rdata = {6'd0, dd.bm_micro_error, 9'd0};
    //             R_ID: reg_bus.rdata = {dd.drive_id};
    //             default: reg_bus.rdata = 16'd0;
    //         endcase
    //     end
    // end

    // always_ff @(posedge sys.clk) begin
    //     bus.ack <= 1'b0;
    //     dd.bm_interrupt_ack <= 1'b0;

    //     if (dd.hard_reset_clear) begin
    //         dd.hard_reset <= 1'b0;
    //     end
    //     if (dd.cmd_ready) begin
    //         dd.data <= dd.cmd_data;
    //         dd.cmd_pending <= 1'b0;
    //         dd.cmd_interrupt <= 1'b1;
    //     end
    //     if (dd.bm_start_clear) begin
    //         dd.bm_start_pending <= 1'b0;
    //     end
    //     if (dd.bm_stop_clear) begin
    //         dd.bm_stop_pending <= 1'b0;
    //     end
    //     if (dd.bm_clear) begin
    //         dd.bm_pending <= 1'b0;
    //     end
    //     if (dd.bm_ready) begin
    //         dd.bm_interrupt <= 1'b1;
    //     end
    //     if (bus.real_address == (M_C2_BUFFER + ({dd.sector_size[7:1], 1'b0} * 3'd4)) && bus.read_op) begin
    //         dd.bm_pending <= 1'b1;
    //     end
    //     if (bus.real_address == (M_SECTOR_BUFFER + {dd.sector_size[7:1], 1'b0}) && (bus.read_op || bus.write_op)) begin
    //         dd.bm_pending <= 1'b1;
    //     end
    //     if (bus.real_address == (M_BASE + R_CMD_SR) && bus.read_op) begin
    //         dd.bm_interrupt <= 1'b0;
    //         dd.bm_interrupt_ack <= 1'b1;
    //     end

    //     if (sys.reset || sys.n64_hard_reset) begin
    //         dd.hard_reset <= 1'b1;
    //         dd.cmd_pending <= 1'b0;
    //         dd.cmd_interrupt <= 1'b0;
    //         dd.bm_start_pending <= 1'b0;
    //         dd.bm_stop_pending <= 1'b0;
    //         dd.bm_pending <= 1'b0;
    //         dd.bm_interrupt <= 1'b0;
    //         state <= S_IDLE;
    //     end else begin
    //         case (state)
    //             S_IDLE: begin
    //                 if (bus.request) begin
    //                     state <= S_WAIT;
    //                     bus.ack <= 1'b1;
    //                     if (bus.write) begin
    //                         case (bus.address[10:0])
    //                             R_DATA: begin
    //                                 dd.data <= bus.wdata;
    //                             end

    //                             R_CMD_SR: begin
    //                                 dd.cmd <= bus.wdata[7:0];
    //                                 dd.cmd_pending <= 1'b1;
    //                             end

    //                             R_BM_SCR: begin
    //                                 dd.sector_num <= bus.wdata[7:0];
    //                                 if (bus.wdata[BM_CONTROL_START_BUFFER_MANAGER]) begin
    //                                     dd.bm_start_pending <= 1'b1;
    //                                     dd.bm_stop_pending <= 1'b0;
    //                                     dd.bm_transfer_mode <= bus.wdata[BM_CONTROL_BUFFER_MANAGER_MODE];
    //                                     dd.bm_transfer_blocks <= bus.wdata[BM_CONTROL_BLOCK_TRANSFER];
    //                                 end
    //                                 if (bus.wdata[BM_CONTROL_BUFFER_MANAGER_RESET]) begin
    //                                     dd.bm_start_pending <= 1'b0;
    //                                     dd.bm_stop_pending <= 1'b1;
    //                                     dd.bm_transfer_mode <= 1'b0;
    //                                     dd.bm_transfer_blocks <= 1'b0;
    //                                     dd.bm_pending <= 1'b0;
    //                                     dd.bm_interrupt <= 1'b0;
    //                                 end
    //                                 if (bus.wdata[BM_CONTROL_MECHANIC_INTERRUPT_RESET]) begin
    //                                     dd.cmd_interrupt <= 1'b0;
    //                                 end
    //                             end

    //                             R_RESET: begin
    //                                 if (bus.wdata == 16'hAAAA) begin
    //                                     dd.hard_reset <= 1'b1;
    //                                     dd.cmd_pending <= 1'b0;
    //                                     dd.cmd_interrupt <= 1'b0;
    //                                     dd.bm_start_pending <= 1'b0;
    //                                     dd.bm_stop_pending <= 1'b0;
    //                                     dd.bm_pending <= 1'b0;
    //                                     dd.bm_interrupt <= 1'b0;
    //                                 end
    //                             end

    //                             R_SEC_SIZ: begin
    //                                 dd.sector_size <= bus.wdata[7:0];
    //                             end

    //                             R_SEC_INFO: begin
    //                                 dd.sectors_in_block <= bus.wdata[15:8];
    //                                 dd.sector_size_full <= bus.wdata[7:0];
    //                             end
    //                         endcase
    //                     end
    //                 end
    //             end

    //             S_WAIT: begin
    //                 state <= S_IDLE;
    //             end
    //         endcase
    //     end
    // end


endmodule
