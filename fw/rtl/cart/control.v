module cart_control (
    input i_clk,
    input i_reset,

    input i_n64_reset,
    input i_n64_nmi,

    input i_request,
    input i_write,
    output o_busy,
    output reg o_ack,
    input [0:0] i_address,
    output reg [31:0] o_data,
    input [31:0] i_data,

    output reg o_rom_switch,
    output reg o_eeprom_enable,
    output reg o_eeprom_16k_mode
);

    // Input synchronization

    reg r_reset_ff1, r_reset_ff2;
    reg r_nmi_ff1, r_nmi_ff2;

    always @(posedge i_clk) begin
        {r_reset_ff2, r_reset_ff1} <= {r_reset_ff1, i_n64_reset};
        {r_nmi_ff2, r_nmi_ff1} <= {r_nmi_ff1, i_n64_nmi};
    end


    // Registers

    reg [31:0] r_bootloader;


    // Bus controller

    assign o_busy = 1'b0;

    always @(posedge i_clk) begin
        if (i_reset) begin
            o_ack <= 1'b0;
        end else begin
            o_ack <= i_request && !i_write && !o_busy;
        end
    end

    always @(posedge i_clk) begin
        if (i_reset) begin
            o_rom_switch <= 1'b0;
            o_eeprom_enable <= 1'b0;
            o_eeprom_16k_mode <= 1'b0;
            r_bootloader <= 32'h0000_0000;
        end else begin
            if (i_request && i_write && !o_busy) begin
                case (i_address)
                    1'd0: {o_eeprom_16k_mode, o_eeprom_enable, o_rom_switch} <= {i_data[4], i_data[3], i_data[1]};
                    1'd1: r_bootloader <= i_data;
                endcase
            end
            if (!r_reset_ff2 || !r_nmi_ff2) begin
                o_rom_switch <= 1'b0;
            end
        end
    end

    always @(posedge i_clk) begin
        if (i_request && !i_write && !o_busy) begin
            case (i_address)
                1'd0: o_data <= {o_eeprom_16k_mode, o_eeprom_enable, 1'b0, o_rom_switch, 1'b0};
                1'd1: o_data <= r_bootloader;
            endcase
        end
    end

endmodule
