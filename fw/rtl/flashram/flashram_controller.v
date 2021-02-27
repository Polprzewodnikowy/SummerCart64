`include "../constants.vh"

module flashram_controller (
    input i_clk,
    input i_reset,

    input [23:0] i_save_address,

    output o_flashram_read_mode,

    input i_request,
    input i_write,
    output o_busy,
    output reg o_ack,
    input [14:0] i_address,
    output reg [31:0] o_data,
    input [31:0] i_data,

    output reg o_mem_request,
    output reg o_mem_write,
    input i_mem_busy,
    input i_mem_ack,
    output reg [3:0] o_mem_bank,
    output reg [23:0] o_mem_address,
    output reg [31:0] o_mem_data,
    input [31:0] i_mem_data
);

    // State machine and command decoder

    localparam [31:0] FLASH_TYPE_ID         = 32'h1111_8001;
    localparam [31:0] FLASH_MODEL_ID        = 32'h00C2_001D;

    localparam [7:0] CMD_STATUS_MODE        = 8'hD2;
    localparam [7:0] CMD_READID_MODE        = 8'hE1;
    localparam [7:0] CMD_READ_MODE          = 8'hF0;
    localparam [7:0] CMD_ERASE_SECTOR       = 8'h4B;
    localparam [7:0] CMD_ERASE_CHIP         = 8'h3C;
    localparam [7:0] CMD_WRITE_MODE         = 8'hB4;
    localparam [7:0] CMD_ERASE_START        = 8'h78;
    localparam [7:0] CMD_WRITE_START        = 8'hA5;

    localparam STATE_STATUS                 = 0;
    localparam STATE_ID                     = 1;
    localparam STATE_READ                   = 2;
    localparam STATE_ERASE                  = 3;
    localparam STATE_WRITE                  = 4;
    localparam STATE_EXECUTE                = 5;

    localparam [1:0] EXECUTE_WRITE          = 2'b00;
    localparam [1:0] EXECUTE_ERASE_SECTOR   = 2'b10;
    localparam [1:0] EXECUTE_ERASE_CHIP     = 2'b11;

    reg [5:0] r_flashram_state;
    reg [9:0] r_sector_offset;
    reg [1:0] r_execute_mode;
    reg r_execute_start;
    reg r_execute_done;

    assign o_flashram_read_mode = r_flashram_state[STATE_READ];

    wire w_cmd_request = i_request && i_write && i_address[14] && !r_flashram_state[STATE_EXECUTE];
    wire [7:0] w_cmd_op = i_data[31:24];

    always @(posedge i_clk) begin
        r_execute_start <= 1'b0;

        if (i_reset || r_execute_done) begin
            r_flashram_state <= (1'b1 << STATE_STATUS);
        end else begin
            if (w_cmd_request) begin
                r_flashram_state <= 6'b000000;

                if (w_cmd_op == CMD_STATUS_MODE) begin
                    r_flashram_state[STATE_STATUS] <= 1'b1;
                end

                if (w_cmd_op == CMD_READID_MODE) begin
                    r_flashram_state[STATE_ID] <= 1'b1;
                end

                if (w_cmd_op == CMD_READ_MODE) begin
                    r_flashram_state[STATE_READ] <= 1'b1;
                end

                if (w_cmd_op == CMD_ERASE_SECTOR) begin
                    r_flashram_state[STATE_ERASE] <= 1'b1;
                    r_sector_offset <= {i_data[9:7], 7'd0};
                    r_execute_mode <= EXECUTE_ERASE_SECTOR;
                end

                if (w_cmd_op == CMD_ERASE_CHIP) begin
                    r_flashram_state[STATE_ERASE] <= 1'b1;
                    r_sector_offset <= 10'd0;
                    r_execute_mode <= EXECUTE_ERASE_CHIP;
                end

                if (w_cmd_op == CMD_WRITE_MODE) begin
                    r_flashram_state[STATE_WRITE] <= 1'b1;
                end

                if (w_cmd_op == CMD_ERASE_START) begin
                    if (r_flashram_state[STATE_ERASE]) begin
                        r_flashram_state[STATE_EXECUTE] <= 1'b1;
                        r_execute_start <= 1'b1;
                    end
                end

                if (w_cmd_op == CMD_WRITE_START) begin
                    r_flashram_state[STATE_EXECUTE] <= 1'b1;
                    r_sector_offset <= i_data[9:0];
                    r_execute_mode <= EXECUTE_WRITE;
                    r_execute_start <= 1'b1;
                end
            end
        end
    end


    // Status controller

    reg r_erase_busy;
    reg r_erase_done;

    reg r_write_busy;
    reg r_write_done;

    wire [3:0] w_status = {r_erase_done, r_write_done, r_erase_busy, r_write_busy};

    wire w_status_write_request = i_request && i_write && !i_address[14] && r_flashram_state[STATE_STATUS];

    always @(posedge i_clk) begin
        if (i_reset) begin
            r_erase_busy <= 1'b0;
            r_write_busy <= 1'b0;
            r_erase_done <= 1'b0;
            r_write_done <= 1'b0;
        end else begin
            if (w_status_write_request) begin
                r_erase_done <= r_erase_done & i_data[3];
                r_write_done <= r_write_done & i_data[1];
            end

            if (r_execute_start) begin
                if (r_execute_mode == EXECUTE_WRITE) begin
                    r_write_busy <= 1'b1;
                    r_write_done <= 1'b0;
                end else begin
                    r_erase_busy <= 1'b1;
                    r_erase_done <= 1'b0;
                end
            end

            if (r_execute_done) begin
                if (r_execute_mode == EXECUTE_WRITE) begin
                    r_write_busy <= 1'b0;
                    r_write_done <= 1'b1;
                end else begin
                    r_erase_busy <= 1'b0;
                    r_erase_done <= 1'b1;
                end
            end
        end
    end


    // Bus controller

    assign o_busy = 1'b0;

    wire [31:0] w_write_buffer_o_data;

    wire w_flashram_controller_read_request = i_request && !i_write && !o_busy && !r_flashram_state[STATE_READ];

    always @(posedge i_clk) begin
        o_ack <= 1'b0;

        if (w_flashram_controller_read_request) begin
            o_ack <= 1'b1;
            o_data <= {12'h000, w_status, 12'h000, w_status};

            if (r_flashram_state[STATE_ID]) begin
                o_data <= i_address[0] ? FLASH_MODEL_ID : FLASH_TYPE_ID;
            end

            if (r_flashram_state[STATE_WRITE]) begin
                o_data <= w_write_buffer_o_data;
            end
        end
    end


    // Page write buffer

    reg [4:0] r_write_buffer_address;

    wire [31:0] w_write_buffer_o_mem_data;

    wire w_write_buffer_request = i_request && i_write && !o_busy && !i_address[14] && r_flashram_state[STATE_WRITE];

    ram_flashram_write_buffer ram_flashram_write_buffer_inst (
        .clock(i_clk),

        .wren_a(w_write_buffer_request),
        .address_a(i_address[4:0]),
        .data_a(i_data),
        .q_a(w_write_buffer_o_data),

        .wren_b(i_mem_ack),
        .address_b(r_write_buffer_address),
        .data_b(i_mem_data & w_write_buffer_o_mem_data),
        .q_b(w_write_buffer_o_mem_data)
    );


    // Memory controller

    reg [15:0] r_items_left;

    wire w_execute_done = !r_execute_start && (r_items_left == 16'd0) && r_flashram_state[STATE_EXECUTE];
    wire w_read_phase_done = w_execute_done && !o_mem_write && i_mem_ack;
    wire w_write_phase_done = w_execute_done && o_mem_write;

    wire w_mem_request_successful = o_mem_request && !i_mem_busy;
    wire w_address_reset = r_execute_start || w_read_phase_done;
    wire w_write_buffer_address_increment = o_mem_write ? w_mem_request_successful : i_mem_ack;

    always @(posedge i_clk) begin
        r_execute_done <= w_write_phase_done;
    end

    always @(posedge i_clk) begin
        if (w_address_reset) begin
            case (r_execute_mode)
                EXECUTE_WRITE: r_items_left <= 16'h20;
                EXECUTE_ERASE_SECTOR: r_items_left <= 16'h1000;
                EXECUTE_ERASE_CHIP: r_items_left <= 16'h8000;
                default: r_items_left <= 16'd0;
            endcase
        end

        if (w_mem_request_successful) begin
            r_items_left <= r_items_left - 1'd1;
        end
    end

    always @(posedge i_clk) begin
        if (i_reset) begin
            o_mem_request <= 1'b0;
        end else begin
            if (r_items_left > 16'd0) begin
                o_mem_request <= 1'b1;
            end

            if (w_mem_request_successful) begin
                o_mem_request <= 1'b0;
            end
        end
    end

    always @(posedge i_clk) begin
        if (r_execute_start) begin
            if (r_execute_mode == EXECUTE_WRITE) begin
                o_mem_write <= 1'b0;
            end else begin
                o_mem_write <= 1'b1;
            end
        end

        if (w_read_phase_done) begin
            o_mem_write <= 1'b1;
        end
    end

    always @(posedge i_clk) begin
        o_mem_bank <= `BANK_SDRAM;
    end

    always @(posedge i_clk) begin
        if (w_address_reset) begin
            o_mem_address <= i_save_address + {9'd0, r_sector_offset, 5'd0};
            r_write_buffer_address <= 5'd0;
        end else begin
            if (w_mem_request_successful) begin
                o_mem_address <= o_mem_address + 1'd1;
            end

            if (w_write_buffer_address_increment) begin
                r_write_buffer_address <= r_write_buffer_address + 1'd1;
            end
        end
    end

    always @(*) begin
        o_mem_data = 32'hFFFF_FFFF;
        if (r_execute_mode == EXECUTE_WRITE) begin
            o_mem_data = w_write_buffer_o_mem_data;
        end
    end

endmodule
