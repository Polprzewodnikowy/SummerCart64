interface if_dma ();

    logic request;
    logic ack;
    logic write;
    logic [31:0] address;
    logic [15:0] rdata;
    logic [15:0] wdata;

    modport cpu (
        output request,
        input ack,
        output write,
        output address,
        input rdata,
        output wdata
    );

    modport device (
        input request,
        output ack,
        input write,
        input address,
        output rdata,
        input wdata
    );

endinterface

module cpu_usb (
    if_system sys,
    if_cpu_bus bus,
    if_dma.cpu dma,

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

    // always_ff @(posedge sys.clk) begin
    //     bus.ack <= 1'b0;
    //     if (bus.request) begin
    //         bus.ack <= 1'b1;
    //     end        
    // end

    // always_comb begin
    //     bus.rdata = 32'd0;
    //     if (bus.ack) begin
    //         case (bus.address[2:2])
    //             0: bus.rdata = {30'd0, ~tx_full, ~rx_empty};
    //             1: bus.rdata = {24'd0, rx_rdata};
    //             default: bus.rdata = 32'd0;
    //         endcase
    //     end
    // end

    // always_ff @(posedge sys.clk) begin
    //     rx_flush <= 1'b0;
    //     rx_read <= 1'b0;

    //     tx_flush <= 1'b0;
    //     tx_write <= 1'b0;

    //     if (bus.request) begin
    //         case (bus.address[2:2])
    //             2'd0: if (bus.wmask[0]) begin
    //                 {tx_flush, rx_flush} <= bus.wdata[3:2];
    //             end

    //             2'd1: if (bus.wmask[0]) begin
    //                 if (!tx_full) begin
    //                     tx_write <= 1'b1;
    //                     tx_wdata <= bus.wdata[7:0];
    //                 end
    //             end else begin
    //                 rx_read <= 1'b1;
    //             end
    //         endcase
    //     end
    // end

    typedef enum bit [0:0] { 
        S_IDLE,
        S_WAIT
    } e_state;

    e_state state;

    logic byte_counter;

    always_ff @(posedge sys.clk) begin
        rx_read <= 1'b0;

        if (sys.reset) begin
            dma.request <= 1'b0;
            dma.write <= 1'b1;
            dma.address <= 32'd0;
            state <= S_IDLE;
            byte_counter <= 1'b0;
        end else begin
            case (state)
                S_IDLE: begin
                    if (!rx_empty && !rx_read) begin
                        byte_counter <= ~byte_counter;
                        rx_read <= 1'b1;
                        dma.wdata <= {dma.wdata[7:0], rx_rdata};
                        if (byte_counter) begin
                            dma.request <= 1'b1;
                            state <= S_WAIT;
                        end
                    end
                end

                S_WAIT: begin
                    if (dma.ack) begin
                        dma.address <= dma.address + 2'd2;
                        dma.request <= 1'b0;
                        state <= S_IDLE;
                    end
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

        // .tx_flush(tx_flush),
        // .tx_full(tx_full),
        // .tx_write(tx_write),
        // .tx_wdata(tx_wdata)
    );

endmodule
