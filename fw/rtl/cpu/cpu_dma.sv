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
        rx_empty = 1'b0;
        tx_full = 1'b0;

        for (integer i = 0; i < NUM_DEVICES; i++) begin
            rx_rdata = rx_rdata | device_rx_rdata[i];//(device_rx_rdata[i] & {8{id == i[1:0]});
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

    typedef enum bit [2:0] { 
        S_IDLE,
        S_FETCH,
        S_TRANSFER
    } e_state;

    e_state state;
    logic direction;
    logic [27:0] length;
    logic [15:0] rdata_buffer;

    always_comb begin
        bus.rdata = 32'd0;
        if (bus.ack) begin
            case (bus.address[3:2])
                0: bus.rdata = {28'd0, state != S_IDLE, direction, 2'b00};
                1: bus.rdata = dma.address;
                2: bus.rdata = {2'b00, dma.id, length};
            endcase
        end
    end

    logic byte_counter;

    always_ff @(posedge sys.clk) begin
        bus.ack <= 1'b0;
        if (bus.request) begin
            bus.ack <= 1'b1;
        end

        dma.rx_read <= 1'b0;
        dma.tx_write <= 1'b0;

        if (sys.reset) begin
            state <= S_IDLE;
            dma.request <= 1'b0;
        end else begin
            case (state)
                S_IDLE: begin
                    if (bus.request) begin
                        case (bus.address[3:2])
                            0: if (bus.wmask[0]) begin
                                direction <= bus.wdata[2];
                                if (bus.wdata[0]) begin
                                    state <= S_FETCH;
                                    byte_counter <= 1'b0;
                                end
                            end

                            1: if (&bus.wmask) begin
                                dma.address <= bus.wdata;
                            end

                            2: if (&bus.wmask) begin
                                {dma.id, length} <= {bus.wdata[29:1], 1'b0};
                            end
                        endcase
                    end
                end

                S_FETCH: begin
                    if (length != 28'd0) begin
                        if (direction) begin 
                            if (!dma.rx_empty && !dma.rx_read) begin
                                dma.rx_read <= 1'b1;
                                dma.wdata <= {dma.wdata[7:0], dma.rx_rdata};
                                byte_counter <= ~byte_counter;
                                if (byte_counter) begin
                                    state <= S_TRANSFER;
                                    dma.request <= 1'b1;
                                    dma.write <= 1'b1;
                                end
                            end
                        end else begin
                            dma.request <= 1'b1;
                            dma.write <= 1'b0;
                            if (dma.ack) begin
                                state <= S_TRANSFER;
                                dma.request <= 1'b0;
                                rdata_buffer <= dma.rdata;
                            end
                        end
                    end else begin
                        state <= S_IDLE;
                    end
                end

                S_TRANSFER: begin
                    if (direction) begin
                        if (dma.ack) begin
                            state <= S_FETCH;
                            dma.request <= 1'b0;
                            dma.address <= dma.address + 2'd2;
                            length <= length - 2'd2;
                        end
                    end else begin
                        if (!dma.tx_full && !dma.tx_write) begin
                            dma.tx_write <= 1'b1;
                            dma.tx_wdata <= byte_counter ? rdata_buffer[7:0] : rdata_buffer[15:8];
                            byte_counter <= ~byte_counter;
                            if (byte_counter) begin
                                state <= S_FETCH;
                                dma.address <= dma.address + 2'd2;
                                length <= length - 2'd2;
                            end
                        end
                    end
                end

                default: begin
                    state <= S_IDLE;
                    dma.request <= 1'b0;
                end
            endcase
        end
    end

endmodule
