module top (
    input inclk,

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

    input usb_pwrsav,
    output usb_clk,
    output usb_cs,
    input usb_miso,
    inout [7:0] usb_miosi,

    input sd_det,
    output sd_clk,
    inout sd_cmd,
    inout [3:0] sd_dat,

    output sdram_clk,
    output sdram_cs,
    output sdram_ras,
    output sdram_cas,
    output sdram_we,
    output [1:0] sdram_ba,
    output [12:0] sdram_a,
    output [1:0] sdram_dqm,
    inout [15:0] sdram_dq,

    output flash_clk,
    output flash_cs,
    inout [3:0] flash_dq,

    input button,

    output mcu_int,
    input mcu_clk,
    input mcu_cs,
    input mcu_mosi,
    output mcu_miso
);

    logic clk;
    logic reset;

    n64_scb n64_scb ();
    usb_scb usb_scb ();
    dma_scb usb_dma_scb ();
    sd_scb sd_scb ();
    dma_scb sd_dma_scb ();
    flash_scb flash_scb ();

    fifo_bus usb_cfg_fifo_bus ();
    fifo_bus usb_dma_fifo_bus ();
    fifo_bus usb_fifo_bus ();
    fifo_bus sd_fifo_bus ();

    mem_bus n64_mem_bus ();
    mem_bus cfg_mem_bus ();
    mem_bus usb_dma_mem_bus ();
    mem_bus sd_dma_mem_bus ();
    mem_bus sdram_mem_bus ();
    mem_bus flash_mem_bus ();

    pll pll_inst (
        .inclk(inclk),
        .clk(clk),
        .sdram_clk(sdram_clk),
        .reset(reset)
    );


    // MCU controller

    mcu_top mcu_top_inst (
        .clk(clk),
        .reset(reset),

        .n64_scb(n64_scb),
        .usb_scb(usb_scb),
        .usb_dma_scb(usb_dma_scb),
        .sd_scb(sd_scb),
        .sd_dma_scb(sd_dma_scb),
        .flash_scb(flash_scb),

        .fifo_bus(usb_cfg_fifo_bus),
        .mem_bus(cfg_mem_bus),

        .sd_det(sd_det),
        .button(button),

        .mcu_int(mcu_int),
        .mcu_clk(mcu_clk),
        .mcu_cs(mcu_cs),
        .mcu_mosi(mcu_mosi),
        .mcu_miso(mcu_miso)
    );


    // N64 controller

    n64_top n64_top_inst (
        .clk(clk),
        .reset(reset),

        .n64_scb(n64_scb),

        .mem_bus(n64_mem_bus),

        .n64_reset(n64_reset),
        .n64_nmi(n64_nmi),
        .n64_irq(n64_irq),

        .n64_pi_alel(n64_pi_alel),
        .n64_pi_aleh(n64_pi_aleh),
        .n64_pi_read(n64_pi_read),
        .n64_pi_write(n64_pi_write),
        .n64_pi_ad(n64_pi_ad),

        .n64_si_clk(n64_si_clk),
        .n64_si_dq(n64_si_dq)
    );


    // USB

    usb_ft1248 usb_ft1248_inst (
        .clk(clk),
        .reset(reset),

        .usb_scb(usb_scb),

        .fifo_bus(usb_fifo_bus),

        .usb_pwrsav(usb_pwrsav),
        .usb_clk(usb_clk),
        .usb_cs(usb_cs),
        .usb_miso(usb_miso),
        .usb_miosi(usb_miosi)
    );

    memory_dma memory_usb_dma_inst (
        .clk(clk),
        .reset(reset),

        .dma_scb(usb_dma_scb),

        .fifo_bus(usb_dma_fifo_bus),
        .mem_bus(usb_dma_mem_bus)
    );

    fifo_junction usb_fifo_junction_inst (
        .cfg_bus(usb_cfg_fifo_bus),
        .dma_bus(usb_dma_fifo_bus),
        .dev_bus(usb_fifo_bus)
    );


    // SD card

    sd_top sd_top_inst (
        .clk(clk),
        .reset(reset),

        .sd_scb(sd_scb),

        .fifo_bus(sd_fifo_bus),

        .sd_clk(sd_clk),
        .sd_cmd(sd_cmd),
        .sd_dat(sd_dat)
    );

    memory_dma memory_sd_dma_inst (
        .clk(clk),
        .reset(reset),

        .dma_scb(sd_dma_scb),

        .fifo_bus(sd_fifo_bus),
        .mem_bus(sd_dma_mem_bus)
    );


    // Memory bus arbiter

    memory_arbiter memory_arbiter_inst (
        .clk(clk),
        .reset(reset),

        .n64_bus(n64_mem_bus),
        .cfg_bus(cfg_mem_bus),
        .usb_dma_bus(usb_dma_mem_bus),
        .sd_dma_bus(sd_dma_mem_bus),

        .sdram_mem_bus(sdram_mem_bus),
        .flash_mem_bus(flash_mem_bus)
    );


    // Memory controllers

    memory_sdram memory_sdram_inst (
        .clk(clk),
        .reset(reset),

        .mem_bus(sdram_mem_bus),

        .sdram_cs(sdram_cs),
        .sdram_ras(sdram_ras),
        .sdram_cas(sdram_cas),
        .sdram_we(sdram_we),
        .sdram_ba(sdram_ba),
        .sdram_a(sdram_a),
        .sdram_dqm(sdram_dqm),
        .sdram_dq(sdram_dq)
    );

    memory_flash memory_flash_inst (
        .clk(clk),
        .reset(reset),

        .flash_scb(flash_scb),

        .mem_bus(flash_mem_bus),

        .flash_clk(flash_clk),
        .flash_cs(flash_cs),
        .flash_dq(flash_dq)
    );

endmodule
