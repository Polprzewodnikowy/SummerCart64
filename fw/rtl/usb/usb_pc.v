module usb_pc (
    input i_clk,
    input i_reset,

    output o_ftdi_clk,
    output o_ftdi_si,
    input i_ftdi_so,
    input i_ftdi_cts,

    output reg o_request,
    output reg o_write,
    input i_busy,
    input i_ack,
    output reg [3:0] o_bank,
    output reg [25:0] o_address,
    input [31:0] i_data,
    output reg [31:0] o_data,

    input i_debug_start,
    output o_debug_busy,
    input [3:0] i_debug_bank,
    input [23:0] i_debug_address,
    input [19:0] i_debug_length,

    input i_debug_fifo_request,
    input i_debug_fifo_flush,
    output [10:0] o_debug_fifo_items,
    output [31:0] o_debug_fifo_data
);

    // Module parameters

    parameter byte VERSION = "a";


    // FTDI transport

    reg r_ftdi_rx_ready;
    wire w_ftdi_rx_valid;
    wire [7:0] w_ftdi_rx_data;

    wire w_ftdi_tx_busy;
    reg r_ftdi_tx_valid;
    reg [7:0] r_ftdi_tx_data;

    usb_ftdi_fsi usb_ftdi_fsi_inst (
        .i_clk(i_clk),
        .i_reset(i_reset),

        .o_ftdi_clk(o_ftdi_clk),
        .o_ftdi_si(o_ftdi_si),
        .i_ftdi_so(i_ftdi_so),
        .i_ftdi_cts(i_ftdi_cts),

        .i_rx_ready(r_ftdi_rx_ready),
        .o_rx_valid(w_ftdi_rx_valid),
        .o_rx_channel(1'bZ),
        .o_rx_data(w_ftdi_rx_data),

        .o_tx_busy(w_ftdi_tx_busy),
        .i_tx_valid(r_ftdi_tx_valid),
        .i_tx_channel(1'b1),            // Channel B
        .i_tx_data(r_ftdi_tx_data)
    );


    // Debug FIFO

    wire w_fifo_usb_full;
    wire [9:0] w_fifo_usb_items;
    reg r_fifo_usb_write_request;
    reg [31:0] r_fifo_usb_data;

    assign o_debug_fifo_items = {w_fifo_usb_full, w_fifo_usb_items};

    fifo_usb fifo_usb_inst (
        .clock(i_clk),
        .sclr(i_debug_fifo_flush),

        .full(w_fifo_usb_full),
        .usedw(w_fifo_usb_items),

        .wrreq(r_fifo_usb_write_request),
        .data(r_fifo_usb_data),

        .rdreq(i_debug_fifo_request),
        .q(o_debug_fifo_data)
    );


    // Command ids

    localparam byte CMD_TRIGGER [0:2]   = '{"C", "M", "D"};

    localparam byte CMD_IDENTIFY        = "I";
    localparam byte CMD_WRITE           = "W";
    localparam byte CMD_DEBUG_WRITE     = "D";
    localparam byte CMD_DEBUG_SEND      = "Q";

    localparam [1:0] RX_STAGE_CMD       = 2'd0;
    localparam [1:0] RX_STAGE_PARAM     = 2'd1;
    localparam [1:0] RX_STAGE_DATA      = 2'd2;
    localparam [1:0] RX_STAGE_IGNORE    = 2'd3;


    // RX module

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
            if (i_debug_start) begin
                r_rx_stage <= RX_STAGE_IGNORE;
                r_rx_cmd <= CMD_DEBUG_SEND;
                r_tx_cmd <= CMD_DEBUG_SEND;
                r_tx_cmd_valid <= 1'b1;
            end

            if (w_ftdi_rx_valid) begin
                r_rx_byte_counter <= r_rx_byte_counter + 3'd1;

                case (r_rx_stage)
                    RX_STAGE_CMD: begin
                        if (w_ftdi_rx_data != CMD_TRIGGER[r_rx_byte_counter[1:0]]) begin
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

                                CMD_WRITE: r_rx_stage <= RX_STAGE_PARAM;

                                CMD_DEBUG_WRITE: r_rx_stage <= RX_STAGE_PARAM;

                                default: r_rx_stage <= RX_STAGE_CMD;
                            endcase
                        end
                    end

                    RX_STAGE_PARAM: begin
                        r_rx_buffer <= {r_rx_buffer[55:0], w_ftdi_rx_data};

                        case (r_rx_cmd)
                            CMD_WRITE: begin
                                if (r_rx_byte_counter == 3'd7) begin
                                    r_rx_stage <= RX_STAGE_DATA;
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
                                        r_tx_cmd <= r_rx_cmd;
                                    end
                                    r_rx_byte_counter <= 3'd0;
                                    r_rx_buffer_valid <= 1'b1;
                                end
                            end

                            CMD_DEBUG_WRITE: begin
                                if (r_rx_byte_counter == 3'd3) begin
                                    if (r_data_items_remaining == 20'd0) begin
                                        r_rx_stage <= RX_STAGE_CMD;
                                    end
                                    r_rx_byte_counter <= 3'd0;
                                    r_rx_buffer_valid <= 1'b1;
                                end
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


    // Command parameter decoder

    always @(posedge i_clk) begin
        if (i_reset) begin
        end else begin
            if (r_rx_param_valid) begin
                case (r_rx_cmd)
                    CMD_WRITE: begin
                        o_address <= {r_rx_buffer[57:34], 2'b00};
                        o_bank <= r_rx_buffer[27:24];
                        r_data_items_remaining <= r_rx_buffer[19:0];
                    end

                    CMD_DEBUG_WRITE: begin
                        r_data_items_remaining <= r_rx_buffer[19:0];
                    end
                endcase
            end

            if (r_tx_cmd_valid && r_tx_cmd == CMD_DEBUG_SEND) begin
                o_bank <= i_debug_bank;
                o_address <= {i_debug_address, 2'b00};
                r_data_items_remaining <= i_debug_length;
            end

            if (o_request && !i_busy && r_data_items_remaining > 20'd0) begin
                o_address[25:2] <= o_address[25:2] + 1'd1;
                r_data_items_remaining <= r_data_items_remaining - 1'd1;
            end

            if (r_fifo_usb_write_request && r_data_items_remaining > 20'd0) begin
                r_data_items_remaining <= r_data_items_remaining - 1'd1;
            end
        end
    end


    // TX module

    localparam byte IDENTIFY_STRING [0:3]   = '{"S", "6", "4", VERSION};
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
                    CMD_DEBUG_SEND: r_ftdi_tx_data = r_i_data_buffer[(((4 - {2'b00, r_tx_byte_counter}) * 8) - 1) -: 8];
                    default: begin end
                endcase
            end

            TX_STAGE_RESPONSE: begin
                if (r_tx_byte_counter != 2'd3) r_ftdi_tx_data = RSP_COMPLETE[r_tx_byte_counter];
                else r_ftdi_tx_data = r_tx_cmd;
            end

            default: begin end
        endcase
    end

    wire w_tx_successful = r_ftdi_tx_valid && !w_ftdi_tx_busy;

    reg r_bus_data_request;
    reg r_bus_data_valid;
    reg r_bus_data_feisable;

    assign o_debug_busy = r_tx_stage == TX_STAGE_DATA && r_tx_cmd == CMD_DEBUG_SEND;

    always @(posedge i_clk) begin
        r_ftdi_tx_valid <= 1'b0;
        r_tx_done <= 1'b0;
        r_bus_data_request <= 1'b0;

        if (i_reset) begin
            r_tx_stage <= TX_STAGE_IDLE;
        end else begin
            if (w_tx_successful) r_tx_byte_counter <= r_tx_byte_counter + 2'd1;

            case (r_tx_stage)
                TX_STAGE_IDLE: begin
                    r_tx_byte_counter <= 2'd0;

                    if (r_tx_cmd_valid) begin
                        case (r_tx_cmd)
                            CMD_IDENTIFY: begin
                                r_tx_stage <= TX_STAGE_DATA;
                                r_ftdi_tx_valid <= 1'b1;
                            end

                            CMD_DEBUG_SEND: begin
                                r_bus_data_request <= 1'b1;
                                r_tx_stage <= TX_STAGE_DATA;
                            end

                            default: begin
                                r_tx_stage <= TX_STAGE_RESPONSE;
                                r_ftdi_tx_valid <= 1'b1;
                            end
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
                        endcase
                    end

                    if (r_tx_cmd == CMD_DEBUG_SEND) begin
                        r_ftdi_tx_valid <= 1'b0;

                        if (r_bus_data_valid) begin
                            r_bus_data_feisable <= 1'b1;
                        end

                        if (r_bus_data_feisable) begin
                            r_ftdi_tx_valid <= 1'b1;
                        end

                        if (w_tx_successful) begin
                            if (r_tx_byte_counter == 2'd3 && r_bus_data_feisable) begin
                                r_bus_data_request <= 1'b1;
                                r_bus_data_feisable <= 1'b0;
                            end
                        end

                        if (w_tx_successful && r_data_items_remaining == 20'd0 && r_tx_byte_counter == 2'd3) begin
                            r_bus_data_request <= 1'b0;
                            r_tx_stage <= TX_STAGE_IDLE;
                            r_tx_done <= 1'b1;
                            r_bus_data_feisable <= 1'b0;
                        end
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


    // Bus controller

    always @(posedge i_clk) begin
        o_request <= 1'b0;
        o_write <= 1'b0;
        r_bus_data_valid <= 1'b0;
        r_fifo_usb_write_request <= 1'b0;

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

                CMD_DEBUG_WRITE: begin
                    if (r_rx_buffer_valid) begin
                        r_fifo_usb_data <= r_rx_buffer[31:0];
                        r_fifo_usb_write_request <= 1'b1;
                    end

                    if (w_fifo_usb_full) begin
                        r_ftdi_rx_ready <= 1'b0;
                    end else begin
                        r_ftdi_rx_ready <= 1'b1;
                    end
                end
            endcase

            if (r_bus_data_request) begin
                o_request <= 1'b1;
            end

            if (o_request && i_busy) begin
                o_request <= 1'b1;
            end

            if (i_ack) begin
                r_i_data_buffer <= i_data;
                r_bus_data_valid <= 1'b1;
            end
        end
    end

endmodule
