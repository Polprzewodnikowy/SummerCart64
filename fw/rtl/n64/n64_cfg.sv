module n64_cfg (
    input clk,
    input reset,

    n64_reg_bus.cfg reg_bus,

    n64_scb.cfg n64_scb,

    output logic irq
);

    typedef enum bit [2:0] { 
        REG_STATUS,
        REG_COMMAND,
        REG_DATA_0_H,
        REG_DATA_0_L,
        REG_DATA_1_H,
        REG_DATA_1_L,
        REG_VERSION_H,
        REG_VERSION_L
    } e_reg;

    logic cfg_error;

    always_comb begin
        reg_bus.rdata = 16'd0;
        if (reg_bus.address[16] && (reg_bus.address[15:4] == 12'd0)) begin
            case (reg_bus.address[3:1])
                REG_STATUS: reg_bus.rdata = {
                    n64_scb.cfg_pending,
                    cfg_error,
                    14'd0
                };
                REG_DATA_0_H: reg_bus.rdata = n64_scb.cfg_wdata[0][31:16];
                REG_DATA_0_L: reg_bus.rdata = n64_scb.cfg_wdata[0][15:0];
                REG_DATA_1_H: reg_bus.rdata = n64_scb.cfg_wdata[1][31:16];
                REG_DATA_1_L: reg_bus.rdata = n64_scb.cfg_wdata[1][15:0];
                REG_VERSION_H: reg_bus.rdata = n64_scb.cfg_version[31:16];
                REG_VERSION_L: reg_bus.rdata = n64_scb.cfg_version[15:0];
            endcase
        end
    end

    always_ff @(posedge clk) begin
        if (reset || n64_scb.n64_reset) begin
            n64_scb.cfg_pending <= 1'b0;
            n64_scb.cfg_cmd <= 8'h00;
            irq <= 1'b0;
            cfg_error <= 1'b0;
        end else begin
            if (n64_scb.cfg_done) begin
                n64_scb.cfg_pending <= 1'b0;
                cfg_error <= n64_scb.cfg_error;
            end

            if (n64_scb.cfg_irq) begin
                irq <= 1'b1;
            end

            if (reg_bus.write) begin
                if (reg_bus.address[16] && (reg_bus.address[15:4] == 12'd0)) begin
                    case (reg_bus.address[3:1])
                        REG_COMMAND: begin
                            n64_scb.cfg_pending <= 1'b1;
                            n64_scb.cfg_cmd <= reg_bus.wdata[7:0];
                            cfg_error <= 1'b0;
                        end
                        REG_DATA_0_H: n64_scb.cfg_rdata[0][31:16] <= reg_bus.wdata;
                        REG_DATA_0_L: n64_scb.cfg_rdata[0][15:0] <= reg_bus.wdata;
                        REG_DATA_1_H: n64_scb.cfg_rdata[1][31:16] <= reg_bus.wdata;
                        REG_DATA_1_L: n64_scb.cfg_rdata[1][15:0] <= reg_bus.wdata;
                        REG_VERSION_L: irq <= 1'b0;
                    endcase
                end
            end
        end
    end

endmodule
