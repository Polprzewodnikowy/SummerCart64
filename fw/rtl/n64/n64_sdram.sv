module n64_sdram (
    if_system sys,
    if_n64_bus bus,
    if_dma.memory dma,
    if_sdram.memory sdram,

    output sdram_cs,
    output sdram_ras,
    output sdram_cas,
    output sdram_we,
    output [1:0] sdram_ba,
    output [12:0] sdram_a,
    inout [15:0] sdram_dq
);

    logic mem_request;
    logic mem_ack;
    logic mem_write;
    logic [31:0] mem_address;
    logic [15:0] mem_rdata;
    logic [15:0] mem_wdata;

    typedef enum bit [0:0] { 
        S_IDLE,
        S_WAIT
    } e_state;

    typedef enum bit [1:0] { 
        T_BUS,
        T_DMA,
        T_SDRAM
    } e_source_request;

    e_state state;
    e_source_request source_request;

    always_ff @(posedge sys.clk) begin
        if (sys.reset) begin
            state <= S_IDLE;
            mem_request <= 1'b0;
        end else begin
            case (state)
                S_IDLE: begin
                    if (bus.request) begin
                        state <= S_WAIT;
                        mem_request <= 1'b1;
                        mem_write <= bus.write;
                        mem_address <= bus.address;
                        mem_wdata <= bus.wdata;
                        source_request <= T_BUS;
                    end else if ((!bus.n64_active) && (sdram.request || dma.request)) begin
                        state <= S_WAIT;
                        mem_request <= 1'b1;
                        if (sdram.request) begin
                            mem_write <= sdram.write;
                            mem_address <= sdram.address;
                            mem_wdata <= sdram.wdata;
                            source_request <= T_SDRAM;
                        end else if (dma.request) begin
                            mem_write <= dma.write;
                            mem_address <= dma.address;
                            mem_wdata <= dma.wdata;
                            source_request <= T_DMA;
                        end
                    end
                end

                S_WAIT: begin
                    if (mem_ack) begin
                        state <= S_IDLE;
                        mem_request <= 1'b0;
                    end
                end
            endcase
        end
    end

    always_comb begin
        bus.ack = source_request == T_BUS && mem_ack;
        bus.rdata = bus.ack ? mem_rdata : 16'd0;

        dma.ack = source_request == T_DMA && mem_ack;
        dma.rdata = mem_rdata;

        sdram.ack = source_request == T_SDRAM && mem_ack;
        sdram.rdata = mem_rdata;
    end

    memory_sdram memory_sdram_inst (
        .sys(sys),

        .request(mem_request),
        .ack(mem_ack),
        .write(mem_write),
        .address(mem_address[25:0]),
        .rdata(mem_rdata),
        .wdata(mem_wdata),

        .sdram_cs(sdram_cs),
        .sdram_ras(sdram_ras),
        .sdram_cas(sdram_cas),
        .sdram_we(sdram_we),
        .sdram_ba(sdram_ba),
        .sdram_a(sdram_a),
        .sdram_dq(sdram_dq)
    );

endmodule
