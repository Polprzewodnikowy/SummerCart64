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
    logic [7:0] rx_wdata;

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
        .data(rx_wdata)
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

    logic [7:0] usb_miosi_out;
    logic usb_oe;

    logic ft_clk;
    logic ft_cs;
    logic ft_miso;
    logic [7:0] ft_miosi_in;
    logic [7:0] ft_miosi_out;
    logic ft_oe;

    always_ff @(posedge clk) begin
        usb_clk <= ft_clk;
        usb_cs <= ft_cs;
        ft_miso <= usb_miso;
        ft_miosi_in <= usb_miosi;
        usb_miosi_out <= ft_miosi_out;
        usb_oe <= ft_oe;
    end

    always_comb begin
        usb_miosi = usb_oe ? usb_miosi_out : 8'hZZ;
    end

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

    e_state state;
    e_state next_state;
    e_command cmd;
    e_command next_cmd;
    logic [3:0] phase;
    logic reset_reply;
    logic last_reset_status;
    logic [4:0] modem_status_counter;
    logic write_modem_status_pending;
    logic write_buffer_flush_pending;

    always_ff @(posedge clk) begin
        state <= next_state;
        cmd <= next_cmd;
        
        phase <= {phase[2:0], phase[3]};
        if (state == STATE_IDLE) begin
            phase <= 4'b0100;
        end

        if (reset) begin
            reset_pending <= 1'b0;
            last_reset_status <= 1'b0;
            modem_status_counter <= 5'd0;
            write_modem_status_pending <= 1'b0;
            write_buffer_flush_pending <= 1'b0;
        end else begin
            if (reset_ack) begin
                reset_pending <= 1'b0;
                reset_reply <= 1'b1;
                write_modem_status_pending <= 1'b1;
            end

            if (write_buffer_flush) begin
                write_buffer_flush_pending <= 1'b1;
            end

            if (state == STATE_IDLE) begin
                modem_status_counter <= modem_status_counter + 1'd1;
            end

            if (!ft_miso && (state == STATE_DATA) && phase[3]) begin
                if (cmd == CMD_READ_MODEM_STATUS) begin
                    last_reset_status <= ft_miosi_in[0];
                    if (!last_reset_status && ft_miosi_in[0]) begin
                        reset_pending <= 1'b1;
                    end
                    if (last_reset_status && !ft_miosi_in[0]) begin
                        reset_reply <= 1'b0;
                        write_modem_status_pending <= 1'b1;
                    end
                end
                if (cmd == CMD_WRITE_MODEM_STATUS) begin
                    write_modem_status_pending <= 1'b0;
                end
                if (cmd == CMD_WRITE_BUFFER_FLUSH) begin
                    write_buffer_flush_pending <= 1'b0;
                end
            end
        end
    end

    always_comb begin
        ft_clk = 1'b0;
        ft_cs = 1'b1;
        ft_miosi_out = 8'hFF;
        ft_oe = 1'b0;

        if (state == STATE_SELECT) begin
            ft_cs = 1'b0;
        end

        if (state == STATE_COMMAND) begin
            if (phase[0] || phase[1]) begin
                ft_clk = 1'b1;
            end
            ft_cs = 1'b0;
            ft_miosi_out = cmd;
            ft_oe = 1'b1;
        end

        if (state == STATE_STATUS) begin
            if (phase[0] || phase[1]) begin
                ft_clk = 1'b1;
            end
            ft_cs = 1'b0;
        end

        if (state == STATE_DATA) begin
            if (phase[0] || phase[1]) begin
                ft_clk = 1'b1;
            end
            ft_cs = 1'b0;
            if (cmd == CMD_WRITE) begin
                ft_miosi_out = tx_rdata;
                ft_oe = 1'b1;
            end
            if (cmd == CMD_WRITE_MODEM_STATUS) begin
                ft_miosi_out = {2'b00, reset_reply, 5'b00000};
                ft_oe = 1'b1;
            end
        end
    end

    always_comb begin
        rx_write = 1'b0;
        tx_read = 1'b0;

        rx_wdata = ft_miosi_in;

        if (!ft_miso && (state == STATE_DATA) && phase[3]) begin
            if (cmd == CMD_READ) begin
                rx_write = 1'b1;
            end
            if (cmd == CMD_WRITE) begin
                tx_read = 1'b1; 
            end
        end
    end

    always_comb begin
        next_state = state;
        next_cmd = cmd;

        if (reset) begin
            next_state = STATE_IDLE;
        end else begin
            case (state)
                STATE_IDLE: begin
                    if (write_modem_status_pending) begin
                        next_state = STATE_SELECT;
                        next_cmd = CMD_WRITE_MODEM_STATUS;
                    end else if (&modem_status_counter) begin
                        next_state = STATE_SELECT;
                        next_cmd = CMD_READ_MODEM_STATUS;
                    end else if (!tx_empty) begin
                        next_state = STATE_SELECT;
                        next_cmd = CMD_WRITE;
                    end else if (write_buffer_flush_pending) begin
                        next_state = STATE_SELECT;
                        next_cmd = CMD_WRITE_BUFFER_FLUSH;
                    end else if (!rx_full) begin
                        next_state = STATE_SELECT;
                        next_cmd = CMD_READ;
                    end
                end

                STATE_SELECT: begin
                    if (phase[3]) begin
                        next_state = STATE_COMMAND;
                    end
                end

                STATE_COMMAND: begin
                    if (phase[3]) begin
                        next_state = STATE_STATUS;
                    end
                end

                STATE_STATUS: begin
                    if (phase[3]) begin
                        if (ft_miso) begin
                            next_state = STATE_DESELECT;
                        end else begin
                            next_state = STATE_DATA;
                        end
                    end
                end

                STATE_DATA: begin
                    if (phase[3]) begin
                        if (ft_miso) begin
                            next_state = STATE_DESELECT;
                        end else if (cmd == CMD_READ) begin
                            if (rx_almost_full) begin
                                next_state = STATE_DESELECT;
                            end
                        end else if (cmd == CMD_WRITE) begin
                            if (tx_almost_empty) begin
                                next_state = STATE_DESELECT;
                            end
                        end else begin
                            next_state = STATE_DESELECT;
                        end
                    end
                end

                STATE_DESELECT: begin
                    if (phase[1]) begin
                        next_state = STATE_IDLE;
                    end
                end

                default: begin
                    next_state = STATE_IDLE;
                end
            endcase
        end
    end

endmodule
