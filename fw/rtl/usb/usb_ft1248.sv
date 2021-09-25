module usb_ft1248 (
    if_system.sys sys,

    output usb_clk,
    output usb_cs,
    input usb_miso,
    inout [3:0] usb_miosi,
    input usb_pwren,

    input rx_flush,
    output rx_empty,
    input rx_read,
    output [7:0] rx_rdata,

    input tx_flush,
    output tx_full,
    input tx_write,
    input [7:0] tx_wdata
);

    // FIFOs

    logic rx_full;
    logic rx_write;
    logic [7:0] rx_wdata;

    logic tx_empty;
    logic tx_read;
    logic [7:0] tx_rdata;

    intel_fifo_8 fifo_8_rx_inst (
        .clock(sys.clk),
        .sclr(rx_flush),

        .empty(rx_empty),
        .rdreq(rx_read),
        .q(rx_rdata),

        .full(rx_full),
        .wrreq(rx_write),
        .data(rx_wdata)
    );

    intel_fifo_8 fifo_8_tx_inst (
        .clock(sys.clk),
        .sclr(tx_flush),

        .empty(tx_empty),
        .rdreq(tx_read),
        .q(tx_rdata),

        .full(tx_full),
        .wrreq(tx_write),
        .data(tx_wdata)
    );


    // FT1248 interface controller

    typedef enum bit [1:0] {
        S_TRY_RX,
        S_TRY_TX,
        S_COMMAND,
        S_DATA
    } e_state;

    typedef enum bit [7:0] {
        C_WRITE = 8'h00,
        C_READ = 8'h04
    } e_command;

    typedef enum bit [1:0] {
        P_PRE_RISING,
        P_RISING,
        P_PRE_FALLING,
        P_FALLING
    } e_clock_phase;

    e_state state;

    logic [3:0] clock_phase;

    logic usb_clk_output;
    logic usb_cs_output;
    logic [3:0] usb_miosi_input;
    logic [3:0] usb_miosi_output;
    logic [3:0] usb_miosi_output_data;
    logic usb_miosi_output_enable;
    logic usb_miosi_output_enable_data;
    logic usb_miso_input;
    logic usb_pwren_input;

    logic is_cmd_write;
    logic [1:0] nibble_counter;
    logic [7:0] tx_buffer;

    always_ff @(posedge sys.clk) begin
        if (sys.reset || state == S_TRY_RX || state == S_TRY_TX) begin
            clock_phase <= 4'b0001;
        end else begin
            clock_phase <= {clock_phase[2:0], clock_phase[3]};
        end
    end

    always_ff @(posedge sys.clk) begin
        usb_clk <= usb_clk_output;
        usb_cs <= usb_cs_output;

        usb_miosi_input <= usb_miosi;
        usb_miosi_output <= usb_miosi_output_data;
        usb_miosi_output_enable <= usb_miosi_output_enable_data;
        
        usb_miso_input <= usb_miso;
        usb_pwren_input <= usb_pwren;

        tx_buffer <= tx_rdata;
    end

    always_comb begin
        usb_miosi = usb_miosi_output_enable ? usb_miosi_output : 4'bZZZZ;
    end

    always_comb begin
        case (state)
            S_COMMAND: begin
                usb_clk_output = clock_phase[P_PRE_FALLING] || clock_phase[P_FALLING];
                usb_cs_output = 1'b0;
                if (is_cmd_write) begin
                    usb_miosi_output_data = nibble_counter[0] ? C_WRITE[3:0] : C_WRITE[7:4];
                end else begin
                    usb_miosi_output_data = nibble_counter[0] ? C_READ[3:0] : C_READ[7:4];
                end
                usb_miosi_output_enable_data = nibble_counter < 2'd2;
            end

            S_DATA: begin
                usb_clk_output = clock_phase[P_PRE_FALLING] || clock_phase[P_FALLING];
                usb_cs_output = 1'b0;
                usb_miosi_output_data = nibble_counter[0] ? tx_buffer[7:4] : tx_buffer[3:0];
                usb_miosi_output_enable_data = is_cmd_write;
            end

            default: begin
                usb_clk_output = 1'b0;
                usb_cs_output = 1'b1;
                usb_miosi_output_data = 4'hF;
                usb_miosi_output_enable_data = 1'b0;
            end
        endcase
    end

    always_ff @(posedge sys.clk) begin
        rx_write <= 1'b0;
        tx_read <= 1'b0;

        if (clock_phase[P_RISING]) begin
            nibble_counter <= nibble_counter + 1'd1;
        end

        if (sys.reset) begin
            state <= S_TRY_RX;
        end else begin
            case (state)
                S_TRY_RX: begin
                    if (!rx_full) begin
                        state <= S_COMMAND;
                        is_cmd_write <= 1'b0;
                        nibble_counter <= 2'b11;
                    end else begin
                        state <= S_TRY_TX;
                    end
                end

                S_TRY_TX: begin
                    if (!tx_empty) begin
                        state <= S_COMMAND;
                        is_cmd_write <= 1'b1;
                        nibble_counter <= 2'b11;
                    end else begin
                        state <= S_TRY_RX;
                    end
                end

                S_COMMAND: begin
                    if (clock_phase[P_RISING]) begin
                        if (nibble_counter == 2'd2) begin
                            if (usb_miso_input) begin
                                state <= is_cmd_write ? S_TRY_RX : S_TRY_TX;
                            end else begin
                                state <= S_DATA;
                                nibble_counter <= 2'd0;
                            end
                        end
                    end
                end

                S_DATA: begin
                    if (clock_phase[P_FALLING]) begin
                        if (nibble_counter[0]) begin
                            tx_read <= is_cmd_write;
                        end
                    end
                    if (clock_phase[P_RISING]) begin
                        rx_wdata <= {usb_miosi_input, rx_wdata[7:4]};
                        if (nibble_counter[0]) begin
                            rx_write <= !is_cmd_write;
                        end
                        if (usb_miso_input || (!is_cmd_write && rx_full) || (is_cmd_write && tx_empty)) begin
                            state <= is_cmd_write ? S_TRY_RX : S_TRY_TX;
                        end
                    end
                end

                default: begin
                    state <= S_TRY_RX;
                end
            endcase
        end
    end

endmodule
