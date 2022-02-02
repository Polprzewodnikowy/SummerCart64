module cpu_usb (
    if_system sys,
    if_cpu_bus bus,
    if_memory_dma dma,

    output usb_clk,
    output usb_cs,
    input usb_miso,
    inout [7:0] usb_miosi
);

    logic rx_flush;
    logic tx_flush;
    logic usb_enable;
    logic reset_pending;
    logic reset_ack;
    logic write_buffer_flush;

    typedef enum bit [1:0] {
        R_SCR,
        R_DATA,
        R_ADDR,
        R_LEN
    } e_reg_id;

    always_ff @(posedge sys.clk) begin
        bus.ack <= 1'b0;
        if (bus.request) begin
            bus.ack <= 1'b1;
        end        
    end

    always_comb begin
        bus.rdata = 32'd0;
        if (bus.ack) begin
            case (bus.address[3:2])
                R_SCR: bus.rdata = {
                    23'd0,
                    dma.busy,
                    1'b0,
                    reset_pending,
                    1'b0,
                    usb_enable,
                    2'b00,
                    ~dma.tx_full,
                    ~dma.rx_empty
                };
                R_DATA: bus.rdata = {24'd0, dma.rx_rdata};
                default: bus.rdata = 32'd0;
            endcase
        end
    end

    always_ff @(posedge sys.clk) begin
        dma.start <= 1'b0;
        dma.stop <= 1'b0;
        dma.cpu_rx_read <= 1'b0;
        dma.cpu_tx_write <= 1'b0;
        rx_flush <= 1'b0;
        tx_flush <= 1'b0;
        reset_ack <= 1'b0;
        write_buffer_flush <= 1'b0;

        if (sys.reset) begin
            usb_enable <= 1'b0;
        end else begin
            if (bus.request) begin
                case (bus.address[3:2])
                    R_SCR: if (&bus.wmask) begin
                        {dma.direction, dma.stop, dma.start} <= bus.wdata[11:9];
                        reset_ack <= bus.wdata[7];
                        {write_buffer_flush, usb_enable, tx_flush, rx_flush} <= bus.wdata[5:2];
                    end

                    R_DATA: if (bus.wmask == 4'b0000) begin
                        dma.cpu_rx_read <= 1'b1;
                    end else if (bus.wmask == 4'b0001) begin
                        dma.cpu_tx_write <= 1'b1;
                        dma.cpu_tx_wdata <= bus.wdata[7:0];
                    end

                    R_ADDR: if (&bus.wmask) begin
                        dma.starting_address <= bus.wdata;
                    end

                    R_LEN: if (&bus.wmask) begin
                        dma.transfer_length <= bus.wdata;
                    end
                endcase
            end
        end
    end

    memory_dma usb_memory_dma_inst (
        .clk(sys.clk),
        .reset(~usb_enable),
        .dma(dma)
    );

    usb_ft1248 usb_ft1248_inst (
        .clk(sys.clk),
        .reset(~usb_enable),

        .usb_clk(usb_clk),
        .usb_cs(usb_cs),
        .usb_miso(usb_miso),
        .usb_miosi(usb_miosi),

        .reset_pending(reset_pending),
        .reset_ack(reset_ack),
        .write_buffer_flush(write_buffer_flush),

        .rx_flush(rx_flush),
        .rx_empty(dma.rx_empty),
        .rx_almost_empty(dma.rx_almost_empty),
        .rx_read(dma.rx_read),
        .rx_rdata(dma.rx_rdata),

        .tx_flush(tx_flush),
        .tx_full(dma.tx_full),
        .tx_almost_full(dma.tx_almost_full),
        .tx_write(dma.tx_write),
        .tx_wdata(dma.tx_wdata)
    );

endmodule
