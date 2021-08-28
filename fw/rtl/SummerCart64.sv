module SummerCart64 (
    input i_clk,

    input i_n64_reset,
    input i_n64_nmi,
    output o_n64_irq,

    input i_n64_pi_alel,
    input i_n64_pi_aleh,
    input i_n64_pi_read,
    input i_n64_pi_write,
    inout [15:0] io_n64_pi_ad,

    input i_n64_si_clk,
    inout io_n64_si_dq,

    output o_sdram_clk,
    output o_sdram_cs,
    output o_sdram_ras,
    output o_sdram_cas,
    output o_sdram_we,
    output [1:0] o_sdram_ba,
    output [12:0] o_sdram_a,
    inout [15:0] io_sdram_dq,

    output o_rtc_scl,
    inout io_rtc_sda,

    output o_usb_clk,
    output o_usb_cs,
    input i_usb_miso,
    inout [3:0] io_usb_miosi,
    input i_usb_pwren,

    input i_uart_rxd,
    output o_uart_txd,
    input i_uart_cts,
    output o_uart_rts,

    output o_sd_clk,
    inout io_sd_cmd,
    inout [3:0] io_sd_dat,

    output o_led
);

    logic [7:0] gpio_o;
    logic [7:0] gpio_i;
    logic [7:0] gpio_oe;

    if_system sys (
        .in_clk(i_clk),
        .n64_reset(i_n64_reset),
        .n64_nmi(i_n64_nmi)
    );

    if_config cfg ();

    if_dma dma ();

    if_sdram sdram ();

    if_flashram flashram ();

    system system_inst (
        .sys(sys)
    );

    intel_gpio_ddro sdram_clk_ddro (
        .outclock(sys.sdram.sdram_clk),
        .din({1'b0, 1'b1}),
        .pad_out(o_sdram_clk)
    );

    n64_soc n64_soc_inst (
        .sys(sys),
        .cfg(cfg),
        .dma(dma),
        .sdram(sdram),
        .flashram(flashram),

        .n64_pi_alel(i_n64_pi_alel),
        .n64_pi_aleh(i_n64_pi_aleh),
        .n64_pi_read(i_n64_pi_read),
        .n64_pi_write(i_n64_pi_write),
        .n64_pi_ad(io_n64_pi_ad),

        .n64_si_clk(i_n64_si_clk),
        .n64_si_dq(io_n64_si_dq),

        .sdram_cs(o_sdram_cs),
        .sdram_ras(o_sdram_ras),
        .sdram_cas(o_sdram_cas),
        .sdram_we(o_sdram_we),
        .sdram_ba(o_sdram_ba),
        .sdram_a(o_sdram_a),
        .sdram_dq(io_sdram_dq)
    );

    cpu_soc cpu_soc_inst (
        .sys(sys),
        .cfg(cfg),
        .dma(dma),
        .sdram(sdram),
        .flashram(flashram),

        .gpio_o(gpio_o),
        .gpio_i(gpio_i),
        .gpio_oe(gpio_oe),
        
        .i2c_scl(o_rtc_scl),
        .i2c_sda(io_rtc_sda),

        .usb_clk(o_usb_clk),
        .usb_cs(o_usb_cs),
        .usb_miso(i_usb_miso),
        .usb_miosi(io_usb_miosi),
        .usb_pwren(i_usb_pwren),

        .uart_rxd(i_uart_rxd),
        .uart_txd(o_uart_txd),
        .uart_cts(i_uart_cts),
        .uart_rts(o_uart_rts),

        .sd_clk(o_sd_clk),
        .sd_cmd(io_sd_cmd),
        .sd_dat(io_sd_dat)
    );

    always_comb begin
        o_led = gpio_oe[0] ? gpio_o[0] : 1'bZ;
        o_n64_irq = gpio_oe[1] ? gpio_o[1] : 1'bZ;
    end

    always_ff @(posedge sys.clk) begin
        gpio_i <= {4'b0000, i_n64_nmi, i_n64_reset, o_n64_irq, o_led};
    end

endmodule
