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
    output reg o_eeprom_enable,
    output reg o_eeprom_16k_mode,

    output reg o_n64_reset_btn,

    input i_debug_ready,

    output reg o_debug_dma_start,
    input i_debug_dma_busy,
    output reg [3:0] o_debug_dma_bank,
    output reg [25:0] o_debug_dma_address,
    output reg [19:0] o_debug_dma_length,

    output reg o_debug_fifo_request,
    output reg o_debug_fifo_flush,
    input [10:0] i_debug_fifo_items,
    input [31:0] i_debug_fifo_data
);

    // Module parameters

    parameter byte VERSION = "a";

    localparam [10:0] REG_SCR           = 11'd0;
    localparam [10:0] REG_BOOT          = 11'd1;
    localparam [10:0] REG_VERSION       = 11'd2;
    localparam [10:0] REG_GPIO          = 11'd3;
    localparam [10:0] REG_USB_SCR       = 11'd4;
    localparam [10:0] REG_USB_DMA_ADDR  = 11'd5;
    localparam [10:0] REG_USB_DMA_LEN   = 11'd6;

    localparam [10:0] MEM_USB_FIFO_BASE = 11'h400;
    localparam [10:0] MEM_USB_FIFO_END  = 11'h7FF;


    // Input synchronization

    reg r_reset_ff1, r_reset_ff2;
    reg r_nmi_ff1, r_nmi_ff2;

    always @(posedge i_clk) begin
        {r_reset_ff2, r_reset_ff1} <= {r_reset_ff1, i_n64_reset};
        {r_nmi_ff2, r_nmi_ff1} <= {r_nmi_ff1, i_n64_nmi};
    end


    // Registers

    reg [7:0] r_bootloader;


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
        o_debug_dma_start <= 1'b0;
        o_debug_fifo_flush <= 1'b0;

        if (i_reset) begin
            o_sdram_writable <= 1'b0;
            o_rom_switch <= 1'b0;
            o_eeprom_enable <= 1'b0;
            o_eeprom_16k_mode <= 1'b0;
            o_n64_reset_btn <= 1'b1;
            o_debug_dma_bank <= 4'd1;
            o_debug_dma_address <= 26'h3F0_0000;
            o_debug_dma_length <= 20'd0;
            r_bootloader <= 8'h00;
        end else begin
            if (i_request && i_write && !o_busy) begin
                case (i_address)
                    REG_SCR: begin
                        {o_eeprom_16k_mode, o_eeprom_enable, o_rom_switch, o_sdram_writable} <= {i_data[4:3], i_data[1:0]};
                    end
                    REG_BOOT: begin
                        r_bootloader <= i_data[7:0];
                    end
                    REG_GPIO: begin
                        o_n64_reset_btn <= ~i_data[0];
                    end
                    REG_USB_SCR: begin
                        {o_debug_fifo_flush, o_debug_dma_start} <= {i_data[2], i_data[0]};
                    end
                    REG_USB_DMA_ADDR: begin
                        {o_debug_dma_bank, o_debug_dma_address} <= {i_data[31:28], i_data[25:2], 2'b00};
                    end
                    REG_USB_DMA_LEN: begin
                        o_debug_dma_length <= i_data[19:0];
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

    always @(posedge i_clk) begin
        o_debug_fifo_request <= 1'b0;

        if (i_request && !i_write && !o_busy) begin
            case (i_address)
                REG_SCR: begin
                    o_data <= {27'd0, o_eeprom_16k_mode, o_eeprom_enable, 1'b0, o_rom_switch, o_sdram_writable};
                end
                REG_BOOT: begin
                    o_data <= {24'd0, r_bootloader};
                end
                REG_VERSION: begin
                    o_data <= {"S", "6", "4", VERSION};
                end
                REG_GPIO: begin
                    o_data <= {29'd0, r_nmi_ff2, r_reset_ff2, ~o_n64_reset_btn};
                end
                REG_USB_SCR: begin
                    o_data <= {18'd0, i_debug_fifo_items, 1'b0, i_debug_ready, i_debug_dma_busy};
                end
                REG_USB_DMA_ADDR: begin
                    o_data <= {o_debug_dma_bank, 2'b00, o_debug_dma_address};
                end
                REG_USB_DMA_LEN: begin
                    o_data <= {12'd0, o_debug_dma_length};
                end
                default: begin
                    o_data <= 32'h0000_0000;
                end
            endcase

            if ((i_address >= MEM_USB_FIFO_BASE) && (i_address <= MEM_USB_FIFO_END)) begin
                o_data <= i_debug_fifo_data;
                o_debug_fifo_request <= 1'b1;
            end
        end
    end

endmodule
