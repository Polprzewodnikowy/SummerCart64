module cpu_cfg (
    if_system.sys sys,
    if_cpu_bus bus,
    if_config.cpu cfg
);

    logic skip_bootloader;
    logic trigger_reconfiguration;
    logic [15:0] isv_current_rd_ptr;

    typedef enum bit [3:0] { 
        R_SCR,
        R_DDIPL_OFFSET,
        R_SAVE_OFFSET,
        R_COMMAND,
        R_DATA_0,
        R_DATA_1,
        R_VERSION,
        R_RECONFIGURE,
        R_ISV_OFFSET,
        R_ISV_RD_PTR
    } e_reg_id;

    const logic [31:0] RECONFIGURE_MAGIC = 32'h52535446;

    always_ff @(posedge sys.clk) begin
        bus.ack <= 1'b0;
        if (bus.request) begin
            bus.ack <= 1'b1;
        end
    end

    always_comb begin
        bus.rdata = 32'd0;
        if (bus.ack) begin
            case (bus.address[5:2])
                R_SCR: bus.rdata = {
                    cfg.cpu_ready,
                    cfg.cpu_busy,
                    1'b0,
                    cfg.cmd_error,
                    20'd0,
                    cfg.isv_enabled,
                    skip_bootloader,
                    cfg.flashram_enabled,
                    cfg.sram_banked,
                    cfg.sram_enabled,
                    cfg.dd_enabled,
                    cfg.sdram_writable,
                    cfg.sdram_switch
                };
                R_DDIPL_OFFSET: bus.rdata = {6'd0, cfg.ddipl_offset};
                R_SAVE_OFFSET: bus.rdata = {6'd0, cfg.save_offset};
                R_COMMAND: bus.rdata = {24'd0, cfg.cmd};
                R_DATA_0: bus.rdata = cfg.data[0];
                R_DATA_1: bus.rdata = cfg.data[1];
                R_VERSION: bus.rdata = sc64::SC64_VER;
                R_RECONFIGURE: bus.rdata = RECONFIGURE_MAGIC;
                R_ISV_OFFSET: bus.rdata = {6'd0, cfg.isv_offset};
                R_ISV_RD_PTR: bus.rdata = {isv_current_rd_ptr, cfg.isv_rd_ptr};
                default: bus.rdata = 32'd0;
            endcase
        end
    end

    always_comb begin
        cfg.wdata = bus.wdata;
        cfg.data_write = 2'b00;
        if (bus.request && (&bus.wmask)) begin
            cfg.data_write[0] = bus.address[4:2] == R_DATA_0;
            cfg.data_write[1] = bus.address[4:2] == R_DATA_1;
        end
    end

    always_ff @(posedge sys.clk) begin
        if (sys.reset) begin
            cfg.cpu_ready <= 1'b0;
            cfg.cpu_busy <= 1'b0;
            cfg.cmd_error <= 1'b0;
            cfg.sdram_switch <= 1'b0;
            cfg.sdram_writable <= 1'b0;
            cfg.dd_enabled <= 1'b0;
            cfg.sram_enabled <= 1'b0;
            cfg.sram_banked <= 1'b0;
            cfg.flashram_enabled <= 1'b0;
            cfg.isv_enabled <= 1'b0;
            cfg.ddipl_offset <= 26'h3BE_0000;
            cfg.save_offset <= 26'h3FE_0000;
            cfg.isv_offset <= 26'h3FF_0000;
            skip_bootloader <= 1'b0;
            trigger_reconfiguration <= 1'b0;
        end else begin
            if (sys.n64_soft_reset) begin
                cfg.sdram_switch <= skip_bootloader;
                cfg.sdram_writable <= 1'b0;
                isv_current_rd_ptr <= 16'd0;
            end
            if (cfg.cmd_request) begin
                cfg.cpu_busy <= 1'b1;
            end
            if (bus.request) begin
                case (bus.address[5:2])
                    R_SCR: begin
                        if (bus.wmask[3]) begin
                            {
                                cfg.cpu_ready,
                                cfg.cpu_busy,
                                cfg.cmd_error
                            } <= {bus.wdata[31:30], bus.wdata[28]};
                        end
                        if (bus.wmask[0]) begin
                            {
                                cfg.isv_enabled,
                                skip_bootloader,
                                cfg.flashram_enabled,
                                cfg.sram_banked,
                                cfg.sram_enabled,
                                cfg.dd_enabled,
                                cfg.sdram_writable,
                                cfg.sdram_switch
                            } <= bus.wdata[7:0];
                        end
                    end

                    R_DDIPL_OFFSET: begin
                        if (&bus.wmask) begin
                            cfg.ddipl_offset <= bus.wdata[25:0];
                        end
                    end

                    R_SAVE_OFFSET: begin
                        if (&bus.wmask) begin
                            cfg.save_offset <= bus.wdata[25:0];
                        end
                    end

                    R_RECONFIGURE: begin
                        if (&bus.wmask && bus.wdata == RECONFIGURE_MAGIC) begin
                            trigger_reconfiguration <= 1'b1;
                        end
                    end

                    R_ISV_OFFSET: begin
                        if (&bus.wmask) begin
                            cfg.isv_offset <= bus.wdata[25:0];
                        end
                    end

                    R_ISV_RD_PTR: begin
                        if (&bus.wmask[3:2]) begin
                            isv_current_rd_ptr <= bus.wdata[31:16];
                        end
                    end
                endcase
            end
        end
    end

    logic [1:0] ru_clk;
    logic ru_rconfig;
    logic ru_regout;

    always_ff @(posedge sys.clk) begin
        if (sys.reset) begin
            ru_clk <= 2'd0;
            ru_rconfig <= 1'b0;
        end else begin
            ru_clk <= ru_clk + 1'd1;

            if (ru_clk == 2'd1) begin
                ru_rconfig <= trigger_reconfiguration;
            end
        end
    end

    fiftyfivenm_rublock fiftyfivenm_rublock_inst (
        .clk(ru_clk[1]),
        .shiftnld(1'b0),
        .captnupdt(1'b0),
        .regin(1'b0),
        .rsttimer(1'b0),
        .rconfig(ru_rconfig),
        .regout(ru_regout)
    );

endmodule
