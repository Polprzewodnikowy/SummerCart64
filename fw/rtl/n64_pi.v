module n64_pi (
    input i_clk,
    input i_reset,

    input [1:0] i_n64_pi_alel,
    input [1:0] i_n64_pi_aleh,
    input i_n64_pi_read,
    input i_n64_pi_write,
    input [15:0] i_n64_pi_ad,
    output reg [15:0] o_n64_pi_ad,
    output o_n64_pi_ad_mode,

    output reg o_read_rq,
    output reg o_write_rq,
    input i_ack,
    output reg [31:0] o_address,
    input [31:0] i_data,
    output reg [31:0] o_data,

    input i_address_valid
);

    reg r_last_n64_pi_alel;
    reg r_last_n64_pi_aleh;
    reg r_last_n64_pi_read;
    reg r_last_n64_pi_write;

    reg r_first_transfer;
    reg r_word_select;
    reg [31:0] r_data_i_buffer;
    reg [15:0] r_word_buffer;
    reg r_address_valid;
    reg r_address_valid_buffer;

    wire w_aleh_valid = (&i_n64_pi_alel) && (&i_n64_pi_aleh);
    wire w_alel_valid = (&i_n64_pi_alel) && (~|i_n64_pi_aleh);
    wire w_address_op = r_last_n64_pi_alel && !i_n64_pi_alel[0] && !i_n64_pi_aleh[0];
    wire w_read_op = r_last_n64_pi_read && !i_n64_pi_read;
    wire w_write_op = r_last_n64_pi_write && !i_n64_pi_write;
    wire w_bus_read_op = w_read_op && r_word_select;
    wire w_bus_write_op = w_write_op && !r_word_select;

    wire w_address_increment = w_bus_read_op || (w_bus_write_op && !r_first_transfer);

    assign o_n64_pi_ad_mode = !i_reset && !i_n64_pi_alel[0] && !i_n64_pi_aleh[0] && !i_n64_pi_read && !r_last_n64_pi_read && r_address_valid;

    always @(posedge i_clk) begin
        r_last_n64_pi_alel <= i_n64_pi_alel[0];
        r_last_n64_pi_aleh <= i_n64_pi_aleh[0];
        r_last_n64_pi_read <= i_n64_pi_read;
        r_last_n64_pi_write <= i_n64_pi_write;

        o_read_rq <= 1'b0;
        o_write_rq <= 1'b0;

        if (!i_reset) begin
            o_read_rq <= w_bus_read_op || w_address_op;
            o_write_rq <= w_bus_write_op;

            if (w_aleh_valid) begin
                o_address <= {i_n64_pi_ad, o_address[15:0]};
            end
            if (w_alel_valid) begin
                o_address <= {o_address[31:16], i_n64_pi_ad[15:1], 1'b0};
            end
            if (w_address_op) begin
                r_first_transfer <= 1'b1;
                r_word_select <= 1'b1;
            end
            if (w_read_op || w_write_op) begin
                r_word_select <= ~r_word_select;
                o_address <= {o_address[31:10], (o_address[9:0] + {w_address_increment, 2'b00})};
            end
            if (w_write_op && !r_word_select) begin
                r_first_transfer <= 1'b0;
            end
            if (w_read_op) begin
                {o_n64_pi_ad, r_word_buffer} <= r_word_select ? r_data_i_buffer : {r_word_buffer, 16'hXXXX};
            end
            if (w_write_op) begin
                o_data <= {o_data[15:0], i_n64_pi_ad};
            end
            if (w_bus_read_op) begin
                r_address_valid <= r_address_valid_buffer;
            end
            if (o_read_rq) begin
                r_address_valid_buffer <= i_address_valid;
            end
            if (i_ack) begin
                r_data_i_buffer <= i_data;
            end
        end
    end

endmodule
