module n64_sdram (
    if_system sys,
    if_n64_bus bus,
    if_dma.memory dma,

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

    typedef enum bit [0:0] { 
        T_BUS,
        T_DMA
    } e_bus_or_dma;

    e_state state;
    e_bus_or_dma bus_or_dma;

    always_ff @(posedge sys.clk) begin
        if (sys.reset) begin
            state <= S_IDLE;
            mem_request <= 1'b0;
        end else begin
            case (state)
                S_IDLE: begin
                    if (bus.request || dma.request) begin
                        state <= S_WAIT;
                        mem_request <= 1'b1;
                        mem_write <= bus.request ? bus.write : dma.write;
                        mem_address <= bus.request ? bus.address : dma.address;
                        mem_wdata <= bus.request ? bus.wdata : dma.wdata;
                        bus_or_dma <= bus.request ? T_BUS : T_DMA;
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
        bus.ack = bus_or_dma == T_BUS && mem_ack;
        bus.rdata = mem_rdata;

        dma.ack = bus_or_dma == T_DMA && mem_ack;
        dma.rdata = mem_rdata;
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
