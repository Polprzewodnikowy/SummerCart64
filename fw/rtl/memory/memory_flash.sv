module memory_flash (
    if_system.sys sys,
    if_n64_bus bus
);

    logic flash_enable;
    logic flash_request;
    logic flash_busy;
    logic flash_ack;
    logic [31:0] flash_rdata;

    logic dummy_ack;

    always_comb begin
        flash_enable = bus.address < 32'h10016800;

        bus.ack = flash_ack | dummy_ack;

        bus.rdata = 16'd0;
        if (bus.ack && flash_enable) begin
            if (bus.address[1]) bus.rdata = {flash_rdata[23:16], flash_rdata[31:24]};
            else bus.rdata = {flash_rdata[7:0], flash_rdata[15:8]};
        end
    end

    always_ff @(posedge sys.clk) begin
        dummy_ack <= 1'b0;
        if (sys.reset) begin
            flash_request <= 1'b0;
        end else begin
            if (!flash_busy) begin
                flash_request <= 1'b0;
            end
            if (bus.request) begin
                if (flash_enable) begin
                    flash_request <= 1'b1;
                end else begin
                    dummy_ack <= 1'b1;
                end
            end
        end
    end

    intel_flash intel_flash_inst (
        .clock(sys.clk),
        .reset_n(~sys.reset),
        .avmm_data_addr(bus.address[31:2]),
        .avmm_data_read(flash_request || (bus.request && flash_enable)),
        .avmm_data_readdata(flash_rdata),
        .avmm_data_waitrequest(flash_busy),
        .avmm_data_readdatavalid(flash_ack),
        .avmm_data_burstcount(2'd1)
    );

endmodule
