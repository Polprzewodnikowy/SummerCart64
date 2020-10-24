module top (
    input i_clk,

    output o_ftdi_clk,
    output o_ftdi_si,
    input i_ftdi_so,
    input i_ftdi_cts,

    input i_n64_nmi,
    input i_n64_reset,

    input i_n64_pi_alel,
    input i_n64_pi_aleh,
    input i_n64_pi_read,
    input i_n64_pi_write,
    inout [15:0] io_n64_pi_ad,

    input i_n64_si_clk,
    inout io_n64_si_dq,

    input i_n64_cic_clk,
    inout io_n64_cic_dq,

    output o_sdram_clk,
    output o_sdram_cs,
    output o_sdram_cas,
    output o_sdram_ras,
    output o_sdram_we,
    output [1:0] o_sdram_ba,
    output [12:0] o_sdram_a,
    inout [15:0] io_sdram_dq,

    output o_sd_clk,
    inout io_sd_cmd,
    inout [3:0] io_sd_dat,

    output o_flash_clk,
    output o_flash_cs,
    inout [3:0] io_flash_dq,

    output o_sram_clk,
    output o_sram_cs,
    inout [3:0] io_sram_dq,

    output o_rtc_scl,
    inout io_rtc_sda,

    output o_led,

    inout [7:0] io_pmod
);

    wire w_sys_clk;
    wire w_sdram_clk;
    wire w_pll_lock;
    wire w_sys_reset = ~w_pll_lock;

    pll sys_pll(
        .inclk0(i_clk),
        .c0(w_sys_clk),
        .c1(w_sdram_clk),
        .locked(w_pll_lock)
    );

    gpio_ddro sdram_clk_ddro(
        .outclock(w_sdram_clk),
        .outclocken(1'b1),
        .din({1'b0, 1'b1}),
        .pad_out(o_sdram_clk)
    );

    wire w_n64_request;
    wire w_n64_write;
    wire w_n64_busy;
    wire w_n64_ack;
    wire [3:0] w_n64_bank;
    wire [25:0] w_n64_address;
    wire [31:0] w_n64_i_data;
    wire [31:0] w_n64_o_data;

    n64_pi n64_pi_inst (
        .i_clk(w_sys_clk),
        .i_reset(w_sys_reset),

        .i_n64_reset(i_n64_reset),
        .i_n64_pi_alel(i_n64_pi_alel),
        .i_n64_pi_aleh(i_n64_pi_aleh),
        .i_n64_pi_read(i_n64_pi_read),
        .i_n64_pi_write(i_n64_pi_write),
        .io_n64_pi_ad(io_n64_pi_ad),

        .o_request(w_n64_request),
        .o_write(w_n64_write),
        .i_busy(w_n64_busy),
        .i_ack(w_n64_ack),
        .o_bank(w_n64_bank),
        .o_address(w_n64_address),
        .i_data(w_n64_i_data),
        .o_data(w_n64_o_data),

        .o_debug(o_led)
    );

    wire w_embedded_flash_request = w_n64_request && w_n64_bank == 4'd1;

    embedded_flash embedded_flash_inst (
        .i_clk(w_sys_clk),
        .i_reset(w_sys_reset),

        .i_request(w_embedded_flash_request),
        .o_busy(w_n64_busy),
        .o_ack(w_n64_ack),
        .i_address(w_n64_address[20:2]),
        .o_data(w_n64_i_data)
    );

endmodule
