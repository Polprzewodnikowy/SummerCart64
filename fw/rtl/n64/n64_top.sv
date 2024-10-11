module n64_top (
    input clk,
    input reset,

    n64_scb n64_scb,
    dd_scb.dd dd_scb,

    mem_bus.controller mem_bus,

    input n64_reset,
    input n64_nmi,
    output n64_irq,

    input n64_pi_alel,
    input n64_pi_aleh,
    input n64_pi_read,
    input n64_pi_write,
    inout [15:0] n64_pi_ad,

    input n64_si_clk,
    inout n64_si_dq,

    input n64_cic_clk,
    inout n64_cic_dq
);

    logic n64_dd_irq;
    logic n64_cfg_irq;

    logic irq_data;
    logic irq_dq;
    logic [1:0] irq_oe;

    assign irq_data = (n64_dd_irq || n64_cfg_irq);

    always @(posedge clk) begin
        irq_dq <= (~irq_data);
        irq_oe <= {irq_oe[0], irq_data};
    end

    assign n64_irq = irq_oe[1] ? irq_dq : 1'bZ;

    n64_reg_bus reg_bus ();

    n64_pi n64_pi_inst (
        .clk(clk),
        .reset(reset),

        .mem_bus(mem_bus),
        .reg_bus(reg_bus),

        .n64_scb(n64_scb),

        .n64_reset(n64_reset),
        .n64_nmi(n64_nmi),
        .n64_pi_alel(n64_pi_alel),
        .n64_pi_aleh(n64_pi_aleh),
        .n64_pi_read(n64_pi_read),
        .n64_pi_write(n64_pi_write),
        .n64_pi_ad(n64_pi_ad)
    );

    n64_dd n64_dd_inst (
        .clk(clk),
        .reset(reset),

        .reg_bus(reg_bus),

        .n64_scb(n64_scb),
        .dd_scb(dd_scb),

        .irq(n64_dd_irq)
    );

    n64_flashram n64_flashram_inst (
        .clk(clk),
        .reset(reset),

        .reg_bus(reg_bus),

        .n64_scb(n64_scb)
    );

    n64_cfg n64_cfg_inst (
        .clk(clk),
        .reset(reset),

        .reg_bus(reg_bus),

        .n64_scb(n64_scb),

        .irq(n64_cfg_irq)
    );

    n64_si n64_si_inst (
        .clk(clk),
        .reset(reset),

        .n64_scb(n64_scb),

        .n64_reset(n64_reset),
        .n64_si_clk(n64_si_clk),
        .n64_si_dq(n64_si_dq)
    );

    n64_save_counter n64_save_counter_inst (
        .clk(clk),
        .reset(reset),

        .n64_scb(n64_scb)
    );

    n64_cic n64_cic_inst (
        .clk(clk),
        .reset(reset),

        .n64_scb(n64_scb),

        .n64_reset(n64_reset),
        .n64_cic_clk(n64_cic_clk),
        .n64_cic_dq(n64_cic_dq),
        .n64_si_clk(n64_si_clk)
    );

endmodule
