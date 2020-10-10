module cart_config (
    input i_clk,
    input i_reset,

    input i_n64_reset,
    input i_n64_nmi,

    input i_select,
    input i_read_rq,
    input i_write_rq,
    output reg o_ack,
    input [31:0] i_address,
    input [31:0] i_data,
    output reg [31:0] o_data,

    input i_n64_disabled,

    output o_flash_enable,
    output o_sdram_enable,
    output o_eeprom_pi_enable,
    output o_eeprom_enable,
    output o_eeprom_16k_enable
);

    reg [4:0] r_cart_config;
    reg [7:0] r_cic_type;

    always @(*) begin
        o_data = 32'd0;
        if (!i_address[2]) o_data = {27'd0, r_cart_config};
        if (i_address[2]) o_data = {24'd0, r_cic_type};
    end

    assign {
        o_eeprom_16k_enable,
        o_eeprom_enable,
        o_eeprom_pi_enable,
        o_sdram_enable,
        o_flash_enable,
    } = r_cart_config;

    reg r_last_n64_reset;
    reg r_last_n64_nmi;

    wire w_n64_reset_op = !i_n64_disabled && ((!r_last_n64_reset && i_n64_reset) || (!r_last_n64_nmi && i_n64_nmi));

    always @(posedge i_clk) begin
        r_last_n64_reset <= i_n64_reset;
        r_last_n64_nmi <= i_n64_nmi;
    end

    always @(posedge i_clk) begin
        if (i_reset) r_cart_config[4:0] <= 5'b00001;
        if (w_n64_reset_op) r_cart_config[2:0] <= 3'b001;
        if (!i_reset && i_select && i_write_rq && !i_address[2]) r_cart_config <= i_data[4:0];
    end

    always @(posedge i_clk) begin
        if (i_reset) r_cic_type <= 8'd0;
        if (!i_reset && i_select && i_write_rq && i_address[2]) r_cic_type <= i_data[7:0];
    end

    always @(posedge i_clk) begin
        o_ack <= !i_reset && i_select && (i_read_rq || i_write_rq);
    end

endmodule
