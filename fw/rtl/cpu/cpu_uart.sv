module cpu_uart (
    if_system.sys sys,
    if_cpu_bus bus,

    input uart_rxd,
    output uart_txd,
    input uart_cts,
    output uart_rts
);

    localparam BAUD_GEN_VALUE = int'(sc64::CLOCK_FREQUENCY / sc64::UART_BAUD_RATE) - 1'd1;

    typedef enum bit [1:0] {
        S_TRX_IDLE,
        S_TRX_DATA,
        S_TRX_SAMPLING_OFFSET
    } e_trx_state;


    // CPU bus controller

    e_trx_state tx_state;
    e_trx_state rx_state;
    logic [7:0] rx_data;
    logic rx_available;
    logic rx_overrun;

    always_ff @(posedge sys.clk) begin
        bus.ack <= 1'b0;
        if (bus.request) begin
            bus.ack <= 1'b1;
        end
    end

    always_comb begin
        bus.rdata = 32'd0;
        if (bus.ack) begin
            case (bus.address[2:2])
                0: bus.rdata = {29'd0, rx_overrun, tx_state == S_TRX_IDLE, rx_available};
                1: bus.rdata = {24'd0, rx_data};
                default: bus.rdata = 32'd0;
            endcase
        end
    end


    // TX path

    logic [6:0] tx_baud_counter;
    logic [3:0] tx_bit_counter;
    logic [9:0] tx_shifter;

    always_ff @(posedge sys.clk) begin
        tx_baud_counter <= tx_baud_counter + 1'd1;
        uart_txd <= tx_shifter[0];

        if (sys.reset) begin
            tx_state <= S_TRX_IDLE;
            tx_shifter <= 10'h3FF;
        end else begin
            case (tx_state)
                S_TRX_IDLE: begin
                    if (bus.request && bus.wmask[0] && bus.address[2]) begin
                        tx_state <= S_TRX_DATA;
                        tx_baud_counter <= 7'd0;
                        tx_bit_counter <= 4'd0;
                        tx_shifter <= {1'b1, bus.wdata[7:0], 1'b0};
                    end
                end

                S_TRX_DATA: begin
                    if (tx_baud_counter == BAUD_GEN_VALUE) begin
                        tx_baud_counter <= 7'd0;
                        tx_bit_counter <= tx_bit_counter + 1'd1;
                        tx_shifter <= {1'b1, tx_shifter[9:1]};
                        if (tx_bit_counter == 4'd9) begin
                            tx_state <= S_TRX_IDLE;
                        end
                    end
                end

                default: begin
                    tx_state <= S_TRX_IDLE;
                    tx_shifter <= 10'h3FF;
                end
            endcase
        end
    end


    // RX path

    logic [6:0] rx_baud_counter;
    logic [3:0] rx_bit_counter;
    logic [7:0] rx_shifter;
    logic [1:0] rxd_ff;

    always_ff @(posedge sys.clk) begin
        rx_baud_counter <= rx_baud_counter + 1'd1;
        rxd_ff <= {rxd_ff[0], uart_rxd};

        if (bus.request && bus.wmask[0] && !bus.address[2]) begin
            rx_overrun <= bus.wdata[2];
        end
        if (bus.request && !bus.wmask[0] && bus.address[2]) begin
            rx_available <= 1'b0;
        end

        if (sys.reset) begin
            rx_state <= S_TRX_IDLE;
            rx_available <= 1'b0;
            rx_overrun <= 1'b0;
        end else begin
            case (rx_state)
                S_TRX_IDLE: begin
                    if (!rxd_ff[1]) begin
                        rx_state <= S_TRX_SAMPLING_OFFSET;
                        rx_baud_counter <= 7'd0;
                        rx_bit_counter <= 4'd0;
                    end
                end

                S_TRX_SAMPLING_OFFSET: begin
                    if (rx_baud_counter == (BAUD_GEN_VALUE / 2)) begin
                        rx_state <= S_TRX_DATA;
                        rx_baud_counter <= 7'd0;
                    end
                end

                S_TRX_DATA: begin
                    if (rx_baud_counter == BAUD_GEN_VALUE) begin
                        rx_baud_counter <= 7'd0;
                        rx_bit_counter <= rx_bit_counter + 1'd1;
                        rx_shifter <= {rxd_ff[1], rx_shifter[7:1]};
                        if (rx_bit_counter == 4'd8) begin
                            rx_state <= S_TRX_IDLE;
                            if (rxd_ff[1]) begin
                                rx_data <= rx_shifter[7:0];
                                rx_available <= 1'b1;
                                rx_overrun <= rx_available;
                            end
                        end
                    end
                end

                default: begin
                    rx_state <= S_TRX_IDLE;
                    rx_available <= 1'b0;
                    rx_overrun <= 1'b0;
                end
            endcase
        end
    end

endmodule
