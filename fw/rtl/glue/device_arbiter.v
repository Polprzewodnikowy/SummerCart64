module device_arbiter #(
    parameter NUM_CONTROLLERS = 2,
    parameter ADDRESS_WIDTH = 26,
    parameter [3:0] DEVICE_BANK = 4'd0,
    parameter ACK_FIFO_LENGTH = 4
) (
    input i_clk,
    input i_reset,

    input [(NUM_CONTROLLERS - 1):0] i_request,
    input [(NUM_CONTROLLERS - 1):0] i_write,
    output reg [(NUM_CONTROLLERS - 1):0] o_busy,
    output reg [(NUM_CONTROLLERS - 1):0] o_ack,
    input [((NUM_CONTROLLERS * 4) - 1):0] i_bank,
    input [((NUM_CONTROLLERS * ADDRESS_WIDTH) - 1):0] i_address,
    output reg [((NUM_CONTROLLERS * 32) - 1):0] o_data,
    input [((NUM_CONTROLLERS * 32) - 1):0] i_data,

    output reg o_device_request,
    output reg o_device_write,
    input i_device_busy,
    input i_device_ack,
    output reg [(ADDRESS_WIDTH - 1):0] o_device_address,
    input [31:0] i_device_data,
    output reg [31:0] o_device_data
);

    localparam FIFO_ADDRESS_WIDTH = $clog2(ACK_FIFO_LENGTH);

    reg [(NUM_CONTROLLERS - 1):0] r_request;
    reg [(NUM_CONTROLLERS - 1):0] r_request_successful;

    reg [(NUM_CONTROLLERS - 1):0] r_ack_fifo_mem [0:(ACK_FIFO_LENGTH - 1)];
    
    reg [FIFO_ADDRESS_WIDTH:0] r_ack_fifo_wrptr;
    reg [FIFO_ADDRESS_WIDTH:0] r_ack_fifo_rdptr;

    wire w_ack_fifo_wrreq = |(r_request_successful & ~i_write);
    wire w_ack_fifo_rdreq = i_device_ack;

    wire w_empty = r_ack_fifo_wrptr[FIFO_ADDRESS_WIDTH] == r_ack_fifo_rdptr[FIFO_ADDRESS_WIDTH];
    wire w_full_or_empty = r_ack_fifo_wrptr[(FIFO_ADDRESS_WIDTH - 1):0] == r_ack_fifo_rdptr[(FIFO_ADDRESS_WIDTH - 1):0];

    wire w_ack_fifo_full = !w_empty && w_full_or_empty;

    always @(posedge i_clk) begin
        if (i_reset) begin
            r_ack_fifo_wrptr <= {(FIFO_ADDRESS_WIDTH + 1){1'b0}};
            r_ack_fifo_rdptr <= {(FIFO_ADDRESS_WIDTH + 1){1'b0}};
        end else begin
            if (w_ack_fifo_wrreq) begin
                r_ack_fifo_mem[r_ack_fifo_wrptr[(FIFO_ADDRESS_WIDTH - 1):0]] <= r_request_successful & ~i_write;
                r_ack_fifo_wrptr <= r_ack_fifo_wrptr + 1'd1;
            end
            if (w_ack_fifo_rdreq) begin
                r_ack_fifo_rdptr <= r_ack_fifo_rdptr + 1'd1;
            end
        end
    end

    always @(*) begin
        for (integer i = 0; i < NUM_CONTROLLERS; i = i + 1) begin
            r_request[i] = i_request[i] && i_bank[(i * 4) +: 4] == DEVICE_BANK;
            o_busy[i] = r_request[i] && (
                i_device_busy ||
                |(r_request & (({{(NUM_CONTROLLERS - 1){1'b0}}, 1'b1} << i) - 1)) ||
                (!i_write[i] && w_ack_fifo_full)
            );
        end

        r_request_successful = r_request & ~o_busy;
        o_ack = {NUM_CONTROLLERS{i_device_ack}} & r_ack_fifo_mem[r_ack_fifo_rdptr[(FIFO_ADDRESS_WIDTH - 1):0]];
        o_data = {NUM_CONTROLLERS{i_device_data}};
    end

    always @(*) begin
        o_device_request = |r_request;
        o_device_write = 1'b0;
        o_device_address = {ADDRESS_WIDTH{1'b0}};
        o_device_data = 32'h0000_0000;

        for (integer i = (NUM_CONTROLLERS - 1); i >= 0 ; i = i - 1) begin
            if (r_request[i]) begin
                o_device_write = i_write[i];
                o_device_address = i_address[(i * ADDRESS_WIDTH) +: ADDRESS_WIDTH];
                o_device_data = i_data[(i * 32) +: 32];
            end
        end
    end

endmodule
