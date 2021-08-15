module SummerCart64 (
    input i_clk,

    output o_usb_clk,
    output o_usb_cs,
    input i_usb_miso,
    inout [3:0] io_usb_miosi,
    input i_usb_powered,

    input i_uart_rxd,
    output o_uart_txd,
    input i_uart_cts,
    output o_uart_rts,

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

    output o_sd_clk,
    inout io_sd_cmd,
    inout [3:0] io_sd_dat,

    inout io_rtc_scl,
    inout io_rtc_sda,

    output o_led
);

    if_system system_if (.in_clk(i_clk));
    system system_inst (.system_if(system_if));

    wire [7:0] gpio_o;
    wire [7:0] gpio_i;
    wire [7:0] gpio_oe;

    assign o_led = gpio_oe[0] ? gpio_o[0] : 1'bZ;
    assign o_n64_irq = gpio_oe[1] ? gpio_o[1] : 1'bZ;
    assign gpio_i = {4'b0000, i_n64_nmi, i_n64_reset, o_n64_irq, o_led};

    cpu_soc cpu_soc_inst (
        .system_if(system_if),
        .gpio_o(gpio_o),
        .gpio_i(gpio_i),
        .gpio_oe(gpio_oe),
        .usb_clk(o_usb_clk),
        .usb_cs(o_usb_cs),
        .usb_miso(i_usb_miso),
        .usb_miosi(io_usb_miosi),
        .usb_powered(i_usb_powered),
        .i2c_scl(io_rtc_scl),
        .i2c_sda(io_rtc_sda)
    );

endmodule
