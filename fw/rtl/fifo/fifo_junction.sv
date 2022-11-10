module fifo_junction (        
    fifo_bus.controller dev_bus,

    fifo_bus.fifo cfg_bus,
    fifo_bus.fifo dma_bus
);

    always_comb begin
        dev_bus.rx_read = cfg_bus.rx_read || dma_bus.rx_read;
        dev_bus.tx_write = cfg_bus.tx_write || dma_bus.tx_write;
        dev_bus.tx_wdata = cfg_bus.tx_write ? cfg_bus.tx_wdata : dma_bus.tx_wdata;

        cfg_bus.rx_empty = dev_bus.rx_empty;
        cfg_bus.rx_almost_empty = dev_bus.rx_almost_empty;
        cfg_bus.rx_rdata = dev_bus.rx_rdata;
        cfg_bus.tx_full = dev_bus.tx_full;
        cfg_bus.tx_almost_full = dev_bus.tx_almost_full;

        dma_bus.rx_empty = dev_bus.rx_empty;
        dma_bus.rx_almost_empty = dev_bus.rx_almost_empty;
        dma_bus.rx_rdata = dev_bus.rx_rdata;
        dma_bus.tx_full = dev_bus.tx_full;
        dma_bus.tx_almost_full = dev_bus.tx_almost_full;
    end

endmodule
