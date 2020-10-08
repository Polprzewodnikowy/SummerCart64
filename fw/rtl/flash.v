module flash (
    input i_clk,
    input i_reset,

    output o_flash_clk,
    output o_flash_cs,
    input [3:0] i_flash_dq,
    output [3:0] o_flash_dq,
    output [1:0] o_flash_dq_mode,

    input i_select,
    input i_cfg_select,
    input i_read_rq,
    input i_write_rq,
    output o_ack,
    input [31:0] i_address,
    input [31:0] i_data,
    output [31:0] o_data
);

    reg r_wb_cyc;

    wire w_wb_cyc;
    wire w_stb;
    wire w_wb_stb;
    wire w_cfg_stb;
    wire w_wb_ack;
    wire w_wb_stall;

    assign w_wb_cyc = w_wb_stb || w_cfg_stb || r_wb_cyc;
    assign w_stb = !w_wb_stall && (i_read_rq || i_write_rq);
    assign w_wb_stb = i_select && w_stb;
    assign w_cfg_stb = i_cfg_select && w_stb;
    assign o_ack = w_wb_ack;

    always @(posedge i_clk or posedge i_reset) begin
        if (i_reset) begin
            r_wb_cyc <= 1'b0;
        end else begin
            if (w_wb_stb || w_cfg_stb) begin
                r_wb_cyc <= 1'b1;
            end else if (w_wb_ack) begin
                r_wb_cyc <= 1'b0;
            end
        end
    end

    qflexpress qflexpress_inst(
        .i_clk(i_clk),
        .i_reset(i_reset),

        .i_wb_cyc(w_wb_cyc),
        .i_wb_stb(w_wb_stb),            // FIXME: Currently strobe can be missed when w_wb_stall is high
        .i_cfg_stb(w_cfg_stb),
        .i_wb_we(i_write_rq),
        .i_wb_addr(i_address[31:1]),
        .i_wb_data(i_data),
        .o_wb_ack(w_wb_ack),
        .o_wb_stall(w_wb_stall),
        .o_wb_data(o_data),

        .o_qspi_sck(o_flash_clk),
        .o_qspi_cs_n(o_flash_cs),
        .o_qspi_mod(o_flash_dq_mode),
        .o_qspi_dat(o_flash_dq),
        .i_qspi_dat(i_flash_dq)
    );
    defparam
        qflexpress_inst.LGFLASHSZ = 24,
        qflexpress_inst.OPT_PIPE = 0,
        qflexpress_inst.OPT_CFG = 1,
        qflexpress_inst.OPT_STARTUP = 1,
        qflexpress_inst.OPT_CLKDIV = 1,
        qflexpress_inst.OPT_ENDIANSWAP = 0,
        qflexpress_inst.RDDELAY = 0,
        qflexpress_inst.NDUMMY = 6,
        qflexpress_inst.OPT_STARTUP_FILE = "";

endmodule
