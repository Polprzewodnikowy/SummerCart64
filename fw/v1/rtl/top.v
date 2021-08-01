`include "constants.vh"

module top (
    input i_clk,

    output o_ftdi_clk,
    output o_ftdi_si,
    input i_ftdi_so,
    input i_ftdi_cts,

    input i_n64_reset,
    input i_n64_nmi,

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

    output o_rtc_scl,
    inout io_rtc_sda,

    output o_led,

    inout [7:0] io_pmod
);

    // Clock and reset signals

    wire w_sys_clk;
    wire w_sdram_clk;
    wire w_pll_lock;
    wire w_sys_reset = ~w_pll_lock;


    // Temporary signal names

    assign {o_rtc_scl, io_rtc_sda} = 2'bZZ;


    // PLL clock generator

    pll sys_pll (
        .inclk0(i_clk),
        .c0(w_sys_clk),
        .c1(w_sdram_clk),
        .locked(w_pll_lock)
    );


    // SDRAM clock output

    gpio_ddro sdram_clk_ddro (
        .outclock(w_sdram_clk),
        .outclocken(1'b1),
        .din({1'b0, 1'b1}),
        .pad_out(o_sdram_clk)
    );


    // SD clock output

    wire w_sd_clk;

    gpio_ddro sd_clk_ddro (
        .outclock(w_sd_clk),
        .outclocken(1'b1),
        .din({1'b0, 1'b1}),
        .pad_out(o_sd_clk)
    );


    // N64 PI

    wire w_n64_request;
    wire w_n64_write;
    wire w_n64_busy;
    wire w_n64_ack;
    wire [3:0] w_n64_bank;
    wire [25:0] w_n64_address;
    wire [31:0] w_n64_i_data;
    wire [31:0] w_n64_o_data;

    wire w_n64_busy_cart_control;
    wire w_n64_ack_cart_control;
    wire [31:0] w_n64_i_data_cart_control;

    wire w_n64_busy_embedded_flash;
    wire w_n64_ack_embedded_flash;
    wire [31:0] w_n64_i_data_embedded_flash;

    wire w_n64_busy_sdram;
    wire w_n64_ack_sdram;
    wire [31:0] w_n64_i_data_sdram;

    wire w_n64_busy_eeprom;
    wire w_n64_ack_eeprom;
    wire [31:0] w_n64_i_data_eeprom;

    wire w_n64_busy_flashram;
    wire w_n64_ack_flashram;
    wire [31:0] w_n64_i_data_flashram;

    wire w_n64_busy_sd;
    wire w_n64_ack_sd;
    wire [31:0] w_n64_i_data_sd;

    wire w_sram_pi_request;
    wire w_flashram_pi_request;

    wire w_ddipl_enable;
    wire w_sram_enable;
    wire w_flashram_enable;
    wire w_sd_enable;
    wire w_eeprom_pi_enable;

    wire [23:0] w_ddipl_address;
    wire [23:0] w_save_address;

    wire w_flashram_read_mode;

    always @(*) begin
        w_n64_busy = (
            w_n64_busy_cart_control ||
            w_n64_busy_embedded_flash ||
            w_n64_busy_sdram ||
            w_n64_busy_eeprom ||
            w_n64_busy_flashram ||
            w_n64_busy_sd
        );
        w_n64_ack = (
            w_n64_ack_cart_control ||
            w_n64_ack_embedded_flash ||
            w_n64_ack_sdram ||
            w_n64_ack_eeprom ||
            w_n64_ack_flashram ||
            w_n64_ack_sd
        );
        w_n64_i_data = 32'h0000_0000;
        if (w_n64_ack_cart_control) w_n64_i_data = w_n64_i_data_cart_control;
        if (w_n64_ack_embedded_flash) w_n64_i_data = w_n64_i_data_embedded_flash;
        if (w_n64_ack_sdram) w_n64_i_data = w_n64_i_data_sdram;
        if (w_n64_ack_eeprom) w_n64_i_data = w_n64_i_data_eeprom;
        if (w_n64_ack_flashram) w_n64_i_data = w_n64_i_data_flashram;
        if (w_n64_ack_sd) w_n64_i_data = w_n64_i_data_sd;
    end

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

        .o_sram_pi_request(w_sram_pi_request),
        .o_flashram_pi_request(w_flashram_pi_request),

        .i_ddipl_enable(w_ddipl_enable),
        .i_sram_enable(w_sram_enable),
        .i_flashram_enable(w_flashram_enable),
        .i_sd_enable(w_sd_enable),
        .i_eeprom_pi_enable(w_eeprom_pi_enable),

        .i_ddipl_address(w_ddipl_address),
        .i_save_address(w_save_address),

        .i_flashram_read_mode(w_flashram_read_mode)
    );


    // PC USB

    wire w_pc_request;
    wire w_pc_write;
    wire w_pc_busy;
    wire w_pc_ack;
    wire [3:0] w_pc_bank;
    wire [25:0] w_pc_address;
    wire [31:0] w_pc_i_data;
    wire [31:0] w_pc_o_data;

    wire w_pc_busy_cart_control;
    wire w_pc_ack_cart_control;
    wire [31:0] w_pc_i_data_cart_control;

    wire w_pc_busy_sdram;
    wire w_pc_ack_sdram;
    wire [31:0] w_pc_i_data_sdram;

    wire w_pc_busy_eeprom;
    wire w_pc_ack_eeprom;
    wire [31:0] w_pc_i_data_eeprom;

    wire w_debug_dma_start;
    wire w_debug_dma_busy;
    wire [3:0] w_debug_dma_bank;
    wire [23:0] w_debug_dma_address;
    wire [19:0] w_debug_dma_length;

    wire w_debug_fifo_request;
    wire w_debug_fifo_flush;
    wire [10:0] w_debug_fifo_items;
    wire [31:0] w_debug_fifo_data;

    always @(*) begin
        w_pc_busy = w_pc_busy_cart_control || w_pc_busy_sdram || w_pc_busy_eeprom;
        w_pc_ack = w_pc_ack_cart_control || w_pc_ack_sdram || w_pc_ack_eeprom;
        w_pc_i_data = 32'h0000_0000;
        if (w_pc_ack_cart_control) w_pc_i_data = w_pc_i_data_cart_control;
        if (w_pc_ack_sdram) w_pc_i_data = w_pc_i_data_sdram;
        if (w_pc_ack_eeprom) w_pc_i_data = w_pc_i_data_eeprom;
    end

    usb_pc #(
        .VERSION(`VERSION)
    ) usb_pc_inst (
        .i_clk(w_sys_clk),
        .i_reset(w_sys_reset),

        .o_ftdi_clk(o_ftdi_clk),
        .o_ftdi_si(o_ftdi_si),
        .i_ftdi_so(i_ftdi_so),
        .i_ftdi_cts(i_ftdi_cts),

        .o_request(w_pc_request),
        .o_write(w_pc_write),
        .i_busy(w_pc_busy),
        .i_ack(w_pc_ack),
        .o_bank(w_pc_bank),
        .o_address(w_pc_address),
        .i_data(w_pc_i_data),
        .o_data(w_pc_o_data),

        .i_debug_start(w_debug_dma_start),
        .o_debug_busy(w_debug_dma_busy),
        .i_debug_bank(w_debug_dma_bank),
        .i_debug_address(w_debug_dma_address),
        .i_debug_length(w_debug_dma_length),

        .i_debug_fifo_request(w_debug_fifo_request),
        .i_debug_fifo_flush(w_debug_fifo_flush),
        .o_debug_fifo_items(w_debug_fifo_items),
        .o_debug_fifo_data(w_debug_fifo_data)
    );


    // Cart interface

    wire w_cart_control_request;
    wire w_cart_control_write;
    wire w_cart_control_busy;
    wire w_cart_control_ack;
    wire [10:0] w_cart_control_address;
    wire [31:0] w_cart_control_o_data;
    wire [31:0] w_cart_control_i_data;

    wire w_sdram_writable;
    wire w_rom_switch;
    wire w_eeprom_enable;
    wire w_eeprom_16k_mode;

    device_arbiter #(
        .NUM_CONTROLLERS(2),
        .ADDRESS_WIDTH(11),
        .DEVICE_BANK(`BANK_CART)
    ) device_arbiter_cart_control_inst (
        .i_clk(w_sys_clk),
        .i_reset(w_sys_reset),

        .i_request({w_pc_request, w_n64_request}),
        .i_write({w_pc_write, w_n64_write}),
        .o_busy({w_pc_busy_cart_control, w_n64_busy_cart_control}),
        .o_ack({w_pc_ack_cart_control, w_n64_ack_cart_control}),
        .i_bank({w_pc_bank, w_n64_bank}),
        .i_address({w_pc_address[12:2], w_n64_address[12:2]}),
        .o_data({w_pc_i_data_cart_control, w_n64_i_data_cart_control}),
        .i_data({w_pc_o_data, w_n64_o_data}),

        .o_device_request(w_cart_control_request),
        .o_device_write(w_cart_control_write),
        .i_device_busy(w_cart_control_busy),
        .i_device_ack(w_cart_control_ack),
        .o_device_address(w_cart_control_address),
        .i_device_data(w_cart_control_o_data),
        .o_device_data(w_cart_control_i_data)
    );

    cart_control #(
        .VERSION(`VERSION)
    ) cart_control_inst (
        .i_clk(w_sys_clk),
        .i_reset(w_sys_reset),

        .i_n64_reset(i_n64_reset),
        .i_n64_nmi(i_n64_nmi),

        .i_request(w_cart_control_request),
        .i_write(w_cart_control_write),
        .o_busy(w_cart_control_busy),
        .o_ack(w_cart_control_ack),
        .i_address(w_cart_control_address),
        .o_data(w_cart_control_o_data),
        .i_data(w_cart_control_i_data),

        .o_sdram_writable(w_sdram_writable),
        .o_rom_switch(w_rom_switch),
        .o_ddipl_enable(w_ddipl_enable),
        .o_sram_enable(w_sram_enable),
        .o_flashram_enable(w_flashram_enable),
        .o_sd_enable(w_sd_enable),
        .o_eeprom_pi_enable(w_eeprom_pi_enable),
        .o_eeprom_enable(w_eeprom_enable),
        .o_eeprom_16k_mode(w_eeprom_16k_mode),

        .io_gpio(io_pmod),

        .i_debug_ready(1'b1),   // TODO: Detect USB cable insertion

        .o_debug_dma_start(w_debug_dma_start),
        .i_debug_dma_busy(w_debug_dma_busy),
        .o_debug_dma_bank(w_debug_dma_bank),
        .o_debug_dma_address(w_debug_dma_address),
        .o_debug_dma_length(w_debug_dma_length),

        .o_debug_fifo_request(w_debug_fifo_request),
        .o_debug_fifo_flush(w_debug_fifo_flush),
        .i_debug_fifo_items(w_debug_fifo_items),
        .i_debug_fifo_data(w_debug_fifo_data),

        .o_ddipl_address(w_ddipl_address),
        .o_save_address(w_save_address)
    );


    // SD card

    wire w_n64_request_sd = (
        w_n64_request &&
        w_n64_bank == `BANK_SD
    );

    wire w_sd_dma_request;
    wire w_sd_dma_write;
    wire w_sd_dma_busy;
    wire w_sd_dma_ack;
    wire [3:0] w_sd_dma_bank;
    wire [23:0] w_sd_dma_address;
    wire [31:0] w_sd_dma_i_data;
    wire [31:0] w_sd_dma_o_data;

    wire w_sd_dma_busy_sdram;
    wire w_sd_dma_ack_sdram;
    wire [31:0] w_sd_dma_i_data_sdram;

    wire w_sd_dma_busy_eeprom;
    wire w_sd_dma_ack_eeprom;
    wire [31:0] w_sd_dma_i_data_eeprom;

    always @(*) begin
        w_sd_dma_busy = w_sd_dma_busy_sdram || w_sd_dma_busy_eeprom;
        w_sd_dma_ack = w_sd_dma_ack_sdram || w_sd_dma_ack_eeprom;
        w_sd_dma_i_data = 32'h0000_0000;
        if (w_sd_dma_ack_sdram) w_sd_dma_i_data = w_sd_dma_i_data_sdram;
        if (w_sd_dma_ack_eeprom) w_sd_dma_i_data = w_sd_dma_i_data_eeprom;
    end

    sd_interface sd_interface_inst (
        .i_clk(w_sys_clk),
        .i_reset(w_sys_reset),

        .o_sd_clk(w_sd_clk),
        .io_sd_cmd(io_sd_cmd),
        .io_sd_dat(io_sd_dat),

        .i_request(w_n64_request_sd),
        .i_write(w_n64_write),
        .o_busy(w_n64_busy_sd),
        .o_ack(w_n64_ack_sd),
        .i_address({w_n64_address[9], w_n64_address[4:2]}),
        .o_data(w_n64_i_data_sd),
        .i_data(w_n64_o_data),

        .o_dma_request(w_sd_dma_request),
        .o_dma_write(w_sd_dma_write),
        .i_dma_busy(w_sd_dma_busy),
        .i_dma_ack(w_sd_dma_ack),
        .o_dma_bank(w_sd_dma_bank),
        .o_dma_address(w_sd_dma_address),
        .o_dma_data(w_sd_dma_o_data),
        .i_dma_data(w_sd_dma_i_data)
    );


    // FlashRAM

    wire w_flashram_request_n64 = (
        w_n64_request &&
        w_flashram_pi_request &&
        w_n64_bank == `BANK_SDRAM
    );

    wire w_flashram_dma_request;
    wire w_flashram_dma_write;
    wire w_flashram_dma_busy;
    wire w_flashram_dma_ack;
    wire [3:0] w_flashram_dma_bank;
    wire [23:0] w_flashram_dma_address;
    wire [31:0] w_flashram_dma_i_data;
    wire [31:0] w_flashram_dma_o_data;

    flashram_controller flashram_controller_inst (
        .i_clk(w_sys_clk),
        .i_reset(w_sys_reset),

        .i_save_address(w_save_address),

        .o_flashram_read_mode(w_flashram_read_mode),

        .i_request(w_flashram_request_n64),
        .i_write(w_n64_write),
        .o_busy(w_n64_busy_flashram),
        .o_ack(w_n64_ack_flashram),
        .i_address(w_n64_address[16:2]),
        .o_data(w_n64_i_data_flashram),
        .i_data(w_n64_o_data),

        .o_mem_request(w_flashram_dma_request),
        .o_mem_write(w_flashram_dma_write),
        .i_mem_busy(w_flashram_dma_busy),
        .i_mem_ack(w_flashram_dma_ack),
        .o_mem_bank(w_flashram_dma_bank),
        .o_mem_address(w_flashram_dma_address),
        .o_mem_data(w_flashram_dma_o_data),
        .i_mem_data(w_flashram_dma_i_data)
    );


    // Embedded flash in FPGA

    wire w_embedded_flash_request_n64 = (
        w_n64_request &&
        !w_rom_switch &&
        !w_n64_write &&
        w_n64_bank == `BANK_SDRAM
    );

    memory_embedded_flash memory_embedded_flash_inst (
        .i_clk(w_sys_clk),
        .i_reset(w_sys_reset),

        .i_request(w_embedded_flash_request_n64),
        .o_busy(w_n64_busy_embedded_flash),
        .o_ack(w_n64_ack_embedded_flash),
        .i_address(w_n64_address[20:2]),
        .o_data(w_n64_i_data_embedded_flash)
    );


    // SDRAM

    wire w_sdram_request_n64 = w_n64_request && (
        (w_rom_switch && (w_sdram_writable || !w_n64_write) && !w_flashram_pi_request) ||
        w_sram_pi_request ||
        (w_flashram_pi_request && w_flashram_read_mode && !w_n64_write)
    );

    wire w_sdram_request;
    wire w_sdram_write;
    wire w_sdram_busy;
    wire w_sdram_ack;
    wire [24:0] w_sdram_address;
    wire [31:0] w_sdram_o_data;
    wire [31:0] w_sdram_i_data;

    device_arbiter #(
        .NUM_CONTROLLERS(4),
        .ADDRESS_WIDTH(25),
        .DEVICE_BANK(`BANK_SDRAM)
    ) device_arbiter_sdram_inst (
        .i_clk(w_sys_clk),
        .i_reset(w_sys_reset),

        .i_request({w_flashram_dma_request, w_sd_dma_request, w_pc_request, w_sdram_request_n64}),
        .i_write({w_flashram_dma_write, w_sd_dma_write, w_pc_write, w_n64_write}),
        .o_busy({w_flashram_dma_busy, w_sd_dma_busy_sdram, w_pc_busy_sdram, w_n64_busy_sdram}),
        .o_ack({w_flashram_dma_ack, w_sd_dma_ack_sdram, w_pc_ack_sdram, w_n64_ack_sdram}),
        .i_bank({w_flashram_dma_bank, w_sd_dma_bank, w_pc_bank, w_n64_bank}),
        .i_address({{w_flashram_dma_address, 1'b0}, {w_sd_dma_address, 1'b0}, w_pc_address[25:1], w_n64_address[25:1]}),
        .o_data({w_flashram_dma_i_data, w_sd_dma_i_data_sdram, w_pc_i_data_sdram, w_n64_i_data_sdram}),
        .i_data({w_flashram_dma_o_data, w_sd_dma_o_data, w_pc_o_data, w_n64_o_data}),

        .o_device_request(w_sdram_request),
        .o_device_write(w_sdram_write),
        .i_device_busy(w_sdram_busy),
        .i_device_ack(w_sdram_ack),
        .o_device_address(w_sdram_address),
        .i_device_data(w_sdram_o_data),
        .o_device_data(w_sdram_i_data)
    );

    memory_sdram memory_sdram_inst (
        .i_clk(w_sys_clk),
        .i_reset(w_sys_reset),

        .o_sdram_cs(o_sdram_cs),
        .o_sdram_ras(o_sdram_ras),
        .o_sdram_cas(o_sdram_cas),
        .o_sdram_we(o_sdram_we),
        .o_sdram_ba(o_sdram_ba),
        .o_sdram_a(o_sdram_a),
        .io_sdram_dq(io_sdram_dq),

        .i_request(w_sdram_request),
        .i_write(w_sdram_write),
        .o_busy(w_sdram_busy),
        .o_ack(w_sdram_ack),
        .i_address(w_sdram_address),
        .o_data(w_sdram_o_data),
        .i_data(w_sdram_i_data)
    );


    // N64 SI (EEPROM 4/16k and RTC)

    wire w_eeprom_request;
    wire w_eeprom_write;
    wire w_eeprom_busy;
    wire w_eeprom_ack;
    wire [8:0] w_eeprom_address;
    wire [31:0] w_eeprom_o_data;
    wire [31:0] w_eeprom_i_data;

    device_arbiter #(
        .NUM_CONTROLLERS(3),
        .ADDRESS_WIDTH(9),
        .DEVICE_BANK(`BANK_EEPROM)
    ) device_arbiter_eeprom_inst (
        .i_clk(w_sys_clk),
        .i_reset(w_sys_reset),

        .i_request({w_sd_dma_request, w_pc_request, w_n64_request}),
        .i_write({w_sd_dma_write, w_pc_write, w_n64_write}),
        .o_busy({w_sd_dma_busy_eeprom, w_pc_busy_eeprom, w_n64_busy_eeprom}),
        .o_ack({w_sd_dma_ack_eeprom, w_pc_ack_eeprom, w_n64_ack_eeprom}),
        .i_bank({w_sd_dma_bank, w_pc_bank, w_n64_bank}),
        .i_address({w_sd_dma_address[8:0], w_pc_address[10:2], w_n64_address[10:2]}),
        .o_data({w_sd_dma_i_data_eeprom, w_pc_i_data_eeprom, w_n64_i_data_eeprom}),
        .i_data({w_sd_dma_o_data, w_pc_o_data, w_n64_o_data}),

        .o_device_request(w_eeprom_request),
        .o_device_write(w_eeprom_write),
        .i_device_busy(w_eeprom_busy),
        .i_device_ack(w_eeprom_ack),
        .o_device_address(w_eeprom_address),
        .i_device_data(w_eeprom_o_data),
        .o_device_data(w_eeprom_i_data)
    );

    n64_si n64_si_inst (
        .i_clk(w_sys_clk),
        .i_reset(w_sys_reset),

        .i_n64_reset(i_n64_reset),
        .i_n64_si_clk(i_n64_si_clk),
        .io_n64_si_dq(io_n64_si_dq),

        .i_request(w_eeprom_request),
        .i_write(w_eeprom_write),
        .o_busy(w_eeprom_busy),
        .o_ack(w_eeprom_ack),
        .i_address(w_eeprom_address),
        .i_data(w_eeprom_i_data),
        .o_data(w_eeprom_o_data),

        .i_eeprom_enable(w_eeprom_enable),
        .i_eeprom_16k_mode(w_eeprom_16k_mode)
    );


    // LED

    wire w_led_trigger = (
        (w_n64_request && !w_n64_busy) ||
        (w_pc_request && !w_pc_busy) ||
        (w_sd_dma_request && !w_sd_dma_busy)
    );

    cart_led cart_led_inst (
        .i_clk(w_sys_clk),
        .i_reset(w_sys_reset),

        .i_trigger(w_led_trigger),

        .o_led(o_led)
    );

endmodule
