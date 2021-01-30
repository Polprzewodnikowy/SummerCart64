module n64_pi (
    input i_clk,
    input i_reset,

    input i_n64_reset,
    input i_n64_pi_alel,
    input i_n64_pi_aleh,
    input i_n64_pi_read,
    input i_n64_pi_write,
    inout reg [15:0] io_n64_pi_ad,

    output reg o_request,
    output reg o_write,
    input i_busy,
    input i_ack,
    output [3:0] o_bank,
    output reg [25:0] o_address,
    input [31:0] i_data,
    output reg [31:0] o_data,

    output o_sram_request,

    input i_ddipl_enable,
    input i_sram_enable,
    input i_flashram_enable,
    input i_sd_enable,
    input i_eeprom_enable,

    input [23:0] i_ddipl_address,
    input [23:0] i_sram_address
);

    // Parameters

    parameter bit PREFETCH_DISABLE = 1'b0;


    // Input synchronization

    reg r_reset_ff1, r_reset_ff2;
    reg r_alel_ff1, r_alel_ff2;
    reg r_aleh_ff1, r_aleh_ff2;
    reg r_read_ff1, r_read_ff2;
    reg r_write_ff1, r_write_ff2;

    always @(posedge i_clk) begin
        {r_reset_ff2, r_reset_ff1} <= {r_reset_ff1, i_n64_reset};
        {r_alel_ff2, r_alel_ff1} <= {r_alel_ff1, i_n64_pi_alel};
        {r_aleh_ff2, r_aleh_ff1} <= {r_aleh_ff1, i_n64_pi_aleh};
        {r_read_ff2, r_read_ff1} <= {r_read_ff1, i_n64_pi_read};
        {r_write_ff2, r_write_ff1} <= {r_write_ff1, i_n64_pi_write};
    end


    // PI event signals generator

    wire [1:0] w_pi_mode = {r_aleh_ff2, r_alel_ff2};
    reg [1:0] r_last_pi_mode;
    reg r_last_read;
    reg r_last_write;

    always @(posedge i_clk) begin
        r_last_pi_mode <= w_pi_mode;
        r_last_read <= r_read_ff2;
        r_last_write <= r_write_ff2;
    end

    localparam [1:0] PI_MODE_IDLE  = 2'b10;
    localparam [1:0] PI_MODE_HIGH  = 2'b11;
    localparam [1:0] PI_MODE_LOW   = 2'b01;
    localparam [1:0] PI_MODE_VALID = 2'b00;

    wire w_address_high_op = r_reset_ff2 && (r_last_pi_mode != PI_MODE_HIGH) && (w_pi_mode == PI_MODE_HIGH);
    wire w_address_low_op = r_reset_ff2 && (r_last_pi_mode != PI_MODE_LOW) && (w_pi_mode == PI_MODE_LOW);
    wire w_address_valid_op = r_reset_ff2 && (r_last_pi_mode != PI_MODE_VALID) && (w_pi_mode == PI_MODE_VALID);
    wire w_read_op = r_reset_ff2 && (w_pi_mode == PI_MODE_VALID) && r_last_read && !r_read_ff2;
    wire w_write_op = r_reset_ff2 && (w_pi_mode == PI_MODE_VALID) && r_last_write && !r_write_ff2;


    // Bus address register

    reg [31:0] r_pi_address;

    always @(posedge i_clk) begin
        if (w_address_high_op) r_pi_address[31:16] <= io_n64_pi_ad;
        if (w_address_low_op) r_pi_address[15:0] <= {io_n64_pi_ad[15:1], 1'b0};
    end


    // Bank decoder, address translator and prefetch signal

    wire [25:0] w_translated_address;
    wire w_bank_prefetch;
    wire w_prefetch = !PREFETCH_DISABLE && w_bank_prefetch;

    n64_bank_decoder n64_bank_decoder_inst (
        .i_address(r_pi_address),
        .o_translated_address(w_translated_address),
        .o_bank(o_bank),
        .o_bank_prefetch(w_bank_prefetch),
        .o_sram_request(o_sram_request),
        .i_ddipl_enable(i_ddipl_enable),
        .i_sram_enable(i_sram_enable),
        .i_flashram_enable(i_flashram_enable),
        .i_sd_enable(i_sd_enable),
        .i_eeprom_enable(i_eeprom_enable),
        .i_ddipl_address(i_ddipl_address),
        .i_sram_address(i_sram_address)
    );


    // Read/write current word logic

    reg r_word_counter;

    always @(posedge i_clk) begin
        if (w_address_valid_op) r_word_counter <= 1'b0;
        if (w_read_op || w_write_op) r_word_counter <= ~r_word_counter;
    end


    // N64 PI output data logic

    reg [31:0] r_pi_output_data;

    always @(*) begin
        io_n64_pi_ad = 16'hZZZZ;
        if (r_reset_ff2 && !r_read_ff2 && o_bank != 4'd0) begin
            io_n64_pi_ad = r_word_counter ? r_pi_output_data[31:16] : r_pi_output_data[15:0];
        end
    end


    // Bus event signals generator

    wire w_bus_read_op = w_read_op && !r_word_counter;
    wire w_bus_write_op = w_write_op && r_word_counter;


    // Read buffer logic

    reg [31:0] r_pi_read_buffer;

    always @(posedge i_clk) begin
        if (i_ack) begin
            if (w_prefetch) r_pi_read_buffer <= i_data;
            else r_pi_output_data <= i_data;
        end
        if (w_prefetch && w_bus_read_op) r_pi_output_data <= r_pi_read_buffer;
    end


    // Write data logic

    reg [15:0] r_pi_write_buffer;

    always @(posedge i_clk) begin
        if (w_write_op) begin
            if (!r_word_counter) begin
                r_pi_write_buffer <= io_n64_pi_ad;
            end else begin
                o_data <= {r_pi_write_buffer, io_n64_pi_ad};
            end
        end
    end


    // Bus request logic

    wire w_bus_request_op = !o_request && ((w_address_valid_op && w_prefetch) || w_bus_read_op || w_bus_write_op);

    always @(posedge i_clk) begin
        if (i_reset) begin
            o_request <= 1'b0;
            o_write <= 1'b0;
        end else begin
            if (w_bus_request_op) begin
                o_request <= 1'b1;
                o_write <= w_bus_write_op;
            end
            if (o_request && !i_busy) begin
                o_request <= 1'b0;
                o_write <= 1'b0;
            end
        end
    end


    // Address increment logic

    reg r_first_transfer;

    wire w_address_increment_op = (
        (w_bus_read_op && (!r_first_transfer || w_prefetch)) ||
        (w_bus_write_op && !r_first_transfer)
    );
    wire w_first_transfer_clear_op = w_bus_read_op || w_bus_write_op;

    always @(posedge i_clk) begin
        if (w_address_valid_op) begin
            o_address <= w_translated_address;
            r_first_transfer <= 1'b1;
        end
        if (w_first_transfer_clear_op) r_first_transfer <= 1'b0;
        if (w_address_increment_op) o_address[15:2] <= o_address[15:2] + 1'b1;
    end

endmodule
