module sd_regs (
    input i_clk,
    input i_reset,

    output reg [1:0] o_sd_clk_config,

    output reg [5:0] o_command_index,
    output reg [31:0] o_command_argument,
    output reg o_command_long_response,
    output reg o_command_skip_response,
    input [5:0] i_command_index,
    input [31:0] i_command_response,
    output reg o_command_start,
    input i_command_busy,
    input i_command_timeout,
    input i_command_response_crc_error,

    output reg o_dat_width,
    output reg o_dat_direction,
    output reg [6:0] o_dat_block_size,
    output reg [10:0] o_dat_num_blocks,
    output reg o_dat_start,
    output reg o_dat_stop,
    input i_dat_busy,
    input i_dat_crc_error,

    output reg o_rx_fifo_flush,
    output reg o_rx_fifo_pop,
    input [7:0] i_rx_fifo_items,
    input i_rx_fifo_overrun,
    input [31:0] i_rx_fifo_data,

    output reg o_tx_fifo_flush,
    output reg o_tx_fifo_push,
    input i_tx_fifo_empty,
    input i_tx_fifo_full,
    output reg [31:0] o_tx_fifo_data,

    output reg [3:0] o_dma_bank,
    output reg [23:0] o_dma_address,
    output reg [17:0] o_dma_length,
    input [3:0] i_dma_bank,
    input [23:0] i_dma_address,
    input [17:0] i_dma_left,
    output reg o_dma_load_bank_address,
    output reg o_dma_load_length,
    output reg o_dma_direction,
    output reg o_dma_start,
    output reg o_dma_stop,
    input i_dma_busy,

    input i_request,
    input i_write,
    output o_busy,
    output reg o_ack,
    input [3:0] i_address,
    output reg [31:0] o_data,
    input [31:0] i_data
);

    localparam [2:0] SD_REG_SCR         = 3'd0;
    localparam [2:0] SD_REG_ARG         = 3'd1;
    localparam [2:0] SD_REG_CMD         = 3'd2;
    localparam [2:0] SD_REG_RSP         = 3'd3;
    localparam [2:0] SD_REG_DAT         = 3'd4;
    localparam [2:0] SD_REG_DMA_SCR     = 3'd5;
    localparam [2:0] SD_REG_DMA_ADDR    = 3'd6;
    localparam [2:0] SD_REG_DMA_LEN     = 3'd7;

    wire w_write_request = i_request && i_write && !o_busy;
    wire w_read_request = i_request && !i_write && !o_busy;

    always @(*) begin
        o_dma_bank = i_data[31:28];
        o_dma_address = i_data[25:2];
        o_dma_length = i_data[17:0];
        o_dma_load_bank_address = w_write_request && !i_address[3] && (i_address[2:0] == SD_REG_DMA_ADDR);
        o_dma_load_length = w_write_request && !i_address[3] && (i_address[2:0] == SD_REG_DMA_LEN);
        o_busy = 1'b0;
    end

    always @(posedge i_clk) begin
        o_command_start <= 1'b0;
        o_dat_start <= 1'b0;
        o_rx_fifo_flush <= 1'b0;
        o_tx_fifo_flush <= 1'b0;
        o_tx_fifo_push <= 1'b0;
        o_dma_start <= 1'b0;
        o_dat_stop <= 1'b0;
        o_dma_stop <= 1'b0;
        
        if (i_reset) begin
            o_sd_clk_config <= 2'd0;
            o_dat_width <= 1'b0;
            o_dat_direction <= 1'b0;
            o_dat_block_size <= 7'd0;
            o_dat_num_blocks <= 11'd0;
            o_dma_direction <= 1'b0;
        end else if (w_write_request) begin
            if (!i_address[3]) begin
                case (i_address[2:0])
                    SD_REG_SCR: begin
                        {o_dat_width, o_sd_clk_config} <= i_data[2:0];
                    end

                    SD_REG_ARG: begin
                        o_command_argument <= i_data;
                    end

                    SD_REG_CMD: begin
                        {
                            o_command_skip_response,
                            o_command_long_response,
                            o_command_start,
                            o_command_index
                        } <= i_data[8:0];
                    end

                    SD_REG_RSP: begin
                    end

                    SD_REG_DAT: begin
                        {
                            o_tx_fifo_flush,
                            o_rx_fifo_flush,
                            o_dat_num_blocks,
                            o_dat_block_size,
                            o_dat_direction,
                            o_dat_stop,
                            o_dat_start
                        } <= i_data[22:0];
                    end

                    SD_REG_DMA_SCR: begin
                        {
                            o_dma_direction,
                            o_dma_stop,
                            o_dma_start
                        } <= i_data[2:0];
                    end

                    SD_REG_DMA_ADDR: begin
                    end

                    SD_REG_DMA_LEN: begin
                    end
                endcase
            end else begin
                o_tx_fifo_push <= 1'b1;
                o_tx_fifo_data <= i_data;
            end
        end
    end

    always @(posedge i_clk) begin
        o_rx_fifo_pop <= 1'b0;
        o_ack <= 1'b0;

        if (i_reset) begin
            o_data <= 32'h0000_0000;
        end else if (w_read_request) begin
            o_ack <= 1'b1;

            if (!i_address[3]) begin
                case (i_address[2:0])
                    SD_REG_SCR: begin
                        o_data <= {29'd0, o_dat_width, o_sd_clk_config};
                    end

                    SD_REG_ARG: begin
                        o_data <= o_command_argument;
                    end

                    SD_REG_CMD: begin
                        o_data <= {
                            21'd0,
                            i_command_response_crc_error,
                            i_command_timeout,
                            o_command_skip_response,
                            o_command_long_response,
                            i_command_busy,
                            i_command_index
                        };
                    end

                    SD_REG_RSP: begin
                        o_data <= i_command_response;
                    end

                    SD_REG_DAT: begin
                        o_data <= {
                            i_rx_fifo_items,
                            i_tx_fifo_full,
                            i_tx_fifo_empty,
                            i_rx_fifo_overrun,
                            o_dat_num_blocks,
                            o_dat_block_size,
                            o_dat_direction,
                            i_dat_crc_error,
                            i_dat_busy
                        };
                    end

                    SD_REG_DMA_SCR: begin
                        o_data <= {29'd0, o_dma_direction, 1'b0, i_dma_busy};
                    end

                    SD_REG_DMA_ADDR: begin
                        o_data <= {i_dma_bank, 2'd0, i_dma_address, 2'b00};
                    end

                    SD_REG_DMA_LEN: begin
                        o_data <= {14'd0, i_dma_left};
                    end
                endcase
            end else begin
                o_rx_fifo_pop <= 1'b1;
                o_data <= i_rx_fifo_data;
            end
        end
    end

endmodule
