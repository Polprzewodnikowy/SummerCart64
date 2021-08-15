module usb_ft1248 (
    if_system.sys system_if,

    output usb_clk,
    output usb_cs,
    input usb_miso,
    inout [3:0] usb_miosi,

    input rx_flush,
    output rx_empty,
    output rx_almost_empty,
    input rx_read,
    output [7:0] rx_rdata,

    input tx_flush,
    output tx_full,
    output tx_almost_full,
    input tx_write,
    input [7:0] tx_wdata
);

    // FIFOs

    wire rx_full;
    wire rx_almost_full;
    reg rx_write;
    reg [7:0] rx_wdata;

    wire tx_empty;
    reg tx_read;
    wire [7:0] tx_rdata;

    fifo8 fifo_8_rx_inst (
        .clock(system_if.clk),
        .sclr(rx_flush),

        .empty(rx_empty),
        .almost_empty(rx_almost_empty),
        .rdreq(rx_read),
        .q(rx_rdata),

        .full(rx_full),
        .almost_full(rx_almost_full),
        .wrreq(rx_write),
        .data(rx_wdata)
    );

    fifo8 fifo_8_tx_inst (
        .clock(system_if.clk),
        .sclr(tx_flush),

        .empty(tx_empty),
        .rdreq(tx_read),
        .q(tx_rdata),

        .full(tx_full),
        .almost_full(tx_almost_full),
        .wrreq(tx_write),
        .data(tx_wdata)
    );


    // FT1248 interface controller

    // Constants definition

    typedef enum bit [2:0] {
        S_TRY_RX = 3'b000,
        S_TRY_TX = 3'b001,
        S_COMMAND = 3'b100,
        S_DATA = 3'b101,
        S_END = 3'b111
    } e_state;

    typedef enum bit [7:0] {
        C_WRITE = 8'h00,
        C_READ = 8'h04
    } e_command;

    // FSM state

    e_state state;

    // Clock divider and generator

    reg [3:0] clock_divider;
    wire rising_edge = clock_divider[1];

    always_ff @(posedge system_if.clk) begin
        if (system_if.reset || state == S_TRY_RX || state == S_TRY_TX) begin
            clock_divider <= 4'b0001;
        end else begin
            clock_divider <= {clock_divider[2:0], clock_divider[3]};
        end
    end

    // Output chip select and data register behavior

    reg [1:0] bit_counter;
    reg [7:0] tx_buffer;

    wire clk_data;
    reg cs_data;
    wire [3:0] miosi_data;
    reg miosi_output_enable_data;

    reg clk_output;
    reg cs_output;
    reg [3:0] miosi_input;
    reg [3:0] miosi_output;
    reg miosi_output_enable;
    reg miso_input;

    always_comb begin
        clk_data = 1'b0;
        if (state == S_COMMAND || state == S_DATA) clk_data = (clock_divider[2] | clock_divider[3]);
        usb_clk = clk_output;
        usb_cs = cs_output;
        miosi_data = bit_counter[0] ? tx_buffer[3:0] : tx_buffer[7:4];
        usb_miosi = miosi_output_enable ? miosi_output : 4'bZZZZ;
    end

    always_ff @(posedge system_if.clk) begin
        clk_output <= clk_data;
        cs_output <= cs_data;
        miosi_input <= usb_miosi;
        miosi_output <= miosi_data;
        miosi_output_enable <= miosi_output_enable_data;
        miso_input <= usb_miso;
    end

    // FSM

    reg is_write;

    always_ff @(posedge system_if.clk) begin
        rx_write <= 1'b0;
        tx_read <= 1'b0;

        if (system_if.reset) begin
            state <= S_TRY_RX;
            cs_data <= 1'b1;
            miosi_output_enable_data <= 1'b0;
        end else begin
            case (state)
                S_TRY_RX: begin
                    if (!rx_full) begin
                        state <= S_COMMAND;
                        tx_buffer <= C_READ;
                        bit_counter <= 2'b11;
                        is_write <= 1'b0;
                    end else begin
                        state <= S_TRY_TX;
                    end
                end

                S_TRY_TX: begin
                    if (!tx_empty) begin
                        state <= S_COMMAND;
                        tx_buffer <= C_WRITE;
                        bit_counter <= 2'b11;
                        is_write <= 1'b1;
                    end else begin
                        state <= S_TRY_RX;
                    end
                end

                S_COMMAND: begin
                    cs_data <= 1'b0;
                    if (rising_edge) begin
                        bit_counter <= bit_counter + 1'd1;
                        miosi_output_enable_data <= 1'b1;
                        if (bit_counter == 2'd1) begin
                            miosi_output_enable_data <= 1'b0;
                        end
                        if (bit_counter == 2'd2) begin
                            if (!miso_input) begin
                                state <= S_DATA;
                                bit_counter <= 2'b11;
                                tx_buffer <= tx_rdata;
                                miosi_output_enable_data <= is_write;
                            end else begin
                                state <= S_END;
                                miosi_output_enable_data <= 1'b0;
                            end
                        end
                    end
                end

                S_DATA: begin
                    if (rising_edge) begin
                        bit_counter <= {1'b0, ~bit_counter[0]};
                        miosi_output_enable_data <= is_write;
                        rx_wdata <= {miosi_input, rx_wdata[7:4]};
                        if (bit_counter == 1'd1) begin
                            tx_buffer <= tx_rdata;
                        end
                        if (!is_write && (bit_counter[0] == 1'd0)) begin
                            rx_write <= 1'b1;
                        end
                        if (is_write && (bit_counter[0] == 1'd1)) begin
                            tx_read <= 1'b1;
                        end
                        if (
                            (bit_counter[0] == 1'd1 && miso_input) ||
                            (bit_counter[0] == 1'd0 && (
                                (is_write && tx_empty) ||
                                (!is_write && rx_almost_full)
                            ))) begin
                            state <= S_END;
                            miosi_output_enable_data <= 1'b0;
                        end
                    end
                end

                S_END: begin
                    cs_data <= 1'b1;
                    state <= is_write ? S_TRY_RX : S_TRY_TX;
                end

                default: begin
                    state <= S_TRY_RX;
                    cs_data <= 1'b1;
                    miosi_output_enable_data <= 1'b0;
                end
            endcase
        end
    end

endmodule
