module n64_cfg (
    if_system sys,
    if_n64_bus bus,
    if_config.n64 cfg
);

    typedef enum bit [0:0] { 
        S_IDLE,
        S_WAIT
    } e_state;

    e_state state;

    always_comb begin
        bus.rdata = 16'd0;
        if (bus.ack) begin
            case (bus.address[4:1])
                0: bus.rdata = {cfg.cpu_bootstrapped, cfg.cpu_busy, 14'd0};
                // ...
                3: bus.rdata = {8'd0, cfg.command};
                4: bus.rdata = cfg.arg[0][31:16];
                5: bus.rdata = cfg.arg[0][15:0];
                6: bus.rdata = cfg.arg[1][31:16];
                7: bus.rdata = cfg.arg[1][15:0];
                8: bus.rdata = cfg.response[31:16];
                9: bus.rdata = cfg.response[15:0];
                10: bus.rdata = cfg.arg[0][31:16];
                11: bus.rdata = cfg.arg[0][15:0];
            endcase
        end
    end

    always_ff @(posedge sys.clk) begin
        bus.ack <= 1'b0;
        cfg.request <= 1'b0;
        cfg.boot_write <= 1'b0;

        if (sys.reset) begin
            state <= S_IDLE;
        end else begin
            case (state)
                S_IDLE: begin
                    if (bus.request) begin
                        state <= S_WAIT;
                        bus.ack <= 1'b1;
                        if (bus.write) begin
                            case (bus.address[4:1])
                                // ...
                                3: begin
                                    cfg.command <= bus.wdata[7:0];
                                    cfg.request <= 1'b1;
                                end
                                4: cfg.arg[0][31:16] <= bus.wdata;
                                5: cfg.arg[0][15:0] <= bus.wdata;
                                6: cfg.arg[1][31:16] <= bus.wdata;
                                7: cfg.arg[1][15:0] <= bus.wdata;
                                // ...
                                10: cfg.arg[0][31:16] <= bus.wdata;
                                11: begin
                                    cfg.arg[0][15:0] <= bus.wdata;
                                    cfg.boot_write <= 1'b1;
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
