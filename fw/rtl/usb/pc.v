module pc (
    input i_clk,
    input i_reset,

    output o_ftdi_clk,
    output o_ftdi_si,
    input i_ftdi_so,
    input i_ftdi_cts,

    output reg [3:0] o_bank_select,
    output reg o_request,
    output reg o_write,
    input i_busy,
    input i_ack,
    output reg [27:0] o_address,
    input [31:0] i_data,
    output reg [31:0] o_data
);

    reg r_ftdi_rx_ready;
    wire w_ftdi_rx_valid;
    wire [7:0] w_ftdi_rx_data;

    wire w_ftdi_tx_busy;
    reg r_ftdi_tx_valid;
    reg [7:0] r_ftdi_tx_data;

    ftdi_fsi ftdi_fsi_inst (
        .i_clk(i_clk),
        .i_reset(i_reset),

        .o_ftdi_clk(o_ftdi_clk),
        .o_ftdi_si(o_ftdi_si),
        .i_ftdi_so(i_ftdi_so),
        .i_ftdi_cts(i_ftdi_cts),

        .i_rx_ready(r_ftdi_rx_ready),
        .o_rx_valid(w_ftdi_rx_valid),
        .o_rx_data(w_ftdi_rx_data),

        .o_tx_busy(w_ftdi_tx_busy),
        .i_tx_valid(r_ftdi_tx_valid),
        .i_tx_channel(1'b1),            // Channel B
        .i_tx_data(r_ftdi_tx_data)
    );

    localparam byte CMD_TRIGGER [0:2]   = '{"C", "M", "D"};

    localparam byte CMD_IDENTIFY        = "I";
    // localparam byte CMD_READ            = "R";
    localparam byte CMD_WRITE           = "W";
    localparam byte CMD_DEBUG_MODE      = "D";
    localparam byte CMD_DEBUG_WRITE     = "F";

    localparam [1:0] RX_STAGE_CMD       = 2'd0;
    localparam [1:0] RX_STAGE_PARAM     = 2'd1;
    localparam [1:0] RX_STAGE_DATA      = 2'd2;
    localparam [1:0] RX_STAGE_IGNORE    = 2'd3;

    reg [1:0] r_rx_stage;
    reg [2:0] r_rx_byte_counter;
    reg [7:0] r_rx_cmd;
    reg r_rx_buffer_valid;
    reg r_rx_param_valid;
    reg [63:0] r_rx_buffer;

    reg [19:0] r_data_items_remaining;

    reg r_tx_cmd_valid;
    reg [7:0] r_tx_cmd;
    reg r_tx_done;

    always @(posedge i_clk) begin
        r_rx_buffer_valid <= 1'b0;
        r_rx_param_valid <= 1'b0;
        r_tx_cmd_valid <= 1'b0;

        if (i_reset) begin
            r_rx_stage <= RX_STAGE_CMD;
            r_rx_byte_counter <= 3'd0;
        end else begin
            if (w_ftdi_rx_valid) begin
                r_rx_byte_counter <= r_rx_byte_counter + 3'd1;

                case (r_rx_stage)
                    RX_STAGE_CMD: begin
                        if (w_ftdi_rx_data != CMD_TRIGGER[r_rx_byte_counter]) begin
                            r_rx_byte_counter <= 3'd0;
                        end

                        if (r_rx_byte_counter == 3'd3) begin
                            r_rx_byte_counter <= 3'd0;
                            r_rx_cmd <= w_ftdi_rx_data;

                            case (w_ftdi_rx_data)
                                CMD_IDENTIFY: begin
                                    r_rx_stage <= RX_STAGE_IGNORE;
                                    r_tx_cmd_valid <= 1'b1;
                                    r_tx_cmd <= w_ftdi_rx_data;
                                end

                                CMD_READ: r_rx_stage <= RX_STAGE_PARAM;

                                CMD_WRITE: r_rx_stage <= RX_STAGE_PARAM;

                                CMD_DEBUG_MODE: r_rx_stage <= RX_STAGE_PARAM;

                                CMD_DEBUG_WRITE: r_rx_stage <= RX_STAGE_DATA;

                                default: r_rx_stage <= RX_STAGE_CMD;
                            endcase
                        end
                    end

                    RX_STAGE_PARAM: begin
                        r_rx_buffer <= {r_rx_buffer[55:0], w_ftdi_rx_data};

                        case (r_rx_cmd)
                            CMD_READ: begin
                                if (r_rx_byte_counter == 3'd7) begin
                                    r_rx_stage <= RX_STAGE_IGNORE;
                                    r_rx_byte_counter <= 3'd0;
                                    r_rx_param_valid <= 1'b1;
                                    r_tx_cmd_valid <= 1'b1;
                                    r_tx_cmd <= r_rx_cmd;
                                end
                            end

                            CMD_WRITE: begin
                                if (r_rx_byte_counter == 3'd7) begin
                                    r_rx_stage <= RX_STAGE_DATA;
                                    r_rx_byte_counter <= 3'd0;
                                    r_rx_param_valid <= 1'b1;
                                end
                            end

                            CMD_DEBUG_MODE: begin
                                if (r_rx_byte_counter == 3'd3) begin
                                    r_rx_stage <= RX_STAGE_CMD;
                                    r_rx_byte_counter <= 3'd0;
                                    r_rx_param_valid <= 1'b1;
                                end
                            end

                            CMD_DEBUG_WRITE: begin
                                if (r_rx_byte_counter == 3'd3) begin
                                    r_rx_stage <= RX_STAGE_DATA;
                                    r_rx_byte_counter <= 3'd0;
                                    r_rx_param_valid <= 1'b1;
                                end
                            end

                            default: begin
                                r_rx_stage <= RX_STAGE_CMD;
                                r_rx_byte_counter <= 3'd0;
                            end
                        endcase
                    end

                    RX_STAGE_DATA: begin
                        r_rx_buffer <= {r_rx_buffer[55:0], w_ftdi_rx_data};

                        case (r_rx_cmd)
                            CMD_WRITE: begin
                                if (r_rx_byte_counter == 3'd3) begin
                                    if (r_data_items_remaining == 20'd0) begin
                                        r_rx_stage <= RX_STAGE_IGNORE;
                                        r_tx_cmd_valid <= 1'b1;
                                        r_tx_cmd = r_rx_cmd;
                                    end
                                    r_rx_byte_counter <= 3'd0;
                                    r_rx_buffer_valid <= 1'b1;
                                end
                            end

                            CMD_DEBUG_WRITE: begin
                                if (r_data_items_remaining == 20'd0) begin
                                    r_rx_stage <= RX_STAGE_CMD;
                                end
                                r_rx_buffer_valid <= 1'b1;
                            end

                            default: begin
                                r_rx_stage <= RX_STAGE_CMD;
                                r_rx_byte_counter <= 3'd0;
                            end
                        endcase
                    end

                    RX_STAGE_IGNORE: begin
                    end

                    default: begin
                        r_rx_stage <= RX_STAGE_CMD;
                        r_rx_byte_counter <= 3'd0;
                    end
                endcase
            end

            if (r_tx_done) begin
                r_rx_stage <= RX_STAGE_CMD;
                r_rx_byte_counter <= 3'd0;
            end
        end
    end

    reg r_tx_debug_enabled;

    always @(posedge i_clk) begin
        if (i_reset) begin
            r_tx_debug_enabled <= 1'b0;
        end else begin
            if (r_rx_param_valid) begin
                case (r_rx_cmd)
                    CMD_READ, CMD_WRITE: begin
                        o_address <= {r_rx_buffer[63:34], 2'b00};
                        o_bank_select <= r_rx_buffer[27:24];
                        r_data_items_remaining <= r_rx_buffer[19:0];
                    end

                    CMD_DEBUG_MODE: begin
                        r_tx_debug_enabled <= r_rx_buffer[0];
                    end

                    CMD_DEBUG_WRITE: begin
                        r_data_items_remaining <= r_rx_buffer[19:0];
                    end
                endcase
            end

            if (o_request && !i_busy && r_data_items_remaining > 20'd0) begin
                o_address[27:2] <= o_address[27:2] + 26'd1;
                r_data_items_remaining <= r_data_items_remaining - 20'd1;
            end
        end
    end

    localparam byte IDENTIFY_STRING [0:3]   = '{"S", "6", "4", "a"};
    localparam byte RSP_COMPLETE [0:2]      = '{"C", "M", "P"};

    localparam [1:0] TX_STAGE_IDLE          = 2'd0;
    localparam [1:0] TX_STAGE_DATA          = 2'd1;
    localparam [1:0] TX_STAGE_RESPONSE      = 2'd2;

    reg [1:0] r_tx_stage;
    reg [1:0] r_tx_byte_counter;

    reg [31:0] r_i_data_buffer;

    always @(*) begin
        r_ftdi_tx_data = 8'h00;

        case (r_tx_stage)
            TX_STAGE_DATA: begin
                case (r_tx_cmd)
                    CMD_IDENTIFY: r_ftdi_tx_data = IDENTIFY_STRING[r_tx_byte_counter];
                    CMD_READ: r_ftdi_tx_data = r_i_data_buffer[(r_tx_byte_counter * 8) -: 8];
                endcase
            end

            TX_STAGE_RESPONSE: begin
                if (r_tx_byte_counter != 2'd3) r_ftdi_tx_data = RSP_COMPLETE[r_tx_byte_counter];
                else r_ftdi_tx_data = r_tx_cmd;
            end
        endcase
    end

    wire w_tx_successful = r_ftdi_tx_valid && !w_ftdi_tx_busy;

    always @(posedge i_clk) begin
        r_ftdi_tx_valid <= 1'b0;
        r_tx_done <= 1'b0;

        if (i_reset) begin
            r_tx_stage <= TX_STAGE_IDLE;
        end else begin
            if (w_tx_successful) r_tx_byte_counter <= r_tx_byte_counter + 2'd1;

            case (r_tx_stage)
                TX_STAGE_IDLE: begin
                    r_tx_byte_counter <= 2'd0;

                    if (r_tx_cmd_valid) begin
                        r_ftdi_tx_valid <= 1'b1;

                        case (r_tx_cmd)
                            CMD_IDENTIFY: r_tx_stage <= TX_STAGE_DATA;
                            default: r_tx_stage <= TX_STAGE_RESPONSE;
                        endcase
                    end
                end

                TX_STAGE_DATA: begin
                    r_ftdi_tx_valid <= 1'b1;

                    if (r_tx_byte_counter == 2'd3 && w_tx_successful) begin
                        case (r_tx_cmd)
                            CMD_IDENTIFY: begin
                                r_tx_stage <= TX_STAGE_RESPONSE;
                            end

                            CMD_READ: begin
                                if (r_data_items_remaining == 20'd0) begin
                                    r_tx_stage <= TX_STAGE_RESPONSE;
                                end
                            end

                            default: r_tx_stage <= TX_STAGE_RESPONSE;
                        endcase
                    end
                end

                TX_STAGE_RESPONSE: begin
                    r_ftdi_tx_valid <= 1'b1;

                    if (r_tx_byte_counter == 2'd3 && w_tx_successful) begin
                        r_tx_stage <= TX_STAGE_IDLE;
                        r_tx_done <= 1'b1;
                    end
                end

                default: r_tx_stage <= TX_STAGE_IDLE;
            endcase
        end
    end

    always @(posedge i_clk) begin
        o_request <= 1'b0;
        o_write <= 1'b0;

        if (i_reset) begin
            r_ftdi_rx_ready <= 1'b1;
        end else begin
            case (r_rx_cmd)
                CMD_WRITE: begin
                    if ((r_rx_buffer_valid || !r_ftdi_rx_ready) && !i_busy) begin
                        o_request <= 1'b1;
                        o_write <= 1'b1;
                        o_data <= r_rx_buffer[31:0];
                        r_ftdi_rx_ready <= 1'b1;
                    end

                    if (o_request && i_busy) begin
                        o_request <= 1'b1;
                        o_write <= 1'b1;
                    end

                    if (r_rx_buffer_valid && i_busy) begin
                        r_ftdi_rx_ready <= 1'b0;
                    end
                end
            endcase

            case (r_tx_cmd)
                CMD_READ: begin
                end
            endcase
        end
    end

endmodule
