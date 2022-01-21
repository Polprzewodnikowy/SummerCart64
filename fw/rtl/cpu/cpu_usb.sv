module cpu_usb (
    if_system sys,
    if_cpu_bus bus,
    if_dma dma,

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

    logic rx_escape_valid;
    logic rx_escape_ack;
    logic [7:0] rx_escape;

    logic cpu_rx_read;
    logic cpu_tx_write;

    logic usb_enabled;
    logic tx_force;

    always_comb begin
        dma.rx_empty = rx_empty;
        rx_read = cpu_rx_read || dma.rx_read;
        dma.rx_rdata = rx_rdata;

        dma.tx_full = tx_full;
        tx_write = cpu_tx_write || dma.tx_write;
        tx_wdata = dma.tx_write ? dma.tx_wdata : bus.wdata[7:0];
    end

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
                0: bus.rdata = {25'd0, rx_escape_valid, usb_pwren, usb_enabled, 2'b00, ~tx_full, ~rx_empty};
                1: bus.rdata = {24'd0, rx_rdata};
                2: bus.rdata = {24'd0, rx_escape};
                default: bus.rdata = 32'd0;
            endcase
        end
    end

    always_ff @(posedge sys.clk) begin
        rx_flush <= 1'b0;
        cpu_rx_read <= 1'b0;

        tx_flush <= 1'b0;
        cpu_tx_write <= 1'b0;

        rx_escape_ack <= 1'b0;
        tx_force <= 1'b0;

        if (sys.reset) begin
            usb_enabled <= 1'b0;
        end else begin
            if (bus.request) begin
                case (bus.address[2:2])
                    2'd0: begin
                        if (bus.wmask[1]) begin
                            tx_force <= bus.wdata[8];
                        end
                        if (bus.wmask[0]) begin
                            rx_escape_ack <= bus.wdata[7];
                            {usb_enabled, tx_flush, rx_flush} <= bus.wdata[4:2];
                        end
                    end

                    2'd1: begin
                        if (bus.wmask[0]) begin
                            cpu_tx_write <= 1'b1;
                        end else begin
                            cpu_rx_read <= 1'b1;
                        end
                    end
                endcase
            end
        end
    end

    usb_ft1248 usb_ft1248_inst (
        .sys(sys),

        .usb_enabled(usb_enabled),
        .tx_force(tx_force),

        .usb_clk(usb_clk),
        .usb_cs(usb_cs),
        .usb_miso(usb_miso),
        .usb_miosi(usb_miosi),

        .rx_flush(rx_flush),
        .rx_empty(rx_empty),
        .rx_read(rx_read),
        .rx_rdata(rx_rdata),

        .tx_flush(tx_flush),
        .tx_full(tx_full),
        .tx_write(tx_write),
        .tx_wdata(tx_wdata),

        .rx_escape_valid(rx_escape_valid),
        .rx_escape_ack(rx_escape_ack),
        .rx_escape(rx_escape)
    );

endmodule
