module cpu_uart # (
    parameter BAUD_RATE = 1_000_000
) (
    if_cpu_bus bus,

    input uart_rxd,
    output uart_txd,
    input uart_cts,
    output uart_rts
);

    localparam BAUD_GEN_VALUE = int'(100_000_000 / BAUD_RATE) - 1'd1;

    typedef enum bit [1:0] {
        S_TX_IDLE,
        S_TX_DATA
    } e_tx_state;

    e_tx_state tx_state;
    logic [7:0] tx_data;
    logic tx_start;

    always_comb begin
        bus.rdata = 32'd0;
        if (bus.ack) begin
            case (bus.address[2:2])
                0: bus.rdata = {30'd0, tx_state == S_TX_IDLE, 1'b0};
                default: bus.rdata = 32'd0;
            endcase
        end
    end

    always_ff @(posedge bus.clk) begin
        bus.ack <= 1'b0;
        tx_start <= 1'b0;

        if (bus.request) begin
            bus.ack <= 1'b1;

            case (bus.address[2:2])
                2'd1: if (bus.wmask[0]) begin
                    tx_data <= bus.wdata[7:0];
                    tx_start <= 1'b1;
                end
            endcase
        end
    end

    logic [6:0] tx_baud_counter;
    logic [3:0] tx_bit_counter;
    logic [9:0] tx_shifter;

    always_ff @(posedge bus.clk) begin
        tx_baud_counter <= tx_baud_counter + 1'd1;
        uart_txd <= tx_shifter[0];

        if (bus.reset) begin
            tx_state <= S_TX_IDLE;
            tx_shifter <= 10'h3FF;
        end else begin
            case (tx_state)
                S_TX_IDLE: begin
                    if (tx_start) begin
                        tx_state <= S_TX_DATA;
                        tx_baud_counter <= 7'd0;
                        tx_bit_counter <= 4'd0;
                        tx_shifter <= {1'b1, tx_data, 1'b0};
                    end
                end

                S_TX_DATA: begin
                    if (tx_baud_counter == BAUD_GEN_VALUE) begin
                        tx_baud_counter <= 7'd0;
                        tx_bit_counter <= tx_bit_counter + 1'd1;
                        tx_shifter <= {1'b1, tx_shifter[9:1]};
                        if (tx_bit_counter == 4'd9) begin
                            tx_state <= S_TX_IDLE;
                        end
                    end
                end

                default: begin
                    tx_state <= S_TX_IDLE;
                end
            endcase
        end
    end

endmodule
