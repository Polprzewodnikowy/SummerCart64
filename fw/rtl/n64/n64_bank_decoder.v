`include "../constants.vh"

module n64_bank_decoder (
    input i_clk,

    input i_address_high_op,
    input i_address_low_op,
    input [15:0] i_n64_pi_ad,

    output reg [3:0] o_bank,
    output reg o_prefetch,
    output reg o_ddipl_request,
    output reg o_sram_request,

    input i_ddipl_enable,
    input i_sram_enable,
    input i_sram_768k_mode,
    input i_flashram_enable,
    input i_eeprom_enable,
    input i_sd_enable
);

    reg r_address_high_lsb;

    always @(posedge i_clk) begin
        if (i_address_high_op) begin
            o_bank <= `BANK_INVALID;
            o_prefetch <= 1'b1;
            o_ddipl_request <= 1'b0;
            o_sram_request <= 1'b0;

            casez (i_n64_pi_ad)
                16'b0000011000??????: begin // DDIPL
                    if (i_ddipl_enable) begin
                        o_bank <= `BANK_SDRAM;
                        o_ddipl_request <= 1'b1;
                    end
                end

                16'b000010000000000?: begin // SRAM / FlashRAM
                    r_address_high_lsb <= i_n64_pi_ad[0];
                    if (i_flashram_enable && !i_n64_pi_ad[0]) begin
                        o_bank <= `BANK_FLASHRAM;
                    end else if (i_sram_enable) begin
                        o_bank <= `BANK_SDRAM;
                        o_sram_request <= 1'b1;
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
                    if (i_eeprom_enable) begin
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

        if (i_address_low_op) begin
            case (o_bank)
                `BANK_SDRAM: begin
                    if (o_sram_request) begin
                        if (i_sram_768k_mode) begin
                            if (r_address_high_lsb && i_n64_pi_ad[1]) begin
                                o_bank <= `BANK_INVALID;
                            end
                        end else begin
                            if (i_n64_pi_ad[1]) begin
                                o_bank <= `BANK_INVALID;
                            end
                        end
                    end
                end

                `BANK_CART: begin
                    if (i_n64_pi_ad[15:14] != 2'b00) begin
                        o_bank <= `BANK_INVALID;
                    end
                end

                `BANK_EEPROM: begin
                    if (i_n64_pi_ad[15:11] != 5'b00000) begin
                        o_bank <= `BANK_INVALID;
                    end
                end
 
                `BANK_FLASHRAM: begin
                    if (r_address_high_lsb) begin
                        o_bank <= `BANK_INVALID;
                    end
                end

                `BANK_SD: begin
                    if (i_n64_pi_ad[15:10] != 6'b000000) begin
                        o_bank <= `BANK_INVALID;
                    end
                end

                default: begin end
            endcase
        end
    end

endmodule
