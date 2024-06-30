interface usb_scb ();

    logic fifo_flush;
    logic fifo_flush_busy;
    logic write_buffer_flush;
    logic [10:0] rx_count;
    logic [10:0] tx_count;
    logic pwrsav;
    logic reset_state;
    logic reset_on_ack;
    logic reset_off_ack;

    modport controller (
        output fifo_flush,
        input fifo_flush_busy,
        output write_buffer_flush,
        input rx_count,
        input tx_count,
        input pwrsav,
        input reset_state,
        output reset_on_ack,
        output reset_off_ack
    );

    modport usb (
        input fifo_flush,
        output fifo_flush_busy,
        input write_buffer_flush,
        output rx_count,
        output tx_count,
        output pwrsav,
        output reset_state,
        input reset_on_ack,
        input reset_off_ack
    );

endinterface


module usb_ft1248 (
    input clk,
    input reset,

    usb_scb.usb usb_scb,

    fifo_bus.fifo fifo_bus,

    input usb_pwrsav,
    output logic usb_clk,
    output logic usb_cs,
    input usb_miso,
    inout [7:0] usb_miosi
);

    logic rx_full;
    logic rx_almost_full;
    logic rx_write;
    logic [7:0] rx_wdata;

    logic tx_empty;
    logic tx_almost_empty;
    logic tx_read;
    logic [7:0] tx_rdata;

    logic fifo_flush;

    fifo_8kb fifo_8kb_rx_inst (
        .clk(clk),
        .reset(reset || fifo_flush),

        .empty(fifo_bus.rx_empty),
        .almost_empty(fifo_bus.rx_almost_empty),
        .read(fifo_bus.rx_read),
        .rdata(fifo_bus.rx_rdata),

        .full(rx_full),
        .almost_full(rx_almost_full),
        .write(rx_write),
        .wdata(rx_wdata),

        .count(usb_scb.rx_count)
    );

    fifo_8kb fifo_8kb_tx_inst (
        .clk(clk),
        .reset(reset || fifo_flush),

        .empty(tx_empty),
        .almost_empty(tx_almost_empty),
        .read(tx_read),
        .rdata(tx_rdata),

        .full(fifo_bus.tx_full),
        .almost_full(fifo_bus.tx_almost_full),
        .write(fifo_bus.tx_write),
        .wdata(fifo_bus.tx_wdata),

        .count(usb_scb.tx_count)
    );

    logic [1:0] usb_pwrsav_ff;
    logic [7:0] usb_miosi_out;
    logic usb_oe;

    logic ft_pwrsav;
    logic ft_clk;
    logic ft_cs;
    logic ft_miso;
    logic [7:0] ft_miosi_in;
    logic [7:0] ft_miosi_out;
    logic ft_oe;

    always_ff @(posedge clk) begin
        usb_pwrsav_ff <= {usb_pwrsav_ff[0], usb_pwrsav};
        ft_pwrsav <= usb_pwrsav_ff[1];
        usb_clk <= ft_clk;
        usb_cs <= ft_cs;
        ft_miso <= usb_miso;
        ft_miosi_in <= usb_miosi;
        usb_miosi_out <= ft_miosi_out;
        usb_oe <= ft_oe;
    end

    assign usb_miosi = usb_oe ? usb_miosi_out : 8'hZZ;

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
    } e_cmd;

    e_state state;
    e_state next_state;
    e_cmd cmd;
    e_cmd next_cmd;
    logic [3:0] phase;
    logic last_tx_failed;
    logic reset_reply;
    logic [4:0] modem_status_counter;
    logic write_modem_status_pending;
    logic write_buffer_flush_pending;

    always_ff @(posedge clk) begin
        state <= next_state;
        cmd <= next_cmd;

        usb_scb.pwrsav <= !ft_pwrsav;
        fifo_flush <= 1'b0;

        phase <= {phase[2:0], phase[3]};
        if (state == STATE_IDLE) begin
            phase <= 4'b0100;
        end

        if (reset) begin
            usb_scb.fifo_flush_busy <= 1'b0;
            usb_scb.reset_state <= 1'b0;
            last_tx_failed <= 1'b0;
            reset_reply <= 1'b0;
            modem_status_counter <= 5'd0;
            write_modem_status_pending <= 1'b1;
            write_buffer_flush_pending <= 1'b0;
        end else begin
            if (usb_scb.fifo_flush) begin
                usb_scb.fifo_flush_busy <= 1'b1;
            end

            if (usb_scb.reset_on_ack) begin
                reset_reply <= 1'b1;
                write_modem_status_pending <= 1'b1;
            end

            if (usb_scb.reset_off_ack) begin
                reset_reply <= 1'b0;
                write_modem_status_pending <= 1'b1;
            end

            if (usb_scb.write_buffer_flush) begin
                write_buffer_flush_pending <= 1'b1;
            end

            if (state == STATE_IDLE) begin
                modem_status_counter <= modem_status_counter + 1'd1;
                if (usb_scb.fifo_flush_busy) begin
                    usb_scb.fifo_flush_busy <= 1'b0;
                    fifo_flush <= 1'b1;
                end
            end

            if ((state == STATE_DATA) && (cmd == CMD_WRITE) && phase[3]) begin
                last_tx_failed <= ft_miso;
            end

            if (!ft_miso && (state == STATE_DATA) && phase[3]) begin
                if (cmd == CMD_READ_MODEM_STATUS) begin
                    usb_scb.reset_state <= ft_miosi_in[0];
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

        if (!ft_miso && phase[3]) begin
            case (state)
                STATE_STATUS: begin
                    if (cmd == CMD_WRITE && !last_tx_failed) begin
                        tx_read = 1'b1;
                    end
                end

                STATE_DATA: begin
                    if (cmd == CMD_READ) begin
                        rx_write = 1'b1;
                    end
                    if (cmd == CMD_WRITE && !tx_empty) begin
                        tx_read = 1'b1;
                    end
                end
            endcase
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
                    if (ft_pwrsav && !(usb_scb.fifo_flush || usb_scb.fifo_flush_busy || fifo_flush)) begin
                        if (write_modem_status_pending) begin
                            next_state = STATE_SELECT;
                            next_cmd = CMD_WRITE_MODEM_STATUS;
                        end else if (&modem_status_counter) begin
                            next_state = STATE_SELECT;
                            next_cmd = CMD_READ_MODEM_STATUS;
                        end else if (!tx_empty || last_tx_failed) begin
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
                            if (tx_empty) begin
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
