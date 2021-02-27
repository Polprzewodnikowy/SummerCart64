`include "../constants.vh"

module n64_bank_decoder (
    input i_clk,

    input i_address_high_op,
    input [15:0] i_n64_pi_ad,

    output reg [3:0] o_bank,
    output reg o_prefetch,
    output reg o_ddipl_pi_request,
    output reg o_sram_pi_request,
    output reg o_flashram_pi_request,

    input i_ddipl_enable,
    input i_sram_enable,
    input i_flashram_enable,
    input i_eeprom_pi_enable,
    input i_sd_enable
);

    always @(posedge i_clk) begin
        if (i_address_high_op) begin
            o_bank <= `BANK_INVALID;
            o_prefetch <= 1'b1;
            o_ddipl_pi_request <= 1'b0;
            o_sram_pi_request <= 1'b0;
            o_flashram_pi_request <= 1'b0;

            casez (i_n64_pi_ad)
                16'b0000011000??????: begin // DDIPL
                    if (i_ddipl_enable) begin
                        o_bank <= `BANK_SDRAM;
                        o_ddipl_pi_request <= 1'b1;
                    end
                end

                16'b000010000000000?: begin // SRAM / FlashRAM
                    if (i_flashram_enable) begin
                        o_bank <= `BANK_SDRAM;
                        o_flashram_pi_request <= 1'b1;
                    end else if (i_sram_enable) begin
                        o_bank <= `BANK_SDRAM;
                        o_sram_pi_request <= 1'b1;
                    end
                end

                16'b000100??????????: begin // ROM
                    o_bank <= `BANK_SDRAM;
                end

                16'b0001111000000000: begin // CART
                    o_bank <= `BANK_CART;
                    o_prefetch <= 1'b0;
                end

                16'b0001111000000001: begin // EEPROM
                    if (i_eeprom_pi_enable) begin
                        o_bank <= `BANK_EEPROM;
                    end
                end

                16'b0001111000000010: begin // SD
                    if (i_sd_enable) begin
                        o_bank <= `BANK_SD;
                        o_prefetch <= 1'b0;
                    end
                end

                default: begin end
            endcase
        end
    end

endmodule
