module usb_ft1248 (
    input clk,
    input reset,

    output usb_clk,
    output usb_cs,
    input usb_miso,
    inout [7:0] usb_miosi,

    output reset_pending,
    input reset_ack,
    input write_buffer_flush,

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

    logic rx_full;
    logic rx_almost_full;
    logic rx_write;

    logic tx_empty;
    logic tx_almost_empty;
    logic tx_read;
    logic [7:0] tx_rdata;

    intel_fifo_8 fifo_8_rx_inst (
        .clock(clk),
        .sclr(reset || rx_flush),

        .empty(rx_empty),
        .almost_empty(rx_almost_empty),
        .rdreq(rx_read),
        .q(rx_rdata),

        .full(rx_full),
        .almost_full(rx_almost_full),
        .wrreq(rx_write),
        .data(usb_miosi)
    );

    intel_fifo_8 fifo_8_tx_inst (
        .clock(clk),
        .sclr(reset || tx_flush),

        .empty(tx_empty),
        .almost_empty(tx_almost_empty),
        .rdreq(tx_read),
        .q(tx_rdata),

        .full(tx_full),
        .almost_full(tx_almost_full),
        .wrreq(tx_write),
        .data(tx_wdata)
    );

    typedef enum bit [2:0] {
        STATE_IDLE,
        STATE_SELECT,
        STATE_COMMAND,
        STATE_STATUS,
        STATE_DATA,
        STATE_DESELECT
    } e_state;

    typedef enum bit [7:0] {
        CMD_WRITE = 8'h00,
        CMD_READ = 8'h40,
        CMD_READ_MODEM_STATUS = 8'h20,
        CMD_WRITE_MODEM_STATUS = 8'h60,
        CMD_WRITE_BUFFER_FLUSH = 8'h08
    } e_command;

    logic usb_miosi_oe;
    logic write_buffer_flush_pending;
    logic write_modem_status_pending;
    logic [4:0] modem_status_counter;
    logic modem_status_read;
    logic last_reset_status;
    logic reset_reply;
    logic [3:0] phase;
    logic [7:0] usb_cmd;
    e_state state;

    always_comb begin
        usb_miosi = 8'hZZ;
        if (usb_miosi_oe) begin
            usb_miosi = 8'h00;
            if ((state == STATE_COMMAND) || (state == STATE_STATUS)) begin
                usb_miosi = usb_cmd;
            end
            if ((state == STATE_DATA) || (state == STATE_DESELECT)) begin
                if (usb_cmd == CMD_WRITE) begin
                    usb_miosi = tx_rdata;
                end
                if (usb_cmd == CMD_WRITE_MODEM_STATUS) begin
                    usb_miosi = {2'b00, reset_reply, 5'b00000};
                end
            end
        end
    end

    always_ff @(posedge clk) begin
        rx_write <= 1'b0;
        tx_read <= 1'b0;
        modem_status_read <= 1'b0;
        phase <= {phase[2:0], phase[3]};

        if (reset) begin
            usb_clk <= 1'b0;
            usb_cs <= 1'b1;
            usb_miosi_oe <= 1'b0;
            reset_pending <= 1'b0;
            write_buffer_flush_pending <= 1'b0;
            write_modem_status_pending <= 1'b0;
            modem_status_counter <= 5'd0;
            last_reset_status <= 1'b0;
            reset_reply <= 1'b0;
            phase <= 4'b0001;
            state <= STATE_IDLE;
        end else begin
            if (write_buffer_flush) begin
                write_buffer_flush_pending <= 1'b1;
            end

            if (reset_ack) begin
                reset_pending <= 1'b0;
                write_modem_status_pending <= 1'b1;
                reset_reply <= 1'b1;
            end

            if (modem_status_read) begin
                last_reset_status <= usb_miosi[0];
                if (!last_reset_status && usb_miosi[0]) begin
                    reset_pending <= 1'b1;
                end
                if (last_reset_status && !usb_miosi[0]) begin
                    write_modem_status_pending <= 1'b1;
                    reset_reply <= 1'b0;
                end
            end

            case (state)
                STATE_IDLE: begin
                    usb_cs <= 1'b0;
                    state <= STATE_SELECT;
                    if (write_modem_status_pending) begin
                        usb_cmd <= CMD_WRITE_MODEM_STATUS;
                    end else if (&modem_status_counter) begin
                        usb_cmd <= CMD_READ_MODEM_STATUS;
                    end else if (!tx_empty) begin
                        usb_cmd <= CMD_WRITE;
                    end else if (write_buffer_flush_pending) begin
                        usb_cmd <= CMD_WRITE_BUFFER_FLUSH;
                    end else if (!rx_full) begin
                        usb_cmd <= CMD_READ;
                    end else begin
                        usb_cs <= 1'b1;
                        modem_status_counter <= modem_status_counter + 1'd1;
                        state <= STATE_IDLE;
                    end
                end

                STATE_SELECT: begin
                    phase <= 4'b0001;
                    state <= STATE_COMMAND;
                end

                STATE_COMMAND: begin
                    if (phase[0]) begin
                        usb_clk <= 1'b1;
                        usb_miosi_oe <= 1'b1;
                    end else if (phase[2]) begin
                        usb_clk <= 1'b0;
                    end else if (phase[3]) begin
                        state <= STATE_STATUS;
                    end
                end

                STATE_STATUS: begin
                    if (phase[0]) begin
                        usb_clk <= 1'b1;
                        usb_miosi_oe <= 1'b0;
                    end else if (phase[2]) begin
                        usb_clk <= 1'b0;
                    end else if (phase[3]) begin
                        state <= STATE_DATA;
                    end
                end

                STATE_DATA: begin
                    if (phase[0]) begin
                        usb_clk <= 1'b1;
                        usb_miosi_oe <= (usb_cmd == CMD_WRITE) || (usb_cmd == CMD_WRITE_MODEM_STATUS);
                    end else if (phase[2]) begin
                        usb_clk <= 1'b0;
                    end else if (phase[3]) begin
                        if (usb_miso) begin
                            state <= STATE_DESELECT;
                        end else if (usb_cmd == CMD_WRITE) begin
                            tx_read <= 1'b1;
                            if (tx_almost_empty) begin
                                state <= STATE_DESELECT;
                            end
                        end else if (usb_cmd == CMD_READ) begin
                            rx_write <= 1'b1;
                            if (rx_almost_full) begin
                                state <= STATE_DESELECT;
                            end
                        end else if (usb_cmd == CMD_READ_MODEM_STATUS) begin
                            modem_status_read <= 1'b1;
                            state <= STATE_DESELECT;
                        end else if (usb_cmd == CMD_WRITE_MODEM_STATUS) begin
                            write_modem_status_pending <= 1'b0;
                            state <= STATE_DESELECT;
                        end else if (usb_cmd == CMD_WRITE_BUFFER_FLUSH) begin
                            write_buffer_flush_pending <= 1'b0;
                            state <= STATE_DESELECT;
                        end
                    end
                end

                STATE_DESELECT: begin
                    usb_cs <= 1'b1;
                    usb_miosi_oe <= 1'b0;
                    if (phase[1]) begin
                        modem_status_counter <= modem_status_counter + 1'd1;
                        state <= STATE_IDLE;
                    end
                end
            endcase
        end
    end

endmodule
