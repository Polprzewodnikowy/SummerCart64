module top (
    input i_clk,

    input i_ftdi_clk,
    input i_ftdi_cs,
    input i_ftdi_do,
    output o_ftdi_di,

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

    // Clock and reset

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

    // Input synchronization

    reg r_n64_nmi_ff1, r_n64_nmi_ff2;
    reg r_n64_reset_ff1, r_n64_reset_ff2;

    reg r_n64_alel_ff1, r_n64_alel_ff2;
    reg r_n64_aleh_ff1, r_n64_aleh_ff2;
    reg r_n64_read_ff1, r_n64_read_ff2;
    reg r_n64_write_ff1, r_n64_write_ff2;

    reg r_n64_si_clk_ff1, r_n64_si_clk_ff2;
    reg r_n64_si_dq_ff1, r_n64_si_dq_ff2;

    always @(posedge w_sys_clk) begin
        {r_n64_nmi_ff2, r_n64_nmi_ff1} <= {r_n64_nmi_ff1, i_n64_nmi};
        {r_n64_reset_ff2, r_n64_reset_ff1} <= {r_n64_reset_ff1, i_n64_reset};

        {r_n64_alel_ff2, r_n64_alel_ff1} <= {r_n64_alel_ff1, i_n64_pi_alel};
        {r_n64_aleh_ff2, r_n64_aleh_ff1} <= {r_n64_aleh_ff1, i_n64_pi_aleh};
        {r_n64_read_ff2, r_n64_read_ff1} <= {r_n64_read_ff1, i_n64_pi_read};
        {r_n64_write_ff2, r_n64_write_ff1} <= {r_n64_write_ff1, i_n64_pi_write};

        {r_n64_si_clk_ff2, r_n64_si_clk_ff1} <= {r_n64_si_clk_ff1, i_n64_si_clk};
        {r_n64_si_dq_ff2, r_n64_si_dq_ff1} <= {r_n64_si_dq_ff1, io_n64_si_dq};
    end

    // Tri-state connection management

    wire w_n64_pi_ad_mode;
    wire [15:0] w_n64_pi_ad_o;
    assign io_n64_pi_ad = w_n64_pi_ad_mode ? w_n64_pi_ad_o : 16'hZZZZ;

    wire w_n64_si_dq_o;
    assign io_n64_si_dq = w_n64_si_dq_o ? 1'bZ : 1'b0;

    wire w_n64_cic_dq_o;
    assign io_n64_cic_dq = w_n64_cic_dq_o ? 1'bZ : 1'b0;

    wire w_sdram_dq_mode;
    wire [15:0] w_sdram_dq_o;
    assign io_sdram_dq = w_sdram_dq_mode ? w_sdram_dq_o : 16'hZZZZ;

    wire w_sd_cmd_mode;
    wire [1:0] w_sd_dat_mode;
    wire w_sd_cmd_o;
    wire [3:0] w_sd_dat_o;
    assign io_sd_cmd = w_sd_cmd_mode ? w_sd_cmd_o : 1'bZ;
    assign io_sd_dat = w_sd_dat_mode == 2'b00 ? {3'bZZZ, w_sd_dat_o[0]} :
                       w_sd_dat_mode == 2'b10 ? w_sd_dat_o : 4'bZZZZ;

    wire [1:0] w_flash_dq_mode;
    wire [3:0] w_flash_dq_o;
    assign io_flash_dq = w_flash_dq_mode == 2'b00 ? {3'bZZZ, w_flash_dq_o[0]} :
                         w_flash_dq_mode == 2'b10 ? w_flash_dq_o : 4'bZZZZ;

    wire [1:0] w_sram_dq_mode;
    wire [3:0] w_sram_dq_o;
    assign io_sram_dq = w_sram_dq_mode == 2'b00 ? {3'bZZZ, w_sram_dq_o[0]} :
                        w_sram_dq_mode == 2'b10 ? w_sram_dq_o : 4'bZZZZ;

    wire w_rtc_sda_o;
    assign io_rtc_sda = w_rtc_sda_o ? 1'bZ : 1'b0;

    // Temporary assignments

    assign w_n64_cic_dq_o = 1'b1;
    assign w_sd_cmd_mode = 1'b0;
    assign w_sd_dat_mode = 2'b00;
    assign w_sram_dq_mode = 2'b00;
    assign w_rtc_sda_o = 1'b1;
    assign io_pmod = 8'hZZ;

    // Modules connection

    wire w_n64_read_rq;
    wire w_n64_write_rq;
    wire w_n64_ack;
    wire [31:0] w_n64_address;
    wire [31:0] w_n64_i_data;
    wire [31:0] w_n64_o_data;

    wire w_pc_read_rq;
    wire w_pc_write_rq;
    wire w_pc_ack;
    wire [31:0] w_pc_address;
    wire [31:0] w_pc_i_data;
    wire [31:0] w_pc_o_data;

    wire w_bus_read_rq;
    wire w_bus_write_rq;
    wire w_bus_ack;
    wire [31:0] w_bus_address;
    reg [31:0] r_bus_i_data;
    wire [31:0] w_bus_o_data;

    wire w_n64_disable;

    assign w_n64_ack = !w_n64_disable && w_bus_ack;
    assign w_pc_ack = w_n64_disable && w_bus_ack;

    assign w_bus_read_rq = w_n64_disable ? w_pc_read_rq : w_n64_read_rq;
    assign w_bus_write_rq = w_n64_disable ? w_pc_write_rq : w_n64_write_rq;
    assign w_bus_address = w_n64_disable ? w_pc_address : w_n64_address;
    assign w_bus_o_data = w_n64_disable ? w_pc_o_data : w_n64_o_data;

    wire w_cart_config_select;
    wire w_flash_select;
    wire w_flash_cfg_select;
    wire w_sdram_select;
    wire w_eeprom_select;

    wire w_flash_enable;
    wire w_sdram_enable;
    wire w_eeprom_pi_enable;
    wire w_eeprom_enable;
    wire w_eeprom_16k_enable;

    wire w_address_valid;

    wire w_cart_config_ack;
    wire [31:0] w_cart_config_o_data;

    wire w_flash_ack;
    wire [31:0] w_flash_o_data;

    wire w_sdram_ack;
    wire [31:0] w_sdram_o_data;

    wire w_eeprom_ack;
    wire [31:0] w_eeprom_o_data;

    reg r_empty_ack;

    assign w_bus_ack = w_cart_config_ack || w_flash_ack || w_sdram_ack || w_eeprom_ack || r_empty_ack;

    always @(*) begin
        r_bus_i_data = 32'hFFFF_FFFF;
        if (w_cart_config_select) r_bus_i_data = w_cart_config_o_data;
        if (w_flash_select || w_flash_cfg_select) r_bus_i_data = w_flash_o_data;
        if (w_sdram_select) r_bus_i_data = w_sdram_o_data;
        if (w_eeprom_select) r_bus_i_data = w_eeprom_o_data;
    end

    always @(posedge w_sys_clk) begin
        r_empty_ack <= !w_address_valid && (w_bus_read_rq || w_bus_write_rq);
    end

    // Bus activity signal

    reg r_bus_active;
    wire w_bus_active = r_bus_active && !w_bus_ack;

    always @(posedge w_sys_clk or posedge w_sys_reset) begin
        if (w_sys_reset) begin
            r_bus_active <= 1'b0;
        end else begin
            if (w_bus_read_rq || w_bus_write_rq) r_bus_active <= 1'b1;
            if (w_bus_ack) r_bus_active <= 1'b0;
        end
    end

    // Modules

    pc pc_inst (
        .i_clk(w_sys_clk),
        .i_reset(w_sys_reset),

        .i_ftdi_clk(i_ftdi_clk),
        .i_ftdi_cs(i_ftdi_cs),
        .i_ftdi_do(i_ftdi_do),
        .o_ftdi_di(o_ftdi_di),

        .o_read_rq(w_pc_read_rq),
        .o_write_rq(w_pc_write_rq),
        .i_ack(w_pc_ack),
        .o_address(w_pc_address),
        .i_data(r_bus_i_data),
        .o_data(w_pc_o_data),

        .i_bus_active(w_bus_active),
        .o_n64_disable(w_n64_disable)
    );

    n64_pi n64_pi_inst (
        .i_clk(w_sys_clk),
        .i_reset(~r_n64_reset_ff2),

        .i_n64_pi_alel({i_n64_pi_alel, r_n64_alel_ff2}),
        .i_n64_pi_aleh({i_n64_pi_aleh, r_n64_aleh_ff2}),
        .i_n64_pi_read(r_n64_read_ff2),
        .i_n64_pi_write(r_n64_write_ff2),
        .i_n64_pi_ad(io_n64_pi_ad),
        .o_n64_pi_ad(w_n64_pi_ad_o),
        .o_n64_pi_ad_mode(w_n64_pi_ad_mode),

        .o_read_rq(w_n64_read_rq),
        .o_write_rq(w_n64_write_rq),
        .i_ack(w_n64_ack),
        .o_address(w_n64_address),
        .i_data(r_bus_i_data),
        .o_data(w_n64_o_data),

        .i_address_valid(w_address_valid)
    );

    n64_si n64_si_inst (
        .i_clk(w_sys_clk),
        .i_reset(w_sys_reset),
        
        .i_si_clk(r_n64_si_clk_ff2),
        .i_si_reset(~r_n64_reset_ff2),
        .i_si_dq(r_n64_si_dq_ff2),
        .o_si_dq(w_n64_si_dq_o),

        .i_eeprom_select(w_eeprom_select),
        .i_read_rq(w_bus_read_rq),
        .i_write_rq(w_bus_write_rq),
        .o_ack(w_eeprom_ack),
        .i_address(w_bus_address),
        .i_data(w_bus_o_data),
        .o_data(w_eeprom_o_data),

        .i_eeprom_enable(w_eeprom_enable),
        .i_eeprom_16k_enable(w_eeprom_16k_enable)
    );

    address_decoder address_decoder_inst (
        .i_address(w_bus_address),

        .o_cart_config(w_cart_config_select),
        .o_flash(w_flash_select),
        .o_flash_cfg(w_flash_cfg_select),
        .o_sdram(w_sdram_select),
        .o_eeprom(w_eeprom_select),

        .i_flash_enable(w_flash_enable),
        .i_sdram_enable(w_sdram_enable),
        .i_eeprom_pi_enable(w_eeprom_pi_enable),

        .o_address_valid(w_address_valid)
    );

    cart_config cart_config_inst (
        .i_clk(w_sys_clk),
        .i_reset(w_sys_reset),

        .i_n64_reset(~r_n64_reset_ff2),
        .i_n64_nmi(~r_n64_nmi_ff2),

        .i_select(w_cart_config_select),
        .i_read_rq(w_bus_read_rq),
        .i_write_rq(w_bus_write_rq),
        .o_ack(w_cart_config_ack),
        .i_address(w_bus_address),
        .i_data(w_bus_o_data),
        .o_data(w_cart_config_o_data),

        .i_n64_disabled(w_n64_disable),

        .o_flash_enable(w_flash_enable),
        .o_sdram_enable(w_sdram_enable),
        .o_eeprom_pi_enable(w_eeprom_pi_enable),
        .o_eeprom_enable(w_eeprom_enable),
        .o_eeprom_16k_enable(w_eeprom_16k_enable)
    );

    flash flash_inst (
        .i_clk(w_sys_clk),
        .i_reset(w_sys_reset),

        .o_flash_clk(o_flash_clk),
        .o_flash_cs(o_flash_cs),
        .i_flash_dq(io_flash_dq),
        .o_flash_dq(w_flash_dq_o),
        .o_flash_dq_mode(w_flash_dq_mode),

        .i_select(w_flash_select),
        .i_cfg_select(w_flash_cfg_select),
        .i_read_rq(w_bus_read_rq),
        .i_write_rq(w_bus_write_rq),
        .o_ack(w_flash_ack),
        .i_address(w_bus_address),
        .i_data(w_bus_o_data),
        .o_data(w_flash_o_data)
    );

    sdram sdram_inst (
        .i_clk(w_sys_clk),
        .i_reset(w_sys_reset),

        .o_sdram_cs(o_sdram_cs),
        .o_sdram_ras(o_sdram_ras),
        .o_sdram_cas(o_sdram_cas),
        .o_sdram_we(o_sdram_we),
        .o_sdram_ba(o_sdram_ba),
        .o_sdram_a(o_sdram_a),
        .i_sdram_dq(io_sdram_dq),
        .o_sdram_dq(w_sdram_dq_o),
        .o_sdram_dq_mode(w_sdram_dq_mode),

        .i_select(w_sdram_select),
        .i_read_rq(w_bus_read_rq),
        .i_write_rq(w_bus_write_rq),
        .o_ack(w_sdram_ack),
        .i_address(w_bus_address),
        .i_data(w_bus_o_data),
        .o_data(w_sdram_o_data)
    );

    // LED

    localparam ROLLING_LED_WIDTH = 8;

    reg [(ROLLING_LED_WIDTH-1):0] r_rolling_led;

    assign o_led = |r_rolling_led;

    always @(posedge w_sys_clk or posedge w_sys_reset) begin
        if (w_sys_reset) r_rolling_led <= {(ROLLING_LED_WIDTH){1'b0}};
        else r_rolling_led <= {r_rolling_led[(ROLLING_LED_WIDTH-2):0], w_bus_active};
    end

endmodule
