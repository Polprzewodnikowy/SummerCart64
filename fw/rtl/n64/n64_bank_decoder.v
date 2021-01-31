`include "../constants.vh"

module n64_bank_decoder (
    input [31:0] i_address,
    output reg [25:0] o_translated_address,
    output reg [3:0] o_bank,
    output reg o_bank_prefetch,
    output o_sram_request,
    input i_ddipl_enable,
    input i_sram_enable,
    input i_sram_768k_mode,
    input i_flashram_enable,
    input i_sd_enable,
    input i_eeprom_enable,
    input [23:0] i_ddipl_address,
    input [23:0] i_sram_address
);

    localparam [31:0] DDIPL_BASE        = 32'h0600_0000;
    localparam [31:0] DDIPL_END         = 32'h063F_FFFF;

    localparam [31:0] SRAM_BASE         = 32'h0800_0000;
    localparam [31:0] SRAM_END          = 32'h0800_7FFF;
    localparam [31:0] SRAM_768K_END     = 32'h0801_7FFF;

    localparam [31:0] ROM_BASE          = 32'h1000_0000;
    localparam [31:0] ROM_END           = 32'h13FF_FFFF;

    localparam [31:0] CART_BASE         = 32'h1E00_0000;
    localparam [31:0] CART_END          = 32'h1E00_3FFF;

    localparam [31:0] EEPROM_BASE       = 32'h1E00_4000;
    localparam [31:0] EEPROM_END        = 32'h1E00_47FF;

    localparam [31:0] SD_BASE           = 32'h1E00_8000;
    localparam [31:0] SD_END            = 32'h1E00_83FF;

    wire [25:0] w_ddipl_translated_address = i_address[25:0] + {i_ddipl_address, 2'd0};
    wire [25:0] w_sram_translated_address = i_address[25:0] + {i_sram_address, 2'd0};

    always @(*) begin
        o_bank = `BANK_INVALID;
        o_bank_prefetch = 1'b0;
        o_translated_address = i_address[25:0];
        o_sram_request = 1'b0;

        if ((i_address >= DDIPL_BASE) && (i_address <= DDIPL_END) && i_ddipl_enable) begin
            o_translated_address = w_ddipl_translated_address;
            o_bank = `BANK_SDRAM;
            o_bank_prefetch = 1'b1;
        end

        if ((i_address >= SRAM_BASE) && ((i_address <= SRAM_END) || ((i_sram_768k_mode && (i_address <= SRAM_768K_END))))) begin
            if (i_sram_enable && !i_flashram_enable) begin
                o_translated_address = w_sram_translated_address;
                o_bank = `BANK_SDRAM;
                o_bank_prefetch = 1'b1;
                o_sram_request = 1'b1;
            end
        end

        if ((i_address >= ROM_BASE) && (i_address <= ROM_END)) begin
            o_bank = `BANK_SDRAM;
            o_bank_prefetch = 1'b1;
        end

        if ((i_address >= CART_BASE) && (i_address <= CART_END)) begin
            o_bank = `BANK_CART;
        end

        if ((i_address >= EEPROM_BASE) && (i_address <= EEPROM_END) && i_eeprom_enable) begin
            o_bank = `BANK_EEPROM;
            o_bank_prefetch = 1'b1;
        end

        if ((i_address >= SD_BASE) && (i_address <= SD_END) && i_sd_enable) begin
            o_bank = `BANK_SD;
        end
    end

endmodule
