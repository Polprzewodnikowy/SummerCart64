module address_decoder (
    input [31:0] i_address,

    output o_cart_config,
    output o_flash,
    output o_flash_cfg,
    output o_sdram,

    input i_flash_enable,
    input i_sdram_enable,

    output o_address_valid
);

    localparam CART_CONFIG_ADDR = 32'h1E00_0000;
    localparam CART_CONFIG_WIDTH = 8;

    assign o_cart_config = i_address[31:CART_CONFIG_WIDTH] == CART_CONFIG_ADDR[31:CART_CONFIG_WIDTH];

    localparam FLASH_ADDR = 32'h1000_0000;
    localparam FLASH_REMAP_ADDR = 32'h1800_0000;
    localparam FLASH_WIDTH = 24;

    assign o_flash = i_flash_enable && (i_address[31:FLASH_WIDTH] == (i_sdram_enable ? FLASH_REMAP_ADDR[31:FLASH_WIDTH] : FLASH_ADDR[31:FLASH_WIDTH]));

    localparam FLASH_CFG_ADDR = 32'h1C00_0000;

    assign o_flash_cfg = i_flash_enable && (i_address == FLASH_CFG_ADDR);

    localparam SDRAM_ADDR = 32'h1000_0000;
    localparam SDRAM_WIDTH = 26;

    assign o_sdram = i_sdram_enable && (i_address[31:SDRAM_WIDTH] == SDRAM_ADDR[31:SDRAM_WIDTH]);

    assign o_address_valid = (|{o_cart_config, o_flash, o_flash_cfg, o_sdram});

endmodule
