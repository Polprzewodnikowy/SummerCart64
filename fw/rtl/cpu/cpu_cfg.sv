module cpu_cfg (
    if_system.sys sys,
    if_cpu_bus bus,
    if_config.cpu cfg
);

    logic skip_bootloader;
    logic enable_writes_on_reset;
    logic trigger_reconfiguration;

    typedef enum bit [2:0] { 
        R_SCR,
        R_COMMAND,
        R_DATA_0,
        R_DATA_1,
        R_VERSION,
        R_RECONFIGURE
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
            case (bus.address[4:2])
                R_SCR: bus.rdata = {
                    cfg.cpu_ready,
                    cfg.cpu_busy,
                    1'b0,
                    cfg.cmd_error,
                    2'd0,
                    cfg.flash_erase_busy,
                    1'd0,
                    16'd0,
                    enable_writes_on_reset,
                    skip_bootloader,
                    cfg.flashram_enabled,
                    cfg.sram_banked,
                    cfg.sram_enabled,
                    cfg.dd_enabled,
                    cfg.sdram_writable,
                    cfg.sdram_switch
                };
                R_COMMAND: bus.rdata = {24'd0, cfg.cmd};
                R_DATA_0: bus.rdata = cfg.data[0];
                R_DATA_1: bus.rdata = cfg.data[1];
                R_VERSION: bus.rdata = sc64::SC64_VER;
                R_RECONFIGURE: bus.rdata = RECONFIGURE_MAGIC;
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
        cfg.flash_erase_start <= 1'b0;
        cfg.flash_wp_enable <= 1'b0;
        cfg.flash_wp_disable <= 1'b0;

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
            skip_bootloader <= 1'b0;
            enable_writes_on_reset <= 1'b0;
            trigger_reconfiguration <= 1'b0;
        end else begin
            if (sys.n64_soft_reset) begin
                cfg.sdram_switch <= skip_bootloader;
                cfg.sdram_writable <= enable_writes_on_reset;
            end

            if (cfg.cmd_request) begin
                cfg.cpu_busy <= 1'b1;
            end

            if (bus.request) begin
                case (bus.address[4:2])
                    R_SCR: begin
                        if (bus.wmask[3]) begin
                            {
                                cfg.cpu_ready,
                                cfg.cpu_busy,
                                cfg.cmd_error,
                                cfg.flash_wp_disable,
                                cfg.flash_wp_enable,
                                cfg.flash_erase_start
                            } <= {bus.wdata[31:30], bus.wdata[28:26], bus.wdata[24]};
                        end
                        if (bus.wmask[0]) begin
                            {
                                enable_writes_on_reset,
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

                    R_RECONFIGURE: begin
                        if (&bus.wmask && bus.wdata == RECONFIGURE_MAGIC) begin
                            trigger_reconfiguration <= 1'b1;
                        end
                    end
                endcase
            end
        end
    end

    vendor_reconfigure vendor_reconfigure_inst (
        .clk(sys.clk),
        .reset(sys.reset),

        .trigger_reconfiguration(trigger_reconfiguration)
    );

endmodule
