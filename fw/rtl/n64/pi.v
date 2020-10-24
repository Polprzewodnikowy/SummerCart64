module n64_pi (
    input i_clk,
    input i_reset,

    input i_n64_reset,
    input i_n64_pi_alel,
    input i_n64_pi_aleh,
    input i_n64_pi_read,
    input i_n64_pi_write,
    inout [15:0] io_n64_pi_ad,

    output reg o_request,
    output reg o_write,
    input i_busy,
    input i_ack,
    output [3:0] o_bank,
    output reg [25:0] o_address,
    input [31:0] i_data,
    output reg [31:0] o_data,

    output reg o_debug
);

    localparam [1:0] PI_MODE_IDLE  = 2'b10;
    localparam [1:0] PI_MODE_HIGH  = 2'b11;
    localparam [1:0] PI_MODE_LOW   = 2'b01;
    localparam [1:0] PI_MODE_VALID = 2'b00;

    reg r_reset_ff1, r_reset_ff2;
    reg r_alel_ff1, r_alel_ff2;
    reg r_aleh_ff1, r_aleh_ff2;
    reg r_read_ff1, r_read_ff2;
    reg r_write_ff1, r_write_ff2;

    wire [1:0] w_pi_mode = {r_aleh_ff2, r_alel_ff2};
    reg [1:0] r_last_pi_mode;
    reg r_last_read;
    reg r_last_write;

    wire w_address_high_op = r_reset_ff2 && (r_last_pi_mode != PI_MODE_HIGH) && (w_pi_mode == PI_MODE_HIGH);
    wire w_address_low_op = r_reset_ff2 && (r_last_pi_mode != PI_MODE_LOW) && (w_pi_mode == PI_MODE_LOW);
    wire w_address_valid_op = r_reset_ff2 && (r_last_pi_mode != PI_MODE_VALID) && (w_pi_mode == PI_MODE_VALID);
    wire w_read_op = r_reset_ff2 && (w_pi_mode == PI_MODE_VALID) && r_last_read && !r_read_ff2;
    wire w_write_op = r_reset_ff2 && (w_pi_mode == PI_MODE_VALID) && r_last_write && !r_write_ff2;

    wire w_prefetch;

    reg [31:0] r_pi_address;
    reg [31:0] r_pi_read_buffer;
    reg [15:0] r_pi_write_buffer;
    reg r_word_counter;

    n64_bank_decoder n64_bank_decoder_inst (
        .i_address(r_pi_address),
        .o_bank(o_bank),
        .o_prefetch(w_prefetch)
    );

    always @(posedge i_clk) begin
        {r_reset_ff2, r_reset_ff1} <= {r_reset_ff1, i_n64_reset};
        {r_alel_ff2, r_alel_ff1} <= {r_alel_ff1, i_n64_pi_alel};
        {r_aleh_ff2, r_aleh_ff1} <= {r_aleh_ff1, i_n64_pi_aleh};
        {r_read_ff2, r_read_ff1} <= {r_read_ff1, i_n64_pi_read};
        {r_write_ff2, r_write_ff1} <= {r_write_ff1, i_n64_pi_write};
    end

    always @(posedge i_clk) begin
        r_last_pi_mode <= w_pi_mode;
        r_last_read <= r_read_ff2;
        r_last_write <= r_write_ff2;
    end

    always @(posedge i_clk) begin
        if (w_address_high_op) r_pi_address[31:16] <= io_n64_pi_ad;
        if (w_address_low_op) r_pi_address[15:0] <= {io_n64_pi_ad[15:1], 1'b0};
    end

    always @(posedge i_clk) begin
        if (w_address_valid_op) r_word_counter <= 1'b0;
        if (w_read_op || w_write_op) r_word_counter <= ~r_word_counter;
    end

    always @(posedge i_clk) begin
        if (w_write_op) r_pi_write_buffer <= {r_pi_write_buffer[15:0], io_n64_pi_ad};
    end

    always @(posedge i_clk) begin
        if (w_address_valid_op) o_address <= r_pi_address[25:0];
        if (o_request && !i_busy) o_address[8:2] <= o_address[8:2] + 1'b1;
    end

    reg [31:0] r_buffer;

    always @(posedge i_clk) begin
        if (i_reset) begin
            o_request <= 1'b0;
            o_write <= 1'b0;
        end else begin
            if (/*w_address_valid_op || */(w_read_op && !r_word_counter)) o_request <= 1'b1;
            if (o_request && !i_busy) o_request <= 1'b0;   
            if (i_ack) r_pi_read_buffer <= i_data;//i_data; //
            // if (w_read_op && !r_word_counter) r_pi_read_buffer <= r_buffer;
        end
    end

    always @(*) begin
        io_n64_pi_ad = 16'hZZZZ;

        if (r_reset_ff2 && !r_read_ff2) begin
            if (r_word_counter) io_n64_pi_ad = r_pi_read_buffer[15:0];
            else io_n64_pi_ad = r_pi_read_buffer[31:16];
        end
    end

    // always @(posedge i_clk) begin
    //     o_request <= 1'b0;


    //     if (i_reset) begin
            
    //     end else begin
    //         if (w_address_valid_op && w_prefetch) begin

    //         end
    //     end
    // end

endmodule
