module cart_control (
    input i_clk,
    input i_reset,

    input i_n64_reset,
    input i_n64_nmi,

    input i_request,
    input i_write,
    output o_busy,
    output reg o_ack,
    input [10:0] i_address,
    output reg [31:0] o_data,
    input [31:0] i_data,

    output reg o_sdram_writable,
    output reg o_rom_switch,
    output reg o_ddipl_enable,
    output reg o_sram_enable,
    output reg o_flashram_enable,
    output reg o_sd_enable,
    output reg o_eeprom_pi_enable,
    output reg o_eeprom_enable,
    output reg o_eeprom_16k_mode,

    output reg o_n64_reset_btn,

    input i_debug_ready,

    output reg o_debug_dma_start,
    input i_debug_dma_busy,
    output reg [3:0] o_debug_dma_bank,
    output reg [23:0] o_debug_dma_address,
    output reg [19:0] o_debug_dma_length,

    output reg o_debug_fifo_request,
    output reg o_debug_fifo_flush,
    input [10:0] i_debug_fifo_items,
    input [31:0] i_debug_fifo_data,
    
    output reg [23:0] o_ddipl_address,
    output reg [23:0] o_sram_address
);

    // Module parameters

    parameter byte VERSION = "a";


    // Register offsets

    localparam [3:0] REG_SCR            = 4'd0;
    localparam [3:0] REG_BOOT           = 4'd1;
    localparam [3:0] REG_VERSION        = 4'd2;
    localparam [3:0] REG_GPIO           = 4'd3;
    localparam [3:0] REG_USB_SCR        = 4'd4;
    localparam [3:0] REG_USB_DMA_ADDR   = 4'd5;
    localparam [3:0] REG_USB_DMA_LEN    = 4'd6;
    localparam [3:0] REG_DDIPL_ADDR     = 4'd7;
    localparam [3:0] REG_SRAM_ADDR      = 4'd8;

    localparam [10:0] MEM_USB_FIFO_BASE = 11'h400;


    // Input synchronization

    reg r_reset_ff1, r_reset_ff2;
    reg r_nmi_ff1, r_nmi_ff2;

    always @(posedge i_clk) begin
        {r_reset_ff2, r_reset_ff1} <= {r_reset_ff1, i_n64_reset};
        {r_nmi_ff2, r_nmi_ff1} <= {r_nmi_ff1, i_n64_nmi};
    end


    // Registers

    reg [15:0] r_bootloader;


    // Bus controller

    assign o_busy = 1'b0;

    always @(posedge i_clk) begin
        o_ack <= !i_reset && i_request && !i_write && !o_busy;
    end


    // Write logic

    always @(posedge i_clk) begin
        o_debug_dma_start <= 1'b0;
        o_debug_fifo_flush <= 1'b0;

        if (i_reset) begin
            o_sdram_writable <= 1'b0;
            o_rom_switch <= 1'b0;
            o_ddipl_enable <= 1'b0;
            o_sram_enable <= 1'b0;
            o_flashram_enable <= 1'b0;
            o_sd_enable <= 1'b0;
            o_eeprom_pi_enable <= 1'b0;
            o_eeprom_enable <= 1'b0;
            o_eeprom_16k_mode <= 1'b0;
            o_n64_reset_btn <= 1'b1;
            o_ddipl_address <= 24'hF0_0000;
            o_sram_address <= 24'hFF_E000;
            o_debug_dma_bank <= 4'd1;
            o_debug_dma_address <= 24'hFC_0000;
            o_debug_dma_length <= 20'd0;
            r_bootloader <= 16'h0000;
        end else begin
            if (i_request && i_write && !o_busy) begin
                case (i_address[3:0])
                    REG_SCR: begin
                        {
                            o_flashram_enable,
                            o_sram_enable,
                            o_sd_enable,
                            o_eeprom_pi_enable,
                            o_eeprom_16k_mode,
                            o_eeprom_enable,
                            o_ddipl_enable,
                            o_rom_switch,
                            o_sdram_writable
                        } <= i_data[8:0];
                    end
                    REG_BOOT: begin
                        r_bootloader <= i_data[15:0];
                    end
                    REG_GPIO: begin
                        o_n64_reset_btn <= ~i_data[0];
                    end
                    REG_USB_SCR: begin
                        {o_debug_fifo_flush, o_debug_dma_start} <= {i_data[2], i_data[0]};
                    end
                    REG_USB_DMA_ADDR: begin
                        {o_debug_dma_bank, o_debug_dma_address} <= {i_data[31:28], i_data[25:2]};
                    end
                    REG_USB_DMA_LEN: begin
                        o_debug_dma_length <= i_data[19:0];
                    end
                    REG_DDIPL_ADDR: begin
                        o_ddipl_address <= i_data[25:2];
                    end
                    REG_SRAM_ADDR: begin
                        o_sram_address <= i_data[25:2];
                    end
                    default: begin
                    end
                endcase
            end

            if (!r_reset_ff2 || !r_nmi_ff2) begin
                o_sdram_writable <= 1'b0;
                o_rom_switch <= 1'b0;
                o_n64_reset_btn <= 1'b1;
                o_debug_fifo_flush <= 1'b1;
            end
        end
    end


    // Read logic

    always @(posedge i_clk) begin
        o_debug_fifo_request <= 1'b0;

        if (!i_reset && i_request && !i_write && !o_busy) begin
            if (i_address < MEM_USB_FIFO_BASE) begin
                case (i_address[3:0])
                    REG_SCR: begin
                        o_data[8:0] <= {
                            o_flashram_enable,
                            o_sram_enable,
                            o_sd_enable,
                            o_eeprom_pi_enable,
                            o_eeprom_16k_mode,
                            o_eeprom_enable,
                            o_ddipl_enable,
                            o_rom_switch,
                            o_sdram_writable
                        };
                    end
                    REG_BOOT: begin
                        o_data[15:0] <= r_bootloader;
                    end
                    REG_VERSION: begin
                        o_data <= {"S", "6", "4", VERSION};
                    end
                    REG_GPIO: begin
                        o_data[2:0] <= {r_nmi_ff2, r_reset_ff2, ~o_n64_reset_btn};
                    end
                    REG_USB_SCR: begin
                        {o_data[13:3], o_data[1:0]} <= {i_debug_fifo_items, i_debug_ready, i_debug_dma_busy};
                    end
                    default: begin
                    end
                endcase
            end else begin
                o_data <= i_debug_fifo_data;
                o_debug_fifo_request <= 1'b1;
            end
        end
    end

endmodule
