module cpu_cfg (
    if_system.sys sys,
    if_cpu_bus bus,
    if_config.cpu cfg
);

    typedef enum bit [2:0] { 
        R_SCR,
        R_DD_OFFSET,
        R_SAVE_OFFSET,
        R_COMMAND,
        R_ARG_1,
        R_ARG_2,
        R_RESPONSE,
        R_BOOTSTRAP
    } e_reg_id;

    logic bootstrap_pending;

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
                    cfg.cpu_bootstrapped,
                    cfg.cpu_busy,
                    bootstrap_pending,
                    24'd0,
                    cfg.flashram_enabled,
                    cfg.sram_enabled,
                    cfg.dd_enabled,
                    cfg.sdram_writable,
                    cfg.sdram_switch
                };
                R_DD_OFFSET: bus.rdata = {6'd0, cfg.dd_offset};
                R_SAVE_OFFSET: bus.rdata = {6'd0, cfg.save_offset};
                R_COMMAND: bus.rdata = {24'd0, cfg.command};
                R_ARG_1: bus.rdata = cfg.arg[0];
                R_ARG_2: bus.rdata = cfg.arg[1];
                R_RESPONSE: bus.rdata = cfg.response;
                R_BOOTSTRAP: bus.rdata = cfg.arg[0];
            endcase
        end
    end

    always_ff @(posedge sys.clk) begin
        if (sys.reset) begin
            cfg.cpu_bootstrapped <= 1'b0;
            cfg.cpu_busy <= 1'b0;
            cfg.sdram_switch <= 1'b0;
            cfg.sdram_writable <= 1'b0;
            cfg.dd_enabled <= 1'b0;
            cfg.sram_enabled <= 1'b0;
            cfg.flashram_enabled <= 1'b0;
            cfg.dd_offset <= 26'h3BE_0000;
            cfg.save_offset <= 26'h3FE_0000;
            bootstrap_pending <= 1'b0;
        end else begin
            if (sys.n64_soft_reset) begin
                cfg.sdram_switch <= 1'b0;
            end
            if (cfg.request) begin
                cfg.cpu_busy <= 1'b1;
            end
            if (cfg.boot_write) begin
                bootstrap_pending <= 1'b1;
            end
            if (bus.request) begin
                case (bus.address[4:2])
                    R_SCR: begin
                        if (bus.wmask[3]) begin
                            cfg.cpu_bootstrapped <= bus.wdata[31];
                        end
                        if (bus.wmask[0]) begin
                            {
                                cfg.flashram_enabled,
                                cfg.sram_enabled,
                                cfg.dd_enabled,
                                cfg.sdram_writable,
                                cfg.sdram_switch
                            } <= bus.wdata[4:0];
                        end
                    end

                    R_DD_OFFSET: begin
                        if (&bus.wmask) begin
                            cfg.dd_offset <= bus.wdata[25:0];
                        end
                    end

                    R_SAVE_OFFSET: begin
                        if (&bus.wmask) begin
                            cfg.save_offset <= bus.wdata[25:0];
                        end
                    end

                    R_RESPONSE: begin
                        if (&bus.wmask) begin
                            cfg.cpu_busy <= 1'b0;
                            cfg.response <= bus.wdata;
                        end
                    end

                    R_BOOTSTRAP: begin
                        if (!(|bus.wmask)) begin
                            bootstrap_pending <= 1'b0;
                        end
                    end
                endcase
            end
        end
    end

endmodule
