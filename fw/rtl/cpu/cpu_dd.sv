module cpu_dd (
    if_system.sys sys,
    if_cpu_bus bus,
    if_dd.cpu dd
);

    const bit [8:0] M_SECTOR_BUFFER    = 9'h100;

    logic bm_ack;
    logic [31:0] seek_timer;

    typedef enum bit [2:0] {
        R_SCR,
        R_CMD_DATA,
        R_HEAD_TRACK,
        R_SECTOR_INFO,
        R_DRIVE_ID,
        R_SEEK_TIMER
    } e_reg_id;

    always_ff @(posedge sys.clk) begin
        bus.ack <= 1'b0;
        if (bus.request) begin
            bus.ack <= 1'b1;
        end
    end

    always_comb begin
        bus.rdata = 32'd0;
        if (bus.ack) begin
            if (bus.address[8] == M_SECTOR_BUFFER[8]) begin
                bus.rdata = {
                    dd.sector_rdata[7:0],
                    dd.sector_rdata[15:8],
                    dd.sector_rdata[23:16],
                    dd.sector_rdata[31:24]
                };
            end else begin                
                case (bus.address[5:2])
                    R_SCR: bus.rdata = {
                        14'd0,
                        bm_ack,
                        dd.bm_micro_error,
                        dd.bm_transfer_c2,
                        dd.bm_transfer_data,
                        dd.bm_transfer_blocks,
                        dd.bm_transfer_mode,
                        1'b0,
                        dd.bm_stop_pending,
                        1'b0,
                        dd.bm_start_pending,
                        dd.disk_changed,
                        dd.disk_inserted,
                        1'b0,
                        dd.bm_pending,
                        1'b0,
                        dd.cmd_pending,
                        1'b0,
                        dd.hard_reset
                    };
                    R_CMD_DATA: bus.rdata = {8'd0, dd.cmd, dd.data};
                    R_HEAD_TRACK: bus.rdata = {18'd0, dd.index_lock, dd.head_track};
                    R_SECTOR_INFO: bus.rdata = {
                        dd.sectors_in_block,
                        dd.sector_size_full,
                        dd.sector_size,
                        dd.sector_num
                    };
                    R_DRIVE_ID: bus.rdata = {dd.drive_id};
                    R_SEEK_TIMER: bus.rdata = seek_timer;
                    default: bus.rdata = 32'd0;
                endcase
            end
        end
    end

    always_comb begin
        dd.sector_address = bus.address[7:2];
        dd.sector_address_valid = bus.request && bus.address[8] == M_SECTOR_BUFFER[8];
        dd.sector_write = (&bus.wmask) && dd.sector_address_valid;
        dd.sector_wdata = {bus.wdata[7:0], bus.wdata[15:8], bus.wdata[23:16], bus.wdata[31:24]};
    end

    always_ff @(posedge sys.clk) begin
        dd.hard_reset_clear <= 1'b0;
        dd.cmd_ready <= 1'b0;
        dd.bm_start_clear <= 1'b0;
        dd.bm_stop_clear <= 1'b0;
        dd.bm_clear <= 1'b0;
        dd.bm_ready <= 1'b0;

        if (dd.bm_interrupt_ack) begin
            bm_ack <= 1'b1;
        end

        if (!(&seek_timer)) begin
            seek_timer <= seek_timer + 1'd1;
        end

        if (sys.reset) begin
            bm_ack <= 1'b0;
        end else begin
            if (bus.request && (!bus.address[8])) begin
                case (bus.address[4:2])
                    R_SCR: if (&bus.wmask) begin
                        if (bus.wdata[20]) begin
                            seek_timer <= 32'd0;
                        end
                        dd.bm_clear <= bus.wdata[19];
                        if (bus.wdata[18]) begin
                            bm_ack <= 1'b0;
                        end
                        dd.bm_micro_error <= bus.wdata[16];
                        dd.bm_transfer_c2 <= bus.wdata[15];
                        dd.bm_transfer_data <= bus.wdata[14];
                        dd.bm_stop_clear <= bus.wdata[11];
                        dd.bm_start_clear <= bus.wdata[9];
                        dd.disk_changed <= bus.wdata[7];
                        dd.disk_inserted <= bus.wdata[6];
                        dd.bm_ready <= bus.wdata[5];
                        dd.cmd_ready <= bus.wdata[3];
                        dd.hard_reset_clear <= bus.wdata[1];
                    end

                    R_CMD_DATA: if (&bus.wmask[1:0]) begin
                        dd.cmd_data <= bus.wdata[15:0];
                    end

                    R_HEAD_TRACK: if (&bus.wmask[1:0]) begin
                        {dd.index_lock, dd.head_track} <= bus.wdata[13:0];
                    end

                    R_DRIVE_ID: if (&bus.wmask[1:0]) begin
                        dd.drive_id <= bus.wdata[15:0];
                    end
                endcase
            end
        end
    end

endmodule
