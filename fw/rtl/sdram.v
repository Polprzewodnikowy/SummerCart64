module sdram (
    input i_clk,
    input i_reset,

    output o_sdram_cs,
    output o_sdram_ras,
    output o_sdram_cas,
    output o_sdram_we,
    output [1:0] o_sdram_ba,
    output [12:0] o_sdram_a,
    input [15:0] i_sdram_dq,
    output [15:0] o_sdram_dq,
    output o_sdram_dq_mode,

    input i_select,
    input i_read_rq,
    input i_write_rq,
    output reg o_ack,
    input [31:0] i_address,
    input [31:0] i_data,
    output reg [31:0] o_data
);

    reg r_wb_cyc;
    reg r_wb_stb;

    wire w_wb_cyc;
    wire w_wb_stb;
    wire [31:0] w_wb_addr;
    wire w_wb_ack;
    wire w_wb_stall;
    wire [31:0] w_wb_o_data;

    assign w_wb_cyc = w_wb_stb || r_wb_cyc;
    assign w_wb_stb = i_select && !w_wb_stall && (i_read_rq || i_write_rq || r_wb_stb);

    always @(posedge i_clk or posedge i_reset) begin
        if (i_reset) begin
            r_wb_cyc <= 1'b0;
        end else begin
            if (w_wb_stb) begin
                r_wb_cyc <= 1'b1;
            end else if (w_wb_ack) begin
                r_wb_cyc <= 1'b0;
            end
        end
    end

    // FIXME: Ugly solution for 16-bit address read align

    reg r_unaligned_access;
    reg r_next_word;

    assign w_wb_addr = i_address + {r_next_word, 2'b00};

    always @(posedge i_clk or posedge i_reset) begin
        if (i_reset) begin
            o_ack <= 1'b0;
            r_wb_stb <= 1'b0;
            r_unaligned_access <= 1'b0;
            r_next_word <= 1'b0;
        end else begin
            o_ack <= 1'b0;
            r_wb_stb <= 1'b0;

            if (i_read_rq) begin
                if (!i_address[1]) begin
                    r_unaligned_access <= 1'b0;
                    r_next_word <= 1'b0;
                end else begin
                    r_unaligned_access <= 1'b1;
                    r_next_word <= 1'b0;
                end
            end
            if (w_wb_ack) begin
                if (!r_unaligned_access) begin
                    o_data <= w_wb_o_data;
                    o_ack <= 1'b1;
                end else begin
                    if (!r_next_word) begin
                        o_data[31:16] <= w_wb_o_data[15:0];
                        r_wb_stb <= 1'b1;
                        r_next_word <= 1'b1;
                    end else begin
                        o_ack <= 1'b1;
                        o_data[15:0] <= w_wb_o_data[31:16];
                        r_unaligned_access <= 1'b0;
                        r_next_word <= 1'b0;
                    end
                end
            end
        end
    end

    // Weird shift register required by this module
    // https://github.com/ZipCPU/arrowzip/blob/master/rtl/arrowzip/toplevel.v#L176

    reg	[15:0] r_sdram_dq_ext_clk;
	reg	[15:0] r_sdram_dq;

    always @(posedge i_clk) begin
        {r_sdram_dq, r_sdram_dq_ext_clk} <= {r_sdram_dq_ext_clk, i_sdram_dq};
    end

    wbsdram wbsdram_inst (
        .i_clk(i_clk),

        .i_wb_cyc(w_wb_cyc),
        .i_wb_stb(w_wb_stb),            // FIXME: Currently strobe can be missed when w_wb_stall is high
        .i_wb_we(i_write_rq),
        .i_wb_addr(w_wb_addr[31:2]),
        .i_wb_data(i_data),
        .i_wb_sel({4'b1111}),           // No need to control this signal
        .o_wb_ack(w_wb_ack),
        .o_wb_stall(w_wb_stall),
        .o_wb_data(w_wb_o_data),

        .o_ram_cs_n(o_sdram_cs),
        // .o_ram_cke(),                // No connection on PCB
        .o_ram_ras_n(o_sdram_ras),
        .o_ram_cas_n(o_sdram_cas),
        .o_ram_we_n(o_sdram_we),
        .o_ram_bs(o_sdram_ba),
        .o_ram_addr(o_sdram_a),
        .o_ram_dmod(o_sdram_dq_mode),
        .i_ram_data(r_sdram_dq),
        .o_ram_data(o_sdram_dq)
        // .o_ram_dqm(),                // No connection on PCB

        // .o_debug()                   // No need for this signal
    );

endmodule
