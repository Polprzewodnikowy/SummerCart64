module n64_soc (
    if_system sys,
    if_config cfg,
    if_dma.memory dma,

    input n64_pi_alel,
    input n64_pi_aleh,
    input n64_pi_read,
    input n64_pi_write,
    inout [15:0] n64_pi_ad,

    input n64_si_clk,
    inout n64_si_dq,

    output sdram_cs,
    output sdram_ras,
    output sdram_cas,
    output sdram_we,
    output [1:0] sdram_ba,
    output [12:0] sdram_a,
    inout [15:0] sdram_dq
);

    if_n64_bus bus ();

    n64_pi n64_pi_inst (
        .sys(sys),
        .cfg(cfg),
        .bus(bus),

        .n64_pi_alel(n64_pi_alel),
        .n64_pi_aleh(n64_pi_aleh),
        .n64_pi_read(n64_pi_read),
        .n64_pi_write(n64_pi_write),
        .n64_pi_ad(n64_pi_ad)
    );

    n64_sdram n64_sdram_inst (
        .sys(sys),
        .bus(bus.at[sc64::ID_N64_SDRAM].device),
        .dma(dma),

        .sdram_cs(sdram_cs),
        .sdram_ras(sdram_ras),
        .sdram_cas(sdram_cas),
        .sdram_we(sdram_we),
        .sdram_ba(sdram_ba),
        .sdram_a(sdram_a),
        .sdram_dq(sdram_dq)
    );

    n64_bootloader n64_bootloader_inst (
        .sys(sys),
        .bus(bus.at[sc64::ID_N64_BOOTLOADER].device)
    );

    n64_dummy n64_flashram_inst (
        .sys(sys),
        .bus(bus.at[sc64::ID_N64_FLASHRAM].device)
    );

    n64_dummy n64_ddregs_inst (
        .sys(sys),
        .bus(bus.at[sc64::ID_N64_DDREGS].device)
    );

    n64_cfg n64_cfg_inst (
        .sys(sys),
        .bus(bus.at[sc64::ID_N64_CFG].device),
        .cfg(cfg)
    );

endmodule


module n64_dummy (
    if_system.sys sys,
    if_n64_bus bus
);

    typedef enum bit [0:0] {
        S_IDLE,
        S_WAIT
    } e_state;

    e_state state;

    always_comb begin
        bus.rdata = 16'h0000;
        if (bus.ack) begin
            bus.rdata = !bus.address[1] ? 16'h0040 : 16'h0000;
        end
    end

    always_ff @(posedge sys.clk) begin
        bus.ack <= 1'b0;

        if (sys.reset) begin
            state <= S_IDLE;
        end else begin            
            case (state)
                S_IDLE: begin
                    if (bus.request) begin
                        state <= S_WAIT;
                        bus.ack <= 1'b1;
                    end
                end

                S_WAIT: begin
                    state <= S_IDLE;
                end
            endcase
        end
    end

endmodule
