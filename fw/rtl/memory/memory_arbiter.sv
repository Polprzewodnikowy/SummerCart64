module memory_arbiter (
    input clk,
    input reset,

    mem_bus.memory n64_bus,
    mem_bus.memory cfg_bus,
    mem_bus.memory usb_dma_bus,
    mem_bus.memory sd_dma_bus,

    mem_bus.controller sdram_mem_bus,
    mem_bus.controller flash_mem_bus,
    mem_bus.controller bram_mem_bus
);

    typedef enum bit [1:0] {
        SOURCE_N64,
        SOURCE_CFG,
        SOURCE_USB_DMA,
        SOURCE_SD_DMA
    } e_source_request;

    logic n64_sdram_request;
    logic cfg_sdram_request;
    logic usb_dma_sdram_request;
    logic sd_dma_sdram_request;

    logic n64_flash_request;
    logic cfg_flash_request;
    logic usb_dma_flash_request;
    logic sd_dma_flash_request;

    logic n64_bram_request;
    logic cfg_bram_request;
    logic usb_dma_bram_request;
    logic sd_dma_bram_request;

    assign n64_sdram_request = n64_bus.request && !n64_bus.address[26];
    assign cfg_sdram_request = cfg_bus.request && !cfg_bus.address[26];
    assign usb_dma_sdram_request = usb_dma_bus.request && !usb_dma_bus.address[26];
    assign sd_dma_sdram_request = sd_dma_bus.request && !sd_dma_bus.address[26];

    assign n64_flash_request = n64_bus.request && (n64_bus.address[26:24] == 3'b100);
    assign cfg_flash_request = cfg_bus.request && (cfg_bus.address[26:24] == 3'b100);
    assign usb_dma_flash_request = usb_dma_bus.request && (usb_dma_bus.address[26:24] == 3'b100);
    assign sd_dma_flash_request = sd_dma_bus.request && (sd_dma_bus.address[26:24] == 3'b100);

    assign n64_bram_request = n64_bus.request && (n64_bus.address[26:24] >= 3'b101);
    assign cfg_bram_request = cfg_bus.request && (cfg_bus.address[26:24] >= 3'b101);
    assign usb_dma_bram_request = usb_dma_bus.request && (usb_dma_bus.address[26:24] >= 3'b101);
    assign sd_dma_bram_request = sd_dma_bus.request && (sd_dma_bus.address[26:24] >= 3'b101);

    e_source_request sdram_source_request;

    always_ff @(posedge clk) begin
        if (reset) begin
            sdram_mem_bus.request <= 1'b0;
        end else begin
            if (!sdram_mem_bus.request) begin
                sdram_mem_bus.request <= (
                    n64_sdram_request ||
                    cfg_sdram_request ||
                    usb_dma_sdram_request ||
                    sd_dma_sdram_request
                );

                if (n64_sdram_request) begin
                    sdram_mem_bus.write <= n64_bus.write;
                    sdram_mem_bus.wmask <= n64_bus.wmask;
                    sdram_mem_bus.address <= n64_bus.address;
                    sdram_mem_bus.wdata <= n64_bus.wdata;
                    sdram_source_request <= SOURCE_N64;
                end else if (cfg_sdram_request) begin
                    sdram_mem_bus.write <= cfg_bus.write;
                    sdram_mem_bus.wmask <= cfg_bus.wmask;
                    sdram_mem_bus.address <= cfg_bus.address;
                    sdram_mem_bus.wdata <= cfg_bus.wdata;
                    sdram_source_request <= SOURCE_CFG;
                end else if (usb_dma_sdram_request) begin
                    sdram_mem_bus.write <= usb_dma_bus.write;
                    sdram_mem_bus.wmask <= usb_dma_bus.wmask;
                    sdram_mem_bus.address <= usb_dma_bus.address;
                    sdram_mem_bus.wdata <= usb_dma_bus.wdata;
                    sdram_source_request <= SOURCE_USB_DMA;
                end else if (sd_dma_sdram_request) begin
                    sdram_mem_bus.write <= sd_dma_bus.write;
                    sdram_mem_bus.wmask <= sd_dma_bus.wmask;
                    sdram_mem_bus.address <= sd_dma_bus.address;
                    sdram_mem_bus.wdata <= sd_dma_bus.wdata;
                    sdram_source_request <= SOURCE_SD_DMA;
                end
            end

            if (sdram_mem_bus.ack) begin
                sdram_mem_bus.request <= 1'b0;
            end
        end
    end

    e_source_request flash_source_request;

    always_ff @(posedge clk) begin
        if (reset) begin
            flash_mem_bus.request <= 1'b0;
        end else begin
            if (!flash_mem_bus.request) begin
                flash_mem_bus.request <= (
                    n64_flash_request ||
                    cfg_flash_request ||
                    usb_dma_flash_request ||
                    sd_dma_flash_request
                );

                if (n64_flash_request) begin
                    flash_mem_bus.write <= n64_bus.write;
                    flash_mem_bus.wmask <= n64_bus.wmask;
                    flash_mem_bus.address <= n64_bus.address;
                    flash_mem_bus.wdata <= n64_bus.wdata;
                    flash_source_request <= SOURCE_N64;
                end else if (cfg_flash_request) begin
                    flash_mem_bus.write <= cfg_bus.write;
                    flash_mem_bus.wmask <= cfg_bus.wmask;
                    flash_mem_bus.address <= cfg_bus.address;
                    flash_mem_bus.wdata <= cfg_bus.wdata;
                    flash_source_request <= SOURCE_CFG;
                end else if (usb_dma_flash_request) begin
                    flash_mem_bus.write <= usb_dma_bus.write;
                    flash_mem_bus.wmask <= usb_dma_bus.wmask;
                    flash_mem_bus.address <= usb_dma_bus.address;
                    flash_mem_bus.wdata <= usb_dma_bus.wdata;
                    flash_source_request <= SOURCE_USB_DMA;
                end else if (sd_dma_flash_request) begin
                    flash_mem_bus.write <= sd_dma_bus.write;
                    flash_mem_bus.wmask <= sd_dma_bus.wmask;
                    flash_mem_bus.address <= sd_dma_bus.address;
                    flash_mem_bus.wdata <= sd_dma_bus.wdata;
                    flash_source_request <= SOURCE_SD_DMA;
                end
            end

            if (flash_mem_bus.ack) begin
                flash_mem_bus.request <= 1'b0;
            end
        end
    end

    e_source_request bram_source_request;

    always_ff @(posedge clk) begin
        if (reset) begin
            bram_mem_bus.request <= 1'b0;
        end else begin
            if (!bram_mem_bus.request) begin
                bram_mem_bus.request <= (
                    n64_bram_request ||
                    cfg_bram_request ||
                    usb_dma_bram_request ||
                    sd_dma_bram_request
                );

                if (n64_bram_request) begin
                    bram_mem_bus.write <= n64_bus.write;
                    bram_mem_bus.wmask <= n64_bus.wmask;
                    bram_mem_bus.address <= n64_bus.address;
                    bram_mem_bus.wdata <= n64_bus.wdata;
                    bram_source_request <= SOURCE_N64;
                end else if (cfg_bram_request) begin
                    bram_mem_bus.write <= cfg_bus.write;
                    bram_mem_bus.wmask <= cfg_bus.wmask;
                    bram_mem_bus.address <= cfg_bus.address;
                    bram_mem_bus.wdata <= cfg_bus.wdata;
                    bram_source_request <= SOURCE_CFG;
                end else if (usb_dma_bram_request) begin
                    bram_mem_bus.write <= usb_dma_bus.write;
                    bram_mem_bus.wmask <= usb_dma_bus.wmask;
                    bram_mem_bus.address <= usb_dma_bus.address;
                    bram_mem_bus.wdata <= usb_dma_bus.wdata;
                    bram_source_request <= SOURCE_USB_DMA;
                end else if (sd_dma_bram_request) begin
                    bram_mem_bus.write <= sd_dma_bus.write;
                    bram_mem_bus.wmask <= sd_dma_bus.wmask;
                    bram_mem_bus.address <= sd_dma_bus.address;
                    bram_mem_bus.wdata <= sd_dma_bus.wdata;
                    bram_source_request <= SOURCE_SD_DMA;
                end
            end

            if (bram_mem_bus.ack) begin
                bram_mem_bus.request <= 1'b0;
            end
        end
    end

    always_comb begin
        n64_bus.ack = (
            ((sdram_source_request == SOURCE_N64) && sdram_mem_bus.ack) ||
            ((flash_source_request == SOURCE_N64) && flash_mem_bus.ack) ||
            ((bram_source_request == SOURCE_N64) && bram_mem_bus.ack)
        );
        cfg_bus.ack = (
            ((sdram_source_request == SOURCE_CFG) && sdram_mem_bus.ack) ||
            ((flash_source_request == SOURCE_CFG) && flash_mem_bus.ack) ||
            ((bram_source_request == SOURCE_CFG) && bram_mem_bus.ack)
        );
        usb_dma_bus.ack = (
            ((sdram_source_request == SOURCE_USB_DMA) && sdram_mem_bus.ack) ||
            ((flash_source_request == SOURCE_USB_DMA) && flash_mem_bus.ack) ||
            ((bram_source_request == SOURCE_USB_DMA) && bram_mem_bus.ack)
        );
        sd_dma_bus.ack = (
            ((sdram_source_request == SOURCE_SD_DMA) && sdram_mem_bus.ack) ||
            ((flash_source_request == SOURCE_SD_DMA) && flash_mem_bus.ack) ||
            ((bram_source_request == SOURCE_SD_DMA) && bram_mem_bus.ack)
        );

        n64_bus.rdata = n64_bram_request ? bram_mem_bus.rdata :
            n64_flash_request ? flash_mem_bus.rdata :
            sdram_mem_bus.rdata;
        cfg_bus.rdata = cfg_bram_request ? bram_mem_bus.rdata :
            cfg_flash_request ? flash_mem_bus.rdata :
            sdram_mem_bus.rdata;
        usb_dma_bus.rdata = usb_dma_bram_request ? bram_mem_bus.rdata :
            usb_dma_flash_request ? flash_mem_bus.rdata :
            sdram_mem_bus.rdata;
        sd_dma_bus.rdata = sd_dma_bram_request ? bram_mem_bus.rdata :
            sd_dma_flash_request ? flash_mem_bus.rdata :
            sdram_mem_bus.rdata;
    end

endmodule
