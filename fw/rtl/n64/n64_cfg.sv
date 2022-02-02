module n64_cfg (
    if_system sys,
    if_n64_bus bus,
    if_config.n64 cfg
);

    typedef enum bit [2:0] { 
        R_SR,
        R_COMMAND,
        R_DATA_0_H,
        R_DATA_0_L,
        R_DATA_1_H,
        R_DATA_1_L,
        R_VERSION_H,
        R_VERSION_L
    } e_reg_id;

    typedef enum bit [3:0] { 
        R_ISV_ID_H = 4'h0,
        R_ISV_ID_L = 4'h1,
        R_ISV_RD_PTR = 4'hB
    } e_reg_isv_id;

    typedef enum bit [0:0] { 
        S_IDLE,
        S_WAIT
    } e_state;

    e_state state;

    logic [31:0] isv_id;

    always_comb begin
        bus.rdata = 16'd0;
        if (bus.ack) begin
            if (bus.address[15:14] == 2'b00) begin
                case (bus.address[3:1])
                    R_SR: bus.rdata = {
                        cfg.cpu_ready,
                        cfg.cpu_busy,
                        1'b0,
                        cfg.cmd_error,
                        12'd0
                    };
                    R_COMMAND: bus.rdata = {8'd0, cfg.cmd};
                    R_DATA_0_H: bus.rdata = cfg.data[0][31:16];
                    R_DATA_0_L: bus.rdata = cfg.data[0][15:0];
                    R_DATA_1_H: bus.rdata = cfg.data[1][31:16];
                    R_DATA_1_L: bus.rdata = cfg.data[1][15:0];
                    R_VERSION_H: bus.rdata = sc64::SC64_VER[31:16];
                    R_VERSION_L: bus.rdata = sc64::SC64_VER[15:0];
                    default: bus.rdata = 16'd0;
                endcase
            end else if (bus.address[15:14] == 2'b11) begin
                case (bus.address[4:1])
                    R_ISV_ID_H: bus.rdata = isv_id[31:16];
                    R_ISV_ID_L: bus.rdata = isv_id[15:0];
                    R_ISV_RD_PTR: bus.rdata = cfg.isv_rd_ptr;
                    default: bus.rdata = 16'd0;
                endcase
            end
        end
    end

    always_ff @(posedge sys.clk) begin
        bus.ack <= 1'b0;
        cfg.cmd_request <= 1'b0;

        if (cfg.data_write[0]) cfg.data[0] <= cfg.wdata;
        if (cfg.data_write[1]) cfg.data[1] <= cfg.wdata;

        if (sys.n64_soft_reset) begin
            cfg.isv_rd_ptr <= 16'd0;
        end

        if (sys.reset) begin
            state <= S_IDLE;
        end else begin
            case (state)
                S_IDLE: begin
                    if (bus.request) begin
                        state <= S_WAIT;
                        bus.ack <= 1'b1;
                        if (bus.write) begin
                            if (bus.address[15:14] == 2'b00) begin
                                case (bus.address[3:1])
                                    R_COMMAND: begin
                                        cfg.cmd <= bus.wdata[7:0];
                                        cfg.cmd_request <= 1'b1;
                                    end
                                    R_DATA_0_H: cfg.data[0][31:16] <= bus.wdata;
                                    R_DATA_0_L: cfg.data[0][15:0] <= bus.wdata;
                                    R_DATA_1_H: cfg.data[1][31:16] <= bus.wdata;
                                    R_DATA_1_L: cfg.data[1][15:0] <= bus.wdata;
                                endcase
                            end else if (bus.address[15:14] == 2'b11) begin
                                case (bus.address[4:1])
                                    R_ISV_ID_H: isv_id[31:16] <= bus.wdata;
                                    R_ISV_ID_L: isv_id[15:0] <= bus.wdata;
                                    R_ISV_RD_PTR: cfg.isv_rd_ptr <= bus.wdata;
                                endcase
                            end
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
