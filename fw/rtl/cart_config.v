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
    output [31:0] o_data,

    input i_n64_disabled,

    output o_flash_enable,
    output o_sdram_enable
);

    reg [1:0] r_cart_config;
    reg [7:0] r_cic_type;

    wire [31:0] w_regs [1:0];
    assign w_regs[0] = {30'd0, r_cart_config};
    assign w_regs[1] = {24'd0, r_cic_type};

    assign o_data = w_regs[i_address[2]];

    assign o_flash_enable = r_cart_config[0];
    assign o_sdram_enable = r_cart_config[1];

    reg r_last_n64_reset;
    reg r_last_n64_nmi;

    wire w_n64_reset_op = !i_n64_disabled && !r_last_n64_reset && i_n64_reset;
    wire w_n64_nmi_op = !i_n64_disabled && !r_last_n64_nmi && i_n64_nmi;

    always @(posedge i_clk or posedge i_reset) begin
        if (i_reset) begin
            r_last_n64_reset <= 1'b0;
            r_last_n64_nmi <= 1'b0;
        end else begin
            r_last_n64_reset <= i_n64_reset;
            r_last_n64_nmi <= i_n64_nmi;
        end
    end

    always @(posedge i_clk or posedge i_reset or posedge w_n64_reset_op or posedge w_n64_nmi_op) begin
        if (i_reset || w_n64_reset_op || w_n64_nmi_op) begin
            r_cart_config <= 2'b01;
        end else begin
            if (i_select && i_write_rq && !i_address[2]) r_cart_config <= i_data[1:0];
        end
    end

    always @(posedge i_clk or posedge i_reset) begin
        if (i_reset) begin
            r_cic_type <= 8'd0;
        end else begin
            if (i_select && i_write_rq && i_address[2]) r_cic_type <= i_data[7:0];
        end
    end

    always @(posedge i_clk) begin
        o_ack <= 1'b0;
        if (i_select && (i_read_rq || i_write_rq)) o_ack <= 1'b1;
    end

endmodule
