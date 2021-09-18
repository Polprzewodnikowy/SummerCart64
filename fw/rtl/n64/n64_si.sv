interface if_si ();

    logic rx_reset;
    logic rx_ready;
    logic [6:0] rx_length;
    logic [80:0] rx_data;

    logic tx_reset;
    logic tx_start;
    logic tx_busy;
    logic [2:0] tx_wmask;
    logic [6:0] tx_length;
    logic [31:0] tx_data;

    modport si (
        input rx_reset,
        output rx_ready,
        output rx_length,
        output rx_data,
        input tx_reset,
        input tx_start,
        output tx_busy,
        input tx_wmask,
        input tx_length,
        input tx_data
    );

    modport cpu (
        output rx_reset,
        input rx_ready,
        input rx_length,
        input rx_data,
        output tx_reset,
        output tx_start,
        input tx_busy,
        output tx_wmask,
        output tx_length,
        output tx_data
    );

endinterface

module n64_si (
    if_system.sys sys,
    if_si.si si,

    input n64_si_clk,
    inout n64_si_dq
);

    // Control signals and input synchronization

    logic [1:0] n64_si_clk_ff;

    always_ff @(posedge sys.clk) begin
        n64_si_clk_ff <= {n64_si_clk_ff[0], n64_si_clk};
    end

    logic si_reset;
    logic si_clk;
    logic si_dq;

    always_comb begin
        si_reset = sys.n64_hard_reset;
        si_clk = n64_si_clk_ff[1];
        si_dq = n64_si_dq;
    end

    logic last_si_clk;

    always_ff @(posedge sys.clk) begin
        last_si_clk <= si_clk;
    end

    logic si_clk_rising_edge;
    logic si_clk_falling_edge;

    always_comb begin
        si_clk_rising_edge = !si_reset && !last_si_clk && si_clk;
        si_clk_falling_edge = !si_reset && last_si_clk && !si_clk;
    end

    logic si_dq_output_enable;
    logic si_dq_output_enable_data;

    always_ff @(posedge sys.clk) begin
        si_dq_output_enable <= si_dq_output_enable_data;
    end

    always_comb begin
        n64_si_dq = si_dq_output_enable ? 1'b0 : 1'bZ;
    end


    // Data register and shifter

    logic [80:0] trx_data;
    logic rx_shift;
    logic tx_shift;

    always_comb begin
        si.rx_data = trx_data;
    end

    always_ff @(posedge sys.clk) begin
        if (si.tx_wmask[0]) trx_data[80:49] <= si.tx_data;
        if (si.tx_wmask[1]) trx_data[48:17] <= si.tx_data;
        if (si.tx_wmask[2]) trx_data[16:0] <= si.tx_data[16:0];

        if (rx_shift || tx_shift) begin
            trx_data <= {trx_data[79:0], rx_sub_bit_counter < 2'd2};
        end
    end


    // RX path

    typedef enum bit [0:0] { 
        S_RX_IDLE,
        S_RX_WAITING
    } e_rx_state;

    e_rx_state rx_state;

    logic [1:0] rx_sub_bit_counter;
    logic [3:0] rx_timeout_counter;

    always_ff @(posedge sys.clk) begin
        rx_shift <= 1'b0;

        if (si_clk_rising_edge) begin
            if (rx_timeout_counter < 4'd8) begin
                rx_timeout_counter <= rx_timeout_counter + 1'd1;
            end else if (si.rx_length > 7'd0) begin
                si.rx_ready <= 1'b1;
            end
        end

        if (sys.reset || si.rx_reset) begin
            rx_state <= S_RX_IDLE;
            si.rx_ready <= 1'b0;
            si.rx_length <= 7'd0;
        end else if (!si.tx_busy) begin
            case (rx_state)
                S_RX_IDLE: begin
                    if (si_clk_rising_edge && !si_dq) begin
                        rx_state <= S_RX_WAITING;
                        rx_sub_bit_counter <= 2'd0;
                        rx_timeout_counter <= 3'd0;
                    end
                end

                S_RX_WAITING: begin
                    if (si_clk_rising_edge) begin
                        if (si_dq) begin
                            rx_state <= S_RX_IDLE;
                            rx_shift <= 1'b1;
                            si.rx_length <= si.rx_length + 1'd1;
                        end else if (rx_sub_bit_counter < 2'd3) begin
                            rx_sub_bit_counter <= rx_sub_bit_counter + 1'd1;
                        end
                    end
                end
            endcase
        end
    end


    // TX path

    typedef enum bit [0:0] { 
        S_TX_IDLE,
        S_TX_SENDING
    } e_tx_state;

    e_tx_state tx_state;

    logic [2:0] tx_sub_bit_counter;
    logic [6:0] tx_bit_counter;

    always_ff @(posedge sys.clk) begin
        tx_shift <= 1'b0;

        if (sys.reset || si.tx_reset) begin
            tx_state <= S_TX_IDLE;
            si_dq_output_enable_data <= 1'b0;
            si.tx_busy <= 1'b0;
        end else begin
            case (tx_state)
                S_TX_IDLE: begin
                    if (si.tx_start) begin
                        tx_state <= S_TX_SENDING;
                        tx_sub_bit_counter <= 3'd0;
                        tx_bit_counter <= si.tx_length;
                        si.tx_busy <= 1'b1;
                    end
                end

                S_TX_SENDING: begin
                    if (si_clk_falling_edge) begin
                        tx_sub_bit_counter <= tx_sub_bit_counter + 1'd1;
                        if (tx_sub_bit_counter == 3'd7) begin
                            tx_shift <= 1'b1;
                            if (tx_bit_counter >= 7'd1) begin
                                tx_bit_counter <= tx_bit_counter - 1'd1;
                            end else begin
                                tx_state <= S_TX_IDLE;
                                si.tx_busy <= 1'b0;
                            end
                        end
                        if (tx_bit_counter == 7'd0) begin
                            si_dq_output_enable_data <= tx_sub_bit_counter < 3'd4;
                        end else if (trx_data[80]) begin
                            si_dq_output_enable_data <= tx_sub_bit_counter < 3'd2;
                        end else begin
                            si_dq_output_enable_data <= tx_sub_bit_counter < 3'd6;
                        end
                    end
                end
            endcase
        end
    end

endmodule
