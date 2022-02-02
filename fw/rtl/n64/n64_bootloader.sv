module n64_bootloader (
    if_system.sys sys,
    if_n64_bus bus,
    if_config.flash cfg,
    if_flash.flash flash
);

    typedef enum bit [0:0] { 
        S_IDLE,
        S_WAIT
    } e_state;

    typedef enum bit [0:0] { 
        T_N64,
        T_CPU
    } e_source_request;

    e_state state;
    e_source_request source_request;

    logic request;
    logic ack;
    logic write;
    logic [31:0] address;
    logic [31:0] wdata;
    logic [31:0] rdata;

    always_ff @(posedge sys.clk) begin
        if (sys.reset) begin
            state <= S_IDLE;
            request <= 1'b0;
        end else begin
            case (state)
                S_IDLE: begin
                    if (bus.request || flash.request) begin
                        state <= S_WAIT;
                        request <= 1'b1;
                        if (bus.request) begin
                            write <= 1'b0;
                            address <= bus.address;
                            wdata <= bus.wdata;
                            source_request <= T_N64;
                        end else if (flash.request) begin
                            write <= flash.write;
                            address <= flash.address;
                            wdata <= flash.wdata;
                            source_request <= T_CPU;
                        end
                    end
                end

                S_WAIT: begin
                    if (ack) begin
                        state <= S_IDLE;
                        request <= 1'b0;
                    end
                end
            endcase
        end
    end

    always_comb begin
        bus.ack = source_request == T_N64 && ack;
        bus.rdata = 16'd0;
        if (bus.ack && bus.address < 32'h00010000) begin
            if (bus.address[1]) bus.rdata = {rdata[23:16], rdata[31:24]};
            else bus.rdata = {rdata[7:0], rdata[15:8]};
        end

        flash.ack = source_request == T_CPU && ack;
        flash.rdata = 32'd0;
        if (flash.ack) begin
            flash.rdata = rdata;
        end
    end

    vendor_flash vendor_flash_inst (
        .clk(sys.clk),
        .reset(sys.reset),

        .erase_start(cfg.flash_erase_start),
        .erase_busy(cfg.flash_erase_busy),
        .wp_enable(cfg.flash_wp_enable),
        .wp_disable(cfg.flash_wp_disable),

        .request(request),
        .ack(ack),
        .write(write),
        .address(address),
        .wdata(wdata),
        .rdata(rdata)
    );

endmodule
