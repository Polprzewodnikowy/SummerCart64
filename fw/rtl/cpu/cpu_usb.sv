module cpu_usb (
    if_system sys,
    if_cpu_bus bus,

    output usb_clk,
    output usb_cs,
    input usb_miso,
    inout [3:0] usb_miosi,
    input usb_pwren
);

    logic rx_flush;
    logic rx_empty;
    logic rx_read;
    logic [7:0] rx_rdata;

    logic tx_flush;
    logic tx_full;
    logic tx_write;
    logic [7:0] tx_wdata;

    always_comb begin
        bus.rdata = 32'd0;
        if (bus.ack) begin
            case (bus.address[2:2])
                0: bus.rdata = {30'd0, ~tx_full, ~rx_empty};
                1: bus.rdata = {24'd0, rx_rdata};
                default: bus.rdata = 32'd0;
            endcase
        end
    end

    always_ff @(posedge bus.clk) begin
        rx_flush <= 1'b0;
        rx_read <= 1'b0;

        tx_flush <= 1'b0;
        tx_write <= 1'b0;

        bus.ack <= 1'b0;
        if (bus.request) begin
            bus.ack <= 1'b1;
        end

        if (bus.request) begin
            case (bus.address[2:2])
                2'd0: if (bus.wmask[0]) begin
                    {tx_flush, rx_flush} <= bus.wdata[3:2];
                end

                2'd1: if (bus.wmask[0]) begin
                    if (!tx_full) begin
                        tx_write <= 1'b1;
                        tx_wdata <= bus.wdata[7:0];
                    end
                end else begin
                    rx_read <= 1'b1;
                end
            endcase
        end
    end

    usb_ft1248 usb_ft1248_inst (
        .sys(sys),

        .usb_clk(usb_clk),
        .usb_cs(usb_cs),
        .usb_miso(usb_miso),
        .usb_miosi(usb_miosi),
        .usb_pwren(usb_pwren),

        .rx_flush(rx_flush),
        .rx_empty(rx_empty),
        .rx_read(rx_read),
        .rx_rdata(rx_rdata),

        .tx_flush(tx_flush),
        .tx_full(tx_full),
        .tx_write(tx_write),
        .tx_wdata(tx_wdata)
    );

endmodule
