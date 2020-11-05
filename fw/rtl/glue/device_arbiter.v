module device_arbiter (
    input i_clk,
    input i_reset,
    
    input i_request_pri,
    input i_write_pri,
    output o_busy_pri,
    output o_ack_pri,
    input [3:0] i_bank_pri,
    input [25:0] i_address_pri,
    output [31:0] o_data_pri,
    input [31:0] i_data_pri,

    input i_request_sec,
    input i_write_sec,
    output o_busy_sec,
    output o_ack_sec,
    input [3:0] i_bank_sec,
    input [25:0] i_address_sec,
    output [31:0] o_data_sec,
    input [31:0] i_data_sec,

    output o_request,
    output o_write,
    input i_busy,
    input i_ack,
    output [25:0] o_address,
    input [31:0] i_data,
    output [31:0] o_data
);

    parameter [3:0] DEVICE_BANK = 4'd0;

    wire w_request_pri = i_request_pri && i_bank_pri == DEVICE_BANK;
    wire w_request_sec = i_request_sec && i_bank_sec == DEVICE_BANK;

    wire w_request_pri_successful = w_request_pri && !o_busy_pri;
    wire w_request_sec_successful = w_request_sec && !o_busy_sec;

    wire w_read_fifo_ack_full;
    wire w_read_fifo_ack_pri_sec;

    fifo_ack fifo_ack_inst (
        .clock(i_clk),
        .sclr(w_read_fifo_ack_reset),

        .data(w_request_sec_successful && !i_write_sec),
        .wrreq((w_request_pri_successful && !i_write_pri) || (w_request_sec_successful && !i_write_sec)),
        .full(w_read_fifo_ack_full),

        .rdreq(i_ack),
        .q(w_read_fifo_ack_pri_sec)
    );

    assign o_busy_pri = w_request_pri && (i_busy || (!i_write_pri && w_read_fifo_ack_full));
    assign o_ack_pri = i_ack && !w_read_fifo_ack_pri_sec;
    assign o_data_pri = i_data;

    assign o_busy_sec = w_request_sec && (i_request_pri || i_busy || (!i_write_sec && w_read_fifo_ack_full));
    assign o_ack_sec = i_ack && w_read_fifo_ack_pri_sec;
    assign o_data_sec = i_data;

    assign o_request = w_request_pri || w_request_sec;

    always @(*) begin
        o_write = 1'b0;
        o_address = 26'd0;
        o_data = 32'd0;
        if (w_request_pri_successful) begin
            o_write = i_write_pri;
            o_address = i_address_pri;
            o_data = i_data_pri;
        end else if (w_request_sec_successful) begin
            o_write = i_write_sec;
            o_address = i_address_sec;
            o_data = i_data_sec;
        end
    end

endmodule
