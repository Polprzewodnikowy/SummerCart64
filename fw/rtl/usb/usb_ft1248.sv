module usb_ft1248 (
    if_system.sys system_if,

    output usb_clk,
    output usb_cs,
    input usb_miso,
    inout [3:0] usb_miosi,

    input request,
    input write,
    output busy,
    output reg ack,
    input [7:0] wdata,
    output reg [7:0] rdata,

    output reg rx_available,
    output reg tx_available
);

    typedef enum bit [2:0] {
        S_IDLE = 3'b000,
        S_COMMAND = 3'b100,
        S_DATA = 3'b101,
        S_END = 3'b110
    } e_state;

    typedef enum bit [7:0] {
        C_WRITE = 8'h00,
        C_READ = 8'h04
    } e_command;

    e_state state;

    assign busy = state[2];

    reg [3:0] clock_divider;
    wire rising_edge;
    wire falling_edge;

    assign rising_edge = clock_divider[1];
    assign falling_edge = clock_divider[3];

    always_ff @(posedge system_if.clk) begin
        if (system_if.reset || state == S_IDLE) begin
            clock_divider <= 4'b0001;
            usb_clk <= 1'b0;
        end else begin
            clock_divider <= {clock_divider[2:0], clock_divider[3]};
            if (state != S_END) begin
                if (rising_edge) usb_clk <= 1'b1;
                if (falling_edge) usb_clk <= 1'b0;
            end
        end
    end

    reg miosi_ff_1, miosi_ff_2;
    reg miso_ff_1, miso_ff_2;

    reg mode;

    reg [1:0] bit_counter;

    reg output_enable;

    reg [7:0] tx_data;
    reg [3:0] miosi;

    reg [7:0] command;


    assign usb_miosi = output_enable ? miosi : 4'bZZZZ;

    always_ff @(posedge system_if.clk) begin
        {miosi_ff_2, miosi_ff_1} <= {miosi_ff_1, usb_miosi[0]};
        {miso_ff_2, miso_ff_1} <= {miso_ff_1, usb_miso};
        rx_available <= 1'b0;
        tx_available <= 1'b0;
        ack <= 1'b0;

        if (system_if.reset) begin
            state <= S_IDLE;
            usb_cs <= 1'b1;
            output_enable <= 1'b0;
        end else begin
            case (state)
                S_IDLE: begin
                    rx_available <= ~miso_ff_2;
                    tx_available <= ~miosi_ff_2;
                    bit_counter <= 1'b0;
                    usb_cs <= 1'b1;
                    output_enable <= 1'b0;
                    if (request) begin
                        mode <= write;
                        if (rx_available || (write && tx_available)) begin
                            state <= S_COMMAND;
                            command <= write ? C_WRITE : C_READ;
                            usb_cs <= 1'b0;
                            rx_available <= 1'b0;
                            tx_available <= 1'b0;
                            tx_data <= {wdata[3:0], wdata[7:4]};
                        end
                    end
                end

                S_COMMAND: begin
                    if (rising_edge) begin
                        output_enable <= 1'b1;
                        bit_counter <= bit_counter + 1'd1;
                        {miosi, command} <= {command, 4'bXXXX};
                        if (bit_counter == 2'd2) begin
                            output_enable <= 1'b0;
                        end
                    end
                    if (falling_edge && (bit_counter == 2'd3)) begin
                        state <= S_DATA;
                        bit_counter <= 2'd0;
                    end
                end

                S_DATA: begin
                    if (rising_edge) begin
                        output_enable <= mode;
                        bit_counter[0] <= ~bit_counter[0];
                        {miosi, tx_data} <= {tx_data, 4'bXXXX};
                    end
                    if (falling_edge) begin
                        rdata <= {usb_miosi, rdata[7:4]};
                        if (!bit_counter[0]) begin
                            state <= S_END;
                            output_enable <= 1'b0;
                            ack <= 1'b1;
                        end
                    end
                end

                S_END: begin
                    if (rising_edge) begin
                        bit_counter <= bit_counter + 1'd1;
                        if (bit_counter == 2'd3) begin
                            state <= S_IDLE;
                        end
                        usb_cs <= 1'b1;
                    end
                end

                default: begin
                    state <= S_IDLE;
                end
            endcase
        end

    end

endmodule
