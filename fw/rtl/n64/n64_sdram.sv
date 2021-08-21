module n64_sdram (
    if_system sys,
    if_n64_bus bus,
    if_dma.device dma,

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

    always_comb begin
        mem_request = bus.request || dma.request;
        bus.ack = bus.request && mem_ack;
        dma.ack = dma.request && mem_ack;
        mem_write = (bus.request && bus.write) || (dma.request && dma.write);
        mem_address = dma.request ? dma.address : bus.address;
        mem_wdata = dma.request ? dma.wdata : bus.wdata;
        bus.rdata = mem_rdata;
        dma.rdata = mem_rdata;
    end

    memory_sdram memory_sdram_inst (
        .sys(sys),

        .request(mem_request),
        .ack(mem_ack),
        .write(mem_write),
        .address(mem_address),
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
