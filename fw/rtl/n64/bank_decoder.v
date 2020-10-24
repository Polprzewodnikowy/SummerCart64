module n64_bank_decoder (
    input [31:0] i_address,
    output reg [3:0] o_bank,
    output reg o_prefetch
);

    localparam [3:0] BANK_INVALID   = 4'd0;
    localparam [3:0] BANK_ROM       = 4'd1;
    localparam [3:0] BANK_CART      = 4'd2;
    localparam [3:0] BANK_EEPROM    = 4'd3;

    localparam [31:0] ROM_BASE      = 32'h1000_0000;
    localparam [31:0] ROM_END       = 32'h13FF_FFFF;

    localparam [31:0] CART_BASE     = 32'h18F0_0000;
    localparam [31:0] CART_END      = 32'h18FF_FFFF;

    localparam [31:0] EEPROM_BASE   = 32'h1D00_0000;
    localparam [31:0] EEPROM_END    = 32'h1D00_07FF;

    always @(*) begin
        o_bank = BANK_INVALID;
        o_prefetch = 1'b0;

        if (i_address >= ROM_BASE && i_address <= ROM_END) begin
            o_bank = BANK_ROM;
            o_prefetch = 1'b1;
        end

        if (i_address >= CART_BASE && i_address <= CART_END) begin
            o_bank = BANK_CART;
        end

        if (i_address >= EEPROM_BASE && i_address <= EEPROM_END) begin
            o_bank = BANK_EEPROM;
            o_prefetch = 1'b1;
        end
    end

endmodule
