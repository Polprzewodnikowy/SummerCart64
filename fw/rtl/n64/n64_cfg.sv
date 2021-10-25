module n64_cfg (
    if_system sys,
    if_n64_bus bus,
    if_config.n64 cfg
);

    typedef enum bit [3:0] { 
        R_SR,
        R_COMMAND,
        R_DATA_0_H,
        R_DATA_0_L,
        R_DATA_1_H,
        R_DATA_1_L,
        R_VERSION_H,
        R_VERSION_L
    } e_reg_id;

    typedef enum bit [0:0] { 
        S_IDLE,
        S_WAIT
    } e_state;

    e_state state;

    always_comb begin
        bus.rdata = 16'd0;
        if (bus.ack) begin
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
            endcase
        end
    end

    always_ff @(posedge sys.clk) begin
        bus.ack <= 1'b0;
        cfg.cmd_request <= 1'b0;

        if (cfg.data_write[0]) cfg.data[0] <= cfg.wdata;
        if (cfg.data_write[1]) cfg.data[1] <= cfg.wdata;

        if (sys.reset) begin
            state <= S_IDLE;
        end else begin
            case (state)
                S_IDLE: begin
                    if (bus.request) begin
                        state <= S_WAIT;
                        bus.ack <= 1'b1;
                        if (bus.write) begin
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
