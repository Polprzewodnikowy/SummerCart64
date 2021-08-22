interface if_dma ();

    localparam [1:0] NUM_DEVICES = sc64::__ID_DMA_END;

    sc64::e_dma_id id;

    logic rx_empty;
    logic rx_read;
    logic [7:0] rx_rdata;
    logic tx_full;
    logic tx_write;
    logic [7:0] tx_wdata;

    logic request;
    logic ack;
    logic write;
    logic [31:0] address;
    logic [15:0] rdata;
    logic [15:0] wdata;

    modport controller (
        output id,

        input rx_empty,
        output rx_read,
        input rx_rdata,
        input tx_full,
        output tx_write,
        output tx_wdata,

        output request,
        input ack,
        output write,
        output address,
        input rdata,
        output wdata
    );

    modport memory (
        input request,
        output ack,
        input write,
        input address,
        output rdata,
        input wdata
    );

    logic [7:0] device_rx_rdata [(NUM_DEVICES - 1):0];
    logic device_rx_empty [(NUM_DEVICES - 1):0];
    logic device_tx_full [(NUM_DEVICES - 1):0];

    always_comb begin
        rx_rdata = 8'd0;

        for (integer i = 0; i < NUM_DEVICES; i++) begin
            rx_rdata = rx_rdata | device_rx_rdata[i];
            rx_empty = rx_empty | (device_rx_empty[i] && id == i[1:0]);
            tx_full = tx_full | (device_tx_full[i] && id == i[1:0]);
        end
    end

    genvar n;
    generate
        for (n = 0; n < NUM_DEVICES; n++) begin : at
            logic device_selected;
            logic device_rx_read;
            logic device_tx_write;

            always_comb begin
                device_selected = id == n[1:0];
                device_rx_read = device_selected && rx_read;
                device_tx_write = device_selected && tx_write;
            end

            modport device (
                output .rx_empty(device_rx_empty[n]),
                input .rx_read(device_rx_read),
                output .rx_rdata(device_rx_rdata[n]),
                output .tx_full(device_tx_full[n]),
                input .tx_write(device_tx_write),
                input .tx_wdata(tx_wdata)
            );
        end
    endgenerate

endinterface


module cpu_dma (
    if_system.sys sys,
    if_cpu_bus bus,
    if_dma.controller dma
);

    always_ff @(posedge sys.clk) begin
        bus.ack <= 1'b0;
        if (bus.request) begin
            bus.ack <= 1'b1;
        end        
    end

    always_comb begin
        bus.rdata = 32'd0;
        // if (bus.ack) begin
        //     case (bus.address[2:2])
        //         0: bus.rdata = {30'd0, ~tx_full, ~rx_empty};
        //         1: bus.rdata = {24'd0, rx_rdata};
        //         default: bus.rdata = 32'd0;
        //     endcase
        // end
    end

    always_ff @(posedge sys.clk) begin
        // rx_flush <= 1'b0;
        // cpu_rx_read <= 1'b0;

        // tx_flush <= 1'b0;
        // cpu_tx_write <= 1'b0;

        // if (bus.request) begin
        //     case (bus.address[2:2])
        //         2'd0: begin
        //             if (bus.wmask[0]) begin
        //                 {tx_flush, rx_flush} <= bus.wdata[3:2];
        //             end
        //         end

        //         2'd1: begin
        //             if (bus.wmask[0]) begin
        //                 cpu_tx_write <= 1'b1;
        //             end else begin
        //                 cpu_rx_read <= 1'b1;
        //             end
        //         end
        //     endcase
        // end
    end
    // typedef enum bit [0:0] { 
    //     S_IDLE,
    //     S_WAIT
    // } e_state;

    // e_state state;

    // logic byte_counter;

    // always_ff @(posedge sys.clk) begin
    //     rx_read <= 1'b0;

    //     if (sys.reset) begin
    //         dma.request <= 1'b0;
    //         dma.write <= 1'b1;
    //         dma.address <= 32'd0;
    //         state <= S_IDLE;
    //         byte_counter <= 1'b0;
    //     end else begin
    //         case (state)
    //             S_IDLE: begin
    //                 if (!rx_empty && !rx_read) begin
    //                     byte_counter <= ~byte_counter;
    //                     rx_read <= 1'b1;
    //                     dma.wdata <= {dma.wdata[7:0], rx_rdata};
    //                     if (byte_counter) begin
    //                         dma.request <= 1'b1;
    //                         state <= S_WAIT;
    //                     end
    //                 end
    //             end

    //             S_WAIT: begin
    //                 if (dma.ack) begin
    //                     dma.address <= dma.address + 2'd2;
    //                     dma.request <= 1'b0;
    //                     state <= S_IDLE;
    //                 end
    //             end
    //         endcase
    //     end
    // end
endmodule
