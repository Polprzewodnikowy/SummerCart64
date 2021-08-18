module SummerCart64 (
    input i_clk,

    output o_usb_clk,
    output o_usb_cs,
    input i_usb_miso,
    inout [3:0] io_usb_miosi,
    input i_usb_pwren,

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

    logic [7:0] gpio_o;
    logic [7:0] gpio_i;
    logic [7:0] gpio_oe;

    always_comb begin
        o_led = gpio_oe[0] ? gpio_o[0] : 1'bZ;
        o_n64_irq = gpio_oe[1] ? gpio_o[1] : 1'bZ;
        gpio_i = {4'b0000, i_n64_nmi, i_n64_reset, o_n64_irq, o_led};
    end

    if_system sys (
        .in_clk(i_clk),
        .n64_reset(i_n64_reset),
        .n64_nmi(i_n64_nmi)
    );

    system system_inst (
        .sys(sys)
    );

    cpu_soc cpu_soc_inst (
        .sys(sys),

        .gpio_o(gpio_o),
        .gpio_i(gpio_i),
        .gpio_oe(gpio_oe),
        
        .i2c_scl(io_rtc_scl),
        .i2c_sda(io_rtc_sda),

        .usb_clk(o_usb_clk),
        .usb_cs(o_usb_cs),
        .usb_miso(i_usb_miso),
        .usb_miosi(io_usb_miosi),
        .usb_pwren(i_usb_pwren),

        .uart_rxd(i_uart_rxd),
        .uart_txd(o_uart_txd),
        .uart_cts(i_uart_cts),
        .uart_rts(o_uart_rts)
    );

    logic n64_request;
    logic n64_ack;
    logic n64_write;
    logic [31:0] n64_address;
    logic [15:0] n64_wdata;
    logic [15:0] n64_rdata;

    n64_pi n64_pi_inst (
        .sys(sys),

        .n64_pi_alel(i_n64_pi_alel),
        .n64_pi_aleh(i_n64_pi_aleh),
        .n64_pi_read(i_n64_pi_read),
        .n64_pi_write(i_n64_pi_write),
        .n64_pi_ad(io_n64_pi_ad),

        .request(n64_request),
        .ack(n64_ack),
        .write(n64_write),
        .address(n64_address),
        .wdata(n64_wdata),
        .rdata(n64_rdata)
    );

    logic flash_request;
    logic [31:0] flash_rdata;
    logic flash_busy;
    logic flash_ack;

    logic in_address;
    logic dummy_ack;

    intel_flash intel_flash_inst (
        .clock(sys.clk),
        .reset_n(~sys.reset),
        .avmm_data_addr(n64_address[31:2]),
        .avmm_data_read((n64_ack && in_address) || flash_request),
        .avmm_data_readdata(flash_rdata),
        .avmm_data_waitrequest(flash_busy),
        .avmm_data_readdatavalid(flash_ack),
        .avmm_data_burstcount(2'd1)
    );


    always_ff @(posedge sys.clk) begin
        dummy_ack <= 1'b0;
        if (sys.reset) begin
            flash_request <= 1'b0;
        end else begin
            if (!flash_busy) begin
                flash_request <= 1'b0;
            end
            if (n64_request && in_address) begin
                flash_request <= 1'b1;
            end

            if (!in_address && n64_request) begin
                dummy_ack <= 1'b1;
            end
        end
    end

    always_comb begin
        in_address = n64_address < 32'h10008000;
        n64_rdata = n64_address[1] ? {flash_rdata[23:16], flash_rdata[31:24]} : {flash_rdata[7:0], flash_rdata[15:8]};
        if (!in_address) n64_rdata = 32'd0;

        n64_ack = 1'b0;
        if (in_address) begin
            n64_ack = flash_ack;
        end else begin
            n64_ack = dummy_ack;
        end
    end

endmodule
