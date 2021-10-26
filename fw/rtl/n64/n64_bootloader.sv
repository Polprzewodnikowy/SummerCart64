module n64_bootloader (
    if_system.sys sys,
    if_n64_bus bus,
    if_flash.memory flash
);

    logic mem_request;
    logic csr_ack;
    logic data_ack;
    logic write_ack;
    logic data_busy;
    logic mem_write;
    logic [31:0] mem_address;
    logic [31:0] csr_rdata;
    logic [31:0] data_rdata;
    logic [31:0] mem_wdata;

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

    always_ff @(posedge sys.clk) begin
        csr_ack <= 1'b0;
        write_ack <= 1'b0;

        if (sys.reset) begin
            state <= S_IDLE;
            mem_request <= 1'b0;
        end else begin
            case (state)
                S_IDLE: begin
                    if (bus.request || flash.request) begin
                        state <= S_WAIT;
                        mem_request <= 1'b1;
                        if (bus.request) begin
                            mem_write <= 1'b0;
                            mem_address <= bus.address;
                            mem_wdata <= bus.wdata;
                            source_request <= T_N64;
                        end else if (flash.request) begin
                            mem_write <= flash.write;
                            mem_address <= flash.address;
                            mem_wdata <= flash.wdata;
                            source_request <= T_CPU;
                        end
                    end
                end

                S_WAIT: begin
                    if (mem_address[27] && source_request != T_N64 && !csr_ack) begin
                        mem_request <= 1'b0;
                        csr_ack <= 1'b1;
                    end
                    if ((!mem_address[27] || source_request == T_N64) && !data_busy) begin
                        mem_request <= 1'b0;
                    end
                    if (!mem_address[27] && mem_write && !data_busy && !write_ack) begin
                        write_ack <= 1'b1;
                    end
                    if (csr_ack || data_ack || write_ack) begin
                        state <= S_IDLE;
                    end
                end
            endcase
        end
    end

    logic csr_or_data;
    logic csr_read;
    logic csr_write;
    logic data_read;
    logic data_write;

    always_comb begin
        csr_or_data = mem_address[27] && source_request == T_CPU;
        csr_read = csr_or_data && mem_request && !mem_write;
        csr_write = csr_or_data && mem_request && mem_write;
        data_read = !csr_or_data && mem_request && !mem_write;
        data_write = !csr_or_data && mem_request && mem_write;

        bus.ack = source_request == T_N64 && data_ack;
        bus.rdata = 16'd0;
        if (bus.ack && bus.address >= 32'h10000000 && bus.address < 32'h10016800) begin
            if (bus.address[1]) bus.rdata = {data_rdata[23:16], data_rdata[31:24]};
            else bus.rdata = {data_rdata[7:0], data_rdata[15:8]};
        end

        flash.ack = source_request == T_CPU && (csr_ack || data_ack || write_ack);
        flash.rdata = 32'd0;
        if (flash.ack) begin
            flash.rdata = csr_or_data ? csr_rdata : data_rdata;
        end
    end

    intel_flash intel_flash_inst (
        .clock(sys.clk),
        .reset_n(~sys.reset),

        .avmm_csr_addr(mem_address[2]),
        .avmm_csr_read(csr_read),
        .avmm_csr_writedata(mem_wdata),
        .avmm_csr_write(csr_write),
        .avmm_csr_readdata(csr_rdata),

        .avmm_data_addr(mem_address[31:2]),
        .avmm_data_read(data_read),
        .avmm_data_writedata(mem_wdata),
        .avmm_data_write(data_write),
        .avmm_data_readdata(data_rdata),
        .avmm_data_waitrequest(data_busy),
        .avmm_data_readdatavalid(data_ack),
        .avmm_data_burstcount(2'd1)
    );

endmodule
