module n64_cic (
    input clk,
    input reset,

    n64_scb.cic n64_scb,

    input n64_reset,
    input n64_cic_clk,
    inout n64_cic_dq
);

    // Input/output synchronization

    logic [1:0] n64_reset_ff;
    logic [1:0] n64_cic_clk_ff;
    logic [1:0] n64_cic_dq_ff;

    always_ff @(posedge clk) begin
        n64_reset_ff <= {n64_reset_ff[0], n64_reset};
        n64_cic_clk_ff <= {n64_cic_clk_ff[0], n64_cic_clk};
        n64_cic_dq_ff <= {n64_cic_dq_ff[0], n64_cic_dq};
    end

    logic cic_reset;
    logic cic_clk;
    logic cic_dq;

    always_comb begin
        cic_reset = n64_reset_ff[1];
        cic_clk = n64_cic_clk_ff[1];
        cic_dq = n64_cic_dq_ff[1];
    end

    logic cic_dq_out;

    assign n64_cic_dq = cic_dq_out ? 1'bZ : 1'b0;


    // SERV RISC-V CPU

    logic [31:0] ibus_addr;
    logic ibus_cycle;
    logic [31:0] ibus_rdata;
    logic ibus_ack;

    logic [31:0] dbus_addr;
    logic [31:0] dbus_wdata;
    logic [3:0] dbus_wmask;
    logic dbus_write;
    logic dbus_cycle;
    logic [31:0] dbus_rdata;
    logic dbus_ack;

    logic [31:0] ext_rs1;
    logic [31:0] ext_rs2;
    logic [2:0] ext_funct3;
    logic mdu_valid;

    serv_rf_top #(
        .RESET_PC(32'h8000_0000),
        .PRE_REGISTER(0),
        .WITH_CSR(0)
    ) serv_rf_top_inst (
        .clk(clk),
        .i_rst(reset),
        .i_timer_irq(1'b0),

        .o_ibus_adr(ibus_addr),
        .o_ibus_cyc(ibus_cycle),
        .i_ibus_rdt(ibus_rdata),
        .i_ibus_ack(ibus_ack),

        .o_dbus_adr(dbus_addr),
        .o_dbus_dat(dbus_wdata),
        .o_dbus_sel(dbus_wmask),
        .o_dbus_we(dbus_write) ,
        .o_dbus_cyc(dbus_cycle),
        .i_dbus_rdt(dbus_rdata),
        .i_dbus_ack(dbus_ack),

        .o_ext_rs1(ext_rs1),
        .o_ext_rs2(ext_rs2),
        .o_ext_funct3(ext_funct3),
        .i_ext_rd(32'd0),
        .i_ext_ready(1'b0),
        .o_mdu_valid(mdu_valid)
    );


    // CPU memory

    logic [8:0] ram_addr;
    logic [31:0] ram [0:511];
    logic [31:0] ram_output;

    assign ram_addr = ibus_cycle ? ibus_addr[10:2] : dbus_addr[10:2];
    assign ibus_rdata = ram_output;

    always_ff @(posedge clk) begin
        ram_output <= ram[ram_addr];

        ibus_ack <= ibus_cycle && !ibus_ack;
    end

    initial begin
        $readmemh("../../../sw/cic/build/cic.mem", ram);
    end


    // Bus controller

    always_ff @(posedge clk) begin
        n64_scb.cic_invalid_region <= 1'b0;

        dbus_ack <= dbus_cycle && !dbus_ack;

        if (dbus_cycle && dbus_write) begin
            case (dbus_addr[31:30])
                2'b10: begin
                    if (dbus_wmask[0]) ram[ram_addr][7:0] <= dbus_wdata[7:0];
                    if (dbus_wmask[1]) ram[ram_addr][15:8] <= dbus_wdata[15:8];
                    if (dbus_wmask[2]) ram[ram_addr][23:16] <= dbus_wdata[23:16];
                    if (dbus_wmask[3]) ram[ram_addr][31:24] <= dbus_wdata[31:24];
                end

                2'b11: begin
                    case (dbus_addr[3:2])
                        2'b10: begin
                            n64_scb.cic_invalid_region <= dbus_wdata[3];
                            cic_dq_out <= dbus_wdata[0];
                        end
                    endcase
                end
            endcase
        end

        if (reset || !cic_reset) begin
            cic_dq_out <= 1'b1;
        end
    end

    always_comb begin
        dbus_rdata = 32'd0;

        case (dbus_addr[31:30])
            2'b10: begin
                dbus_rdata = ram_output;
            end

            2'b11: begin
                case (dbus_addr[3:2])
                    2'b00: dbus_rdata = {
                        n64_scb.cic_disabled,
                        n64_scb.cic_64dd_mode,
                        n64_scb.cic_region,
                        n64_scb.cic_seed,
                        n64_scb.cic_checksum[47:32]
                    };

                    2'b01: dbus_rdata = n64_scb.cic_checksum[31:0];

                    2'b10: dbus_rdata = {29'd0, cic_reset, cic_clk, cic_dq};
                endcase
            end
        endcase
    end

endmodule
