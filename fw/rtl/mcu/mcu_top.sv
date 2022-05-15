module mcu_top (
    input clk,
    input reset,

    usb_scb.controller usb_scb,
    dma_scb.controller usb_dma_scb,
    sd_scb.controller sd_scb,
    dma_scb.controller sd_dma_scb,
    n64_scb.controller n64_scb,
    flash_scb.controller flash_scb,

    fifo_bus.controller fifo_bus,
    mem_bus.controller mem_bus,

    input sd_det,
    input button,

    output logic mcu_int,
    input mcu_clk,
    input mcu_cs,
    input mcu_mosi,
    output mcu_miso
);

    // Button input synchronization

    logic [2:0] sd_det_ff;
    logic [2:0] button_ff;

    always_ff @(posedge clk) begin
        sd_det_ff <= {sd_det_ff[1:0], sd_det};
        button_ff <= {button_ff[1:0], button};
    end


    // MCU <-> FPGA transport

    logic frame_start;
    logic data_ready;
    logic [7:0] rdata;
    logic [7:0] wdata;

    mcu_spi mcu_spi_inst (
        .clk(clk),
        .reset(reset),

        .frame_start(frame_start),
        .data_ready(data_ready),
        .rx_data(rdata),
        .tx_data(wdata),

        .mcu_clk(mcu_clk),
        .mcu_cs(mcu_cs),
        .mcu_mosi(mcu_mosi),
        .mcu_miso(mcu_miso)
    );


    // Protocol controller

    const bit [7:0] FPGA_ID = 8'h64;

    typedef enum bit [1:0] {
        PHASE_CMD,
        PHASE_ADDRESS,
        PHASE_DATA,
        PHASE_NOP
    } phase_e;

    typedef enum bit [7:0] {
        CMD_IDENTIFY,
        CMD_REG_READ,
        CMD_REG_WRITE,
        CMD_MEM_READ,
        CMD_MEM_WRITE,
        CMD_USB_STATUS,
        CMD_USB_READ,
        CMD_USB_WRITE,
        CMD_FLASHRAM_READ,
        CMD_EEPROM_READ,
        CMD_EEPROM_WRITE
    } cmd_e;

    phase_e phase;
    cmd_e cmd;

    logic [1:0] counter;
    logic [7:0] address;

    logic reg_read;
    logic reg_write;
    logic [31:0] reg_rdata;
    logic [31:0] reg_wdata;

    logic mem_read;
    logic mem_write;
    logic [15:0] mem_rdata;
    logic [15:0] mem_wdata;
    logic mem_word_select;

    always_ff @(posedge clk) begin
        fifo_bus.rx_read <= 1'b0;
        fifo_bus.tx_write <= 1'b0;

        n64_scb.eeprom_write <= 1'b0;

        reg_read <= 1'b0;
        reg_write <= 1'b0;

        mem_read <= 1'b0;
        mem_write <= 1'b0;

        if (reset) begin
        end else begin
            if (frame_start) begin
                counter <= 2'd0;
                phase <= PHASE_CMD;
            end

            if (reg_read || reg_write || (mem_word_select && (mem_read || mem_write))) begin
                address <= address + 1'd1;
            end

            if (n64_scb.eeprom_write) begin
                n64_scb.eeprom_address <= n64_scb.eeprom_address + 1'd1;
            end

            if (data_ready) begin
                case (phase)
                    PHASE_CMD: begin
                        cmd <= cmd_e'(rdata);
                        phase <= PHASE_ADDRESS;

                        if (rdata == CMD_USB_STATUS) begin
                            phase <= PHASE_NOP;
                        end

                        if (rdata == CMD_USB_READ) begin
                            fifo_bus.rx_read <= 1'b1;
                            phase <= PHASE_DATA;
                        end

                        if (rdata == CMD_USB_WRITE) begin
                            phase <= PHASE_DATA;
                        end
                    end

                    PHASE_ADDRESS: begin
                        address <= rdata;
                        phase <= PHASE_DATA;

                        if (cmd == CMD_REG_READ) begin
                            reg_read <= 1'b1;
                        end

                        if (cmd == CMD_MEM_READ) begin
                            mem_read <= 1'b1;
                            mem_word_select <= 1'b0;
                        end

                        if (cmd == CMD_FLASHRAM_READ) begin
                            n64_scb.flashram_buffer_address <= rdata[6:1];
                            counter <= {1'b0, rdata[0]};
                        end

                        if ((cmd == CMD_EEPROM_READ) || (cmd == CMD_EEPROM_WRITE)) begin
                            n64_scb.eeprom_address <= {rdata, 3'd0};
                        end
                    end

                    PHASE_DATA: begin
                        counter <= counter + 1'd1;

                        if (cmd == CMD_REG_READ) begin
                            if (counter == 2'd3) begin
                                reg_read <= 1'd1;
                            end
                        end

                        if (cmd == CMD_REG_WRITE) begin
                            case (counter)
                                2'd0: reg_wdata[7:0] <= rdata;
                                2'd1: reg_wdata[15:8] <= rdata;
                                2'd2: reg_wdata[23:16] <= rdata;
                                2'd3: reg_wdata[31:24] <= rdata;
                            endcase
                            if (counter == 2'd3) begin
                                reg_write <= 1'd1;
                            end
                        end

                        if (cmd == CMD_MEM_READ) begin
                            if (counter[0]) begin
                                mem_read <= 1'b1;
                                mem_word_select <= ~mem_word_select;
                            end
                        end

                        if (cmd == CMD_MEM_WRITE) begin
                            case (counter[0])
                                1'd0: mem_wdata[15:8] <= rdata;
                                1'd1: mem_wdata[7:0] <= rdata;
                            endcase
                            if (counter[0]) begin
                                mem_write <= 1'b1;
                                mem_word_select <= counter[1];
                            end
                        end

                        if (cmd == CMD_USB_READ) begin
                            phase <= PHASE_NOP;
                        end

                        if (cmd == CMD_USB_WRITE) begin
                            fifo_bus.tx_write <= 1'b1;
                            fifo_bus.tx_wdata <= rdata;
                            phase <= PHASE_NOP;
                        end

                        if (cmd == CMD_FLASHRAM_READ) begin
                            if (counter[0]) begin
                                n64_scb.flashram_buffer_address <= n64_scb.flashram_buffer_address + 1'd1;
                            end
                        end

                        if (cmd == CMD_EEPROM_READ) begin
                            n64_scb.eeprom_address <= n64_scb.eeprom_address + 1'd1;
                        end

                        if (cmd == CMD_EEPROM_WRITE) begin
                            n64_scb.eeprom_write <= 1'b1;
                            n64_scb.eeprom_wdata <= rdata;
                        end
                    end

                    PHASE_NOP: begin end
                endcase
            end
        end
    end

    always_comb begin
        wdata = 8'h00;

        case (cmd)
            CMD_IDENTIFY: begin
                wdata = FPGA_ID;
            end

            CMD_REG_READ: begin
                case (counter)
                    2'd0: wdata = reg_rdata[7:0];
                    2'd1: wdata = reg_rdata[15:8];
                    2'd2: wdata = reg_rdata[23:16];
                    2'd3: wdata = reg_rdata[31:24];
                endcase
            end

            CMD_REG_WRITE: begin
                wdata = 8'h00;
            end

            CMD_MEM_READ: begin
                case (counter[0])
                    1'd0: wdata = mem_rdata[15:8];
                    1'd1: wdata = mem_rdata[7:0];
                endcase
            end

            CMD_MEM_WRITE: begin
                wdata = 8'h00;
            end

            CMD_USB_STATUS: begin
                wdata = {6'd0, ~fifo_bus.tx_full, ~fifo_bus.rx_empty};
            end

            CMD_USB_READ: begin
                wdata = fifo_bus.rx_rdata;
            end

            CMD_USB_WRITE: begin
                wdata = 8'h00;
            end

            CMD_FLASHRAM_READ: begin
                case (counter[0])
                    1'd0: wdata = n64_scb.flashram_buffer_rdata[15:8];
                    1'd1: wdata = n64_scb.flashram_buffer_rdata[7:0];
                endcase
            end
            
            CMD_EEPROM_READ: begin
                wdata = n64_scb.eeprom_rdata;
            end

            CMD_EEPROM_WRITE: begin
                wdata = 8'h00;
            end
        endcase
    end


    // Mem bus controller

    logic [15:0] mem_buffer [0:511];
    
    logic mem_start;
    logic mem_stop;
    logic mem_direction;    
    logic [8:0] mem_length;
    logic [31:0] mem_address;

    logic mem_busy;
    logic mem_stop_pending;
    logic [8:0] mem_counter;

    always_ff @(posedge clk) begin
        if (reset) begin
            mem_busy <= 1'b0;
            mem_stop_pending <= 1'b0;
            mem_bus.request <= 1'b0;
        end else begin
            if (mem_read) begin
                mem_rdata <= mem_buffer[{address, mem_word_select}];
            end

            if (mem_write) begin
                mem_buffer[{address, mem_word_select}] <= mem_wdata;
            end

            if (mem_stop) begin
                mem_stop_pending <= mem_busy;
            end else if (mem_start && !mem_busy) begin
                mem_bus.write <= mem_direction;
                mem_bus.address <= mem_address;
                mem_busy <= 1'b1;
                mem_counter <= 9'd0;
            end

            if (mem_busy) begin
                if (!mem_bus.request) begin
                    mem_bus.request <= 1'b1;
                    mem_bus.wdata <= mem_buffer[mem_counter];
                end

                if (mem_bus.ack) begin
                    mem_bus.request <= 1'b0;
                    mem_bus.address <= mem_bus.address + 2'd2;
                    mem_counter <= mem_counter + 1'd1;
                    if (!mem_bus.write) begin
                        mem_buffer[mem_counter] <= mem_bus.rdata;
                    end
                    if ((mem_counter == mem_length) || mem_stop_pending) begin
                        mem_busy <= 1'b0;
                        mem_stop_pending <= 1'b0;
                    end
                end
            end
        end
    end

    always_comb begin
        mem_bus.wmask = 2'b11;
    end


    // Register list

    typedef enum bit [7:0] {
        REG_STATUS,
        REG_MEM_ADDRESS,
        REG_MEM_SCR,
        REG_USB_SCR,
        REG_USB_DMA_ADDRESS,
        REG_USB_DMA_LENGTH,
        REG_USB_DMA_SCR,
        REG_CFG_SCR,
        REG_CFG_DATA_0,
        REG_CFG_DATA_1,
        REG_CFG_CMD,
        REG_CFG_VERSION,
        REG_FLASHRAM_SCR,
        REG_FLASH_SCR,
        REG_RTC_SCR,
        REG_RTC_TIME_0,
        REG_RTC_TIME_1,
        REG_SD_SCR,
        REG_SD_ARG,
        REG_SD_CMD,
        REG_SD_RSP_0,
        REG_SD_RSP_1,
        REG_SD_RSP_2,
        REG_SD_RSP_3,
        REG_SD_DAT,
        REG_SD_DMA_ADDRESS,
        REG_SD_DMA_LENGTH,
        REG_SD_DMA_SCR
    } reg_address_e;

    logic bootloader_skip;

    assign n64_scb.cfg_version = 32'h53437632;


    // Register read logic

    always_ff @(posedge clk) begin
        if (reg_read) begin
            reg_rdata <= 32'd0;

            case (address)
                REG_STATUS: begin
                    reg_rdata <= {
                        24'd0,
                        sd_det_ff[2],
                        ~fifo_bus.tx_full,
                        ~fifo_bus.rx_empty,
                        n64_scb.flashram_pending,
                        n64_scb.cfg_pending,
                        usb_dma_scb.busy,
                        usb_scb.reset_pending,
                        button_ff[2]
                    };
                end

                REG_MEM_ADDRESS: begin
                    reg_rdata <= mem_address;
                end

                REG_MEM_SCR: begin
                    reg_rdata <= {
                        28'd0,
                        mem_busy,
                        3'b000
                    };
                end

                REG_USB_SCR: begin
                    reg_rdata <= {
                        28'd0,
                        usb_scb.reset_pending,
                        ~fifo_bus.tx_full,
                        ~fifo_bus.rx_empty,
                        1'b0
                    };
                end

                REG_USB_DMA_ADDRESS: begin
                    reg_rdata <= {
                        5'd0,
                        usb_dma_scb.starting_address
                    };
                end

                REG_USB_DMA_LENGTH: begin
                    reg_rdata <= {
                        5'd0,
                        usb_dma_scb.transfer_length
                    };
                end

                REG_USB_DMA_SCR: begin
                    reg_rdata <= {
                        28'd0,
                        usb_dma_scb.busy,
                        usb_dma_scb.direction,
                        2'b00
                    };
                end

                REG_CFG_SCR: begin
                    reg_rdata <= {
                        22'd0,
                        n64_scb.eeprom_16k_mode,
                        n64_scb.eeprom_enabled,
                        n64_scb.dd_enabled,
                        n64_scb.flashram_enabled,
                        n64_scb.sram_banked,
                        n64_scb.sram_enabled,
                        n64_scb.rom_shadow_enabled,
                        n64_scb.rom_write_enabled,
                        bootloader_skip,
                        n64_scb.bootloader_enabled
                    };
                end

                REG_CFG_DATA_0: begin
                    reg_rdata <= n64_scb.cfg_rdata[0];
                end

                REG_CFG_DATA_1: begin
                    reg_rdata <= n64_scb.cfg_rdata[1];
                end

                REG_CFG_CMD: begin
                    reg_rdata <= {
                        24'd0,
                        n64_scb.cfg_cmd
                    };
                end

                REG_CFG_VERSION: begin
                    reg_rdata <= n64_scb.cfg_version;
                end

                REG_FLASHRAM_SCR: begin
                    reg_rdata <= {
                        18'd0,
                        n64_scb.flashram_write_or_erase,
                        n64_scb.flashram_sector_or_all,
                        n64_scb.flashram_sector,
                        n64_scb.flashram_pending,
                        1'b0
                    };
                end

                REG_FLASH_SCR: begin
                    reg_rdata <= {
                        31'd0,
                        flash_scb.erase_pending
                    };
                end

                REG_RTC_SCR: begin
                    reg_rdata <= {
                        31'd0,
                        n64_scb.rtc_pending
                    };
                end

                REG_RTC_TIME_0: begin
                    reg_rdata <= {
                        5'd0, n64_scb.rtc_rdata[28:26],
                        2'd0, n64_scb.rtc_rdata[19:14],
                        1'd0, n64_scb.rtc_rdata[13:7],
                        1'd0, n64_scb.rtc_rdata[6:0]
                    };
                end

                REG_RTC_TIME_1: begin
                    reg_rdata <= {
                        8'd0,
                        n64_scb.rtc_rdata[41:34],
                        3'd0, n64_scb.rtc_rdata[33:29],
                        2'd0, n64_scb.rtc_rdata[25:20]
                    };
                end

                REG_SD_SCR: begin
                    reg_rdata <= {
                        30'd0,
                        sd_scb.clock_mode
                    };
                end

                REG_SD_DMA_ADDRESS: begin
                    reg_rdata <= {
                        5'd0,
                        sd_dma_scb.starting_address
                    };
                end

                REG_SD_DMA_LENGTH: begin
                    reg_rdata <= {
                        5'd0,
                        sd_dma_scb.transfer_length
                    };
                end

                REG_SD_DMA_SCR: begin
                    reg_rdata <= {
                        28'd0,
                        sd_dma_scb.busy,
                        sd_dma_scb.direction,
                        2'b00
                    };
                end
            endcase
        end
    end


    // Register write logic

    logic [31:0] reg_buffer;

    always_ff @(posedge clk) begin
        mem_start <= 1'b0;
        mem_stop <= 1'b0;

        usb_scb.write_buffer_flush <= 1'b0;
        usb_scb.reset_ack <= 1'b0;
        usb_scb.fifo_flush <= 1'b0;

        usb_dma_scb.start <= 1'b0;
        usb_dma_scb.stop <= 1'b0;

        sd_dma_scb.start <= 1'b0;
        sd_dma_scb.stop <= 1'b0;

        n64_scb.cfg_done <= 1'b0;
        n64_scb.cfg_irq <= 1'b0;

        n64_scb.flashram_done <= 1'b0;

        n64_scb.rtc_done <= 1'b0;

        if (n64_scb.n64_nmi) begin
            n64_scb.bootloader_enabled <= !bootloader_skip;
        end

        if (flash_scb.erase_done) begin
            flash_scb.erase_pending <= 1'b0;
        end

        if (reset) begin
            mcu_int <= 1'b0;
            sd_scb.clock_mode <= 2'd0;
            n64_scb.eeprom_16k_mode <= 1'b0;
            n64_scb.eeprom_enabled <= 1'b0;
            n64_scb.dd_enabled <= 1'b0;
            n64_scb.flashram_enabled <= 1'b0;
            n64_scb.sram_banked <= 1'b0;
            n64_scb.sram_enabled <= 1'b0;
            n64_scb.rom_shadow_enabled <= 1'b0;
            n64_scb.rom_write_enabled <= 1'b0;
            bootloader_skip <= 1'b0;
            n64_scb.bootloader_enabled <= 1'b1;
            flash_scb.erase_pending <= 1'b0;
        end else if (reg_write) begin
            case (address)
                REG_STATUS: begin end

                REG_MEM_ADDRESS: begin
                    mem_address <= reg_wdata;
                end

                REG_MEM_SCR: begin
                    {
                        mem_length,
                        mem_direction,
                        mem_stop,
                        mem_start
                    } <= {(reg_wdata[14:5] - 1'd1), reg_wdata[2:0]};
                end

                REG_USB_SCR: begin
                    {
                        usb_scb.write_buffer_flush,
                        usb_scb.reset_ack,
                        usb_scb.fifo_flush
                    } <= {reg_wdata[5:4], reg_wdata[0]};
                end

                REG_USB_DMA_ADDRESS: begin
                    usb_dma_scb.starting_address <= reg_wdata[26:0];
                end

                REG_USB_DMA_LENGTH: begin
                    usb_dma_scb.transfer_length <= reg_wdata[26:0];
                end

                REG_USB_DMA_SCR: begin
                    {
                        usb_dma_scb.direction,
                        usb_dma_scb.stop,
                        usb_dma_scb.start
                    } <= reg_wdata[2:0];
                end

                REG_CFG_SCR: begin
                    {
                        n64_scb.eeprom_16k_mode,
                        n64_scb.eeprom_enabled,
                        n64_scb.dd_enabled,
                        n64_scb.flashram_enabled,
                        n64_scb.sram_banked,
                        n64_scb.sram_enabled,
                        n64_scb.rom_shadow_enabled,
                        n64_scb.rom_write_enabled,
                        bootloader_skip,
                        n64_scb.bootloader_enabled
                    } <= reg_wdata[9:0];
                end

                REG_CFG_DATA_0: begin
                    n64_scb.cfg_wdata[0] <= reg_wdata;
                end

                REG_CFG_DATA_1: begin
                    n64_scb.cfg_wdata[1] <= reg_wdata;
                end

                REG_CFG_CMD: begin
                    {
                        n64_scb.cfg_irq,
                        n64_scb.cfg_error,
                        n64_scb.cfg_done
                    } <= reg_wdata[2:0];
                end

                REG_FLASHRAM_SCR: begin
                    n64_scb.flashram_done <= reg_wdata[0];
                end

                REG_FLASH_SCR: begin
                    flash_scb.erase_pending <= 1'b1;
                    flash_scb.erase_block <= reg_wdata[23:16];
                end

                REG_RTC_SCR: begin
                    n64_scb.rtc_done <= reg_wdata[1];
                end

                REG_RTC_TIME_0: begin
                    reg_buffer <= reg_wdata;
                end

                REG_RTC_TIME_1: begin
                    n64_scb.rtc_wdata[41:34] <= reg_wdata[23:16];
                    n64_scb.rtc_wdata[33:29] <= reg_wdata[12:8];
                    n64_scb.rtc_wdata[25:20] <= reg_wdata[5:0];
                    n64_scb.rtc_wdata[28:26] <= reg_buffer[26:24];
                    n64_scb.rtc_wdata[19:14] <= reg_buffer[21:16];
                    n64_scb.rtc_wdata[13:7] <= reg_buffer[14:8];
                    n64_scb.rtc_wdata[6:0] <= reg_buffer[6:0];
                end

                REG_SD_SCR: begin
                    sd_scb.clock_mode <= reg_wdata[1:0];
                end

                REG_SD_DMA_ADDRESS: begin
                    sd_dma_scb.starting_address <= reg_wdata[26:0];
                end

                REG_SD_DMA_LENGTH: begin
                    sd_dma_scb.transfer_length <= reg_wdata[26:0];
                end

                REG_SD_DMA_SCR: begin
                    {
                        sd_dma_scb.direction,
                        sd_dma_scb.stop,
                        sd_dma_scb.start
                    } <= reg_wdata[2:0];
                end
            endcase
        end
    end

endmodule