module memory_sdram (
    input i_clk,
    input i_reset,

    output o_sdram_cs,
    output o_sdram_ras,
    output o_sdram_cas,
    output o_sdram_we,
    output [1:0] o_sdram_ba,
    output [12:0] o_sdram_a,
    inout [15:0] io_sdram_dq,

    input i_request,
    input i_write,
    output o_busy,
    output reg o_ack,
    input [24:0] i_address,
    output reg [31:0] o_data,
    input [31:0] i_data
);

    // SDRAM timings (in nanoseconds)

    parameter real CLK_FREQ     = 90_000_000.0;

    parameter [2:0] CAS_LATENCY = 3'd2;

    parameter real T_INIT       = 100_000.0;
    parameter real T_RC         = 60.0;
    parameter real T_RP         = 15.0;
    parameter real T_RCD        = 15.0;
    parameter real T_RAS        = 37.0;
    parameter real T_WR         = T_RAS - T_RCD;
    parameter real T_MRD        = 14.0;
    parameter real T_REF        = 7_800.0;

    localparam real T_CLK       = (1.0 / CLK_FREQ) * 1_000_000_000.0;
    localparam int C_INIT       = int'((T_INIT + T_CLK - 1) / T_CLK);
    localparam int C_RC         = int'((T_RC + T_CLK - 1) / T_CLK);
    localparam int C_RP         = int'((T_RP + T_CLK - 1) / T_CLK);
    localparam int C_RCD        = int'((T_RCD + T_CLK - 1) / T_CLK);
    localparam int C_RAS        = int'((T_RAS + T_CLK - 1) / T_CLK);
    localparam int C_WR         = int'((T_WR + T_CLK - 1) / T_CLK);
    localparam int C_MRD        = int'((T_MRD + T_CLK - 1) / T_CLK);
    localparam int C_REF        = int'((T_REF + T_CLK - 1) / T_CLK);

    localparam INIT_PRECHARGE   = C_INIT;
    localparam INIT_REFRESH_1   = C_INIT + C_RP;
    localparam INIT_REFRESH_2   = C_INIT + C_RP + C_RC;
    localparam INIT_MODE_REG    = C_INIT + C_RP + (2 * C_RC);
    localparam INIT_DONE        = C_INIT + C_RP + (2 * C_RC) + C_MRD;


    // SDRAM commands (CS, RAS, CAS, WE) and mode register

    localparam [3:0] CMD_DESL   = 4'b1111;
    localparam [3:0] CMD_NOP    = 4'b0111;
    localparam [3:0] CMD_READ   = 4'b0101;
    localparam [3:0] CMD_WRITE  = 4'b0100;
    localparam [3:0] CMD_ACT    = 4'b0011;
    localparam [3:0] CMD_PRE    = 4'b0010;
    localparam [3:0] CMD_REF    = 4'b0001;
    localparam [3:0] CMD_MRS    = 4'b0000;

    localparam MODE_REGISTER    = {2'b00, 1'b0, 1'b0, 2'b00, CAS_LATENCY, 1'b0, 3'b000};


    // Command signal decoder

    reg [3:0] r_sdram_cmd;

    assign {o_sdram_cs, o_sdram_ras, o_sdram_cas, o_sdram_we} = r_sdram_cmd;


    // Address signal decoder

    reg r_sdram_precharge;
    reg [1:0] r_sdram_bank;
    reg [12:0] r_sdram_row;
    reg [9:0] r_sdram_column;
    reg [14:0] r_active_bank_row;

    always @(*) begin
        case (r_sdram_cmd)
            CMD_READ, CMD_WRITE: o_sdram_a = {2'b00, r_sdram_precharge, r_sdram_column};
            CMD_ACT: o_sdram_a = r_sdram_row;
            CMD_PRE: o_sdram_a = {2'b00, r_sdram_precharge, 10'b0000000000};
            CMD_MRS: o_sdram_a = MODE_REGISTER;
            default: o_sdram_a = 13'b0000000000000;
        endcase
    end

    always @(*) begin
        case (r_sdram_cmd)
            CMD_READ, CMD_WRITE, CMD_ACT, CMD_PRE: o_sdram_ba = r_sdram_bank;
            default: o_sdram_ba = 2'b00;
        endcase
    end

    always @(posedge i_clk) begin
        if (i_request && !o_busy) begin
            {r_sdram_bank, r_sdram_row, r_sdram_column} <= i_address;
        end
        if (r_sdram_cmd == CMD_ACT) begin
            r_active_bank_row <= {r_sdram_bank, r_sdram_row};
        end
        if (r_sdram_cmd == CMD_READ || r_sdram_cmd == CMD_WRITE) begin
            {r_sdram_bank, r_sdram_row, r_sdram_column} <= {r_sdram_bank, r_sdram_row, r_sdram_column} + 1'b1;
        end
    end

    wire w_next_address_in_another_row = (&r_sdram_column);
    wire w_request_in_another_row = i_address[24:10] != r_active_bank_row;


    // Data signal decoder

    reg [31:0] r_sdram_data;
    reg r_current_write_word;

    always @(*) begin
        io_sdram_dq = 16'hZZZZ;
        if (r_sdram_cmd == CMD_WRITE) begin
            io_sdram_dq = r_current_write_word ? r_sdram_data[15:0] : r_sdram_data[31:16];
        end
    end

    always @(posedge i_clk) begin
        if (i_reset) begin
            r_current_write_word <= 1'b0;
        end else if (r_sdram_cmd == CMD_WRITE) begin
            r_current_write_word <= ~r_current_write_word;
        end
    end


    // Read latency timing

    reg [(CAS_LATENCY - 1):0] r_read_latency;
    reg r_current_read_word;

    always @(posedge i_clk) begin
        o_ack <= 1'b0;
        if (i_reset) begin
            r_read_latency <= {CAS_LATENCY{1'b0}};
            r_current_read_word <= 1'b0;
        end else begin
            r_read_latency <= {r_read_latency[(CAS_LATENCY - 2):0], r_sdram_cmd == CMD_READ};
            if (r_read_latency[CAS_LATENCY - 1]) begin
                o_data <= {o_data[15:0], io_sdram_dq};
                if (r_current_read_word) o_ack <= 1'b1;
                r_current_read_word <= ~r_current_read_word;
            end
        end
    end

    wire w_read_pending = |r_read_latency;


    // Init timing and logic

    reg [15:0] r_init_counter;

    always @(posedge i_clk) begin
        if (i_reset) begin
            r_init_counter <= 1'd0;
        end else if (r_init_counter < INIT_DONE) begin
            r_init_counter <= r_init_counter + 1'd1;
        end
    end

    wire w_init_hold = r_init_counter <= C_INIT - 1;
    wire w_init_precharge = r_init_counter == INIT_PRECHARGE;
    wire w_init_refresh_1 = r_init_counter == INIT_REFRESH_1;
    wire w_init_refresh_2 = r_init_counter == INIT_REFRESH_2;
    wire w_init_mode_reg = r_init_counter == INIT_MODE_REG;
    wire w_init_done = r_init_counter == INIT_DONE;


    // SDRAM controller FSM

    localparam [2:0] STATE_INIT         = 3'd0;
    localparam [2:0] STATE_IDLE         = 3'd1;
    localparam [2:0] STATE_ACTIVATING   = 3'd2;
    localparam [2:0] STATE_ACTIVE       = 3'd3;
    localparam [2:0] STATE_PRECHARGING  = 3'd4;
    localparam [2:0] STATE_REFRESHING   = 3'd5;

    reg [9:0] r_refresh_counter;
    reg [4:0] r_rcd_ras_rc_counter;
    reg [1:0] r_wr_counter;
    reg [2:0] r_rp_counter;

    wire w_refresh_pending = r_refresh_counter >= (C_REF - 1'd1);
    wire w_rcd_timing_met = r_rcd_ras_rc_counter >= (C_RCD - 1'd1);
    wire w_ras_timing_met = r_rcd_ras_rc_counter >= (C_RAS - 1'd1);
    wire w_rc_timing_met = r_rcd_ras_rc_counter >= (C_RC - 1'd1);
    wire w_wr_timing_met = r_wr_counter >= (C_WR - 1'd1);
    wire w_rp_timing_met = r_rp_counter >= (C_RP - 1'd1);

    reg [2:0] r_state;
    reg r_busy;
    reg r_cross_row_request;
    reg r_request_pending;
    reg r_write_pending;
    reg r_current_word;
    reg r_wr_wait;

    assign o_busy = i_request && (
        w_refresh_pending ||
        r_busy ||
        r_request_pending ||
        r_cross_row_request ||
        w_read_pending ||
        r_sdram_cmd == CMD_READ ||
        r_sdram_cmd == CMD_WRITE
    );

    always @(posedge i_clk) begin
        if (i_reset || w_init_hold) begin
            r_sdram_cmd <= CMD_DESL;
            r_state <= STATE_INIT;
            r_busy <= 1'b1;
            r_cross_row_request <= 1'b0;
            r_wr_wait <= 1'b0;
        end else begin
            r_sdram_cmd <= CMD_NOP;
            r_sdram_precharge <= 1'b0;

            if (r_refresh_counter < (C_REF - 1)) r_refresh_counter <= r_refresh_counter + 1'd1;
            if (r_rcd_ras_rc_counter < (C_RC - 1)) r_rcd_ras_rc_counter <= r_rcd_ras_rc_counter + 1'd1;
            if (r_wr_counter < (C_WR - 1)) r_wr_counter <= r_wr_counter + 1'd1;
            if (r_rp_counter < (C_RP - 1)) r_rp_counter <= r_rp_counter + 1'd1;

            case (r_state)
                STATE_INIT: begin
                    if (w_init_precharge) begin
                        r_sdram_cmd <= CMD_PRE;
                        r_sdram_precharge <= 1'b1;
                    end else if (w_init_refresh_1 || w_init_refresh_2) begin
                        r_sdram_cmd <= CMD_REF;
                        r_refresh_counter <= 1'd0;
                    end else if (w_init_mode_reg) begin
                        r_sdram_cmd <= CMD_MRS;
                    end else if (w_init_done) begin
                        r_state <= STATE_IDLE;
                        r_busy <= 1'b0;
                    end
                end

                STATE_IDLE: begin
                    if (w_refresh_pending) begin
                        r_sdram_cmd <= CMD_REF;
                        r_refresh_counter <= 1'd0;
                        r_rcd_ras_rc_counter <= 1'd0;
                        r_state <= STATE_REFRESHING;
                        r_busy <= 1'b1;
                    end else if (r_request_pending || r_cross_row_request) begin
                        r_sdram_cmd <= CMD_ACT;
                        r_rcd_ras_rc_counter <= 1'd0;
                        r_state <= STATE_ACTIVATING;
                        r_busy <= 1'b1;
                    end else if (i_request && !o_busy) begin
                        r_sdram_cmd <= CMD_ACT;
                        r_sdram_data <= i_data;
                        r_rcd_ras_rc_counter <= 1'd0;
                        r_state <= STATE_ACTIVATING;
                        r_request_pending <= 1'b1;
                        r_write_pending <= i_write;
                        r_current_word <= 1'b0;
                        r_busy <= 1'b1;
                    end
                end

                STATE_ACTIVATING: begin
                    if (w_rcd_timing_met) begin
                        r_sdram_cmd <= r_write_pending ? CMD_WRITE : CMD_READ;
                        if (r_write_pending) r_wr_counter <= 1'd0;
                        if (r_cross_row_request) begin
                            r_cross_row_request <= 1'b0;
                            r_busy <= 1'b0;
                        end else if (r_request_pending) begin
                            r_current_word <= 1'b1;
                        end
                        r_state <= STATE_ACTIVE;
                    end
                end

                STATE_ACTIVE: begin
                    if (r_wr_wait) begin
                        if (w_wr_timing_met) begin
                            r_rp_counter <= 1'd0;
                            r_state <= STATE_PRECHARGING;
                            r_wr_wait <= 1'b0;
                        end
                    end else if (r_request_pending && !(r_write_pending && w_read_pending)) begin
                        r_sdram_cmd <= r_write_pending ? CMD_WRITE : CMD_READ;
                        if (r_write_pending) r_wr_counter <= 1'd0;
                        r_current_word <= 1'b1;
                        if (r_current_word) begin
                            r_busy <= 1'b0;
                            r_request_pending <= 1'b0;
                        end
                    end else if (w_refresh_pending) begin
                        if (w_ras_timing_met && w_wr_timing_met) begin
                            r_sdram_cmd <= CMD_PRE;
                            r_sdram_precharge <= 1'b1;
                            r_rp_counter <= 1'd0;
                            r_state <= STATE_PRECHARGING;
                        end
                    end else if (i_request && !o_busy) begin
                        r_sdram_data <= i_data;
                        r_busy <= 1'b1;
                        r_write_pending <= i_write;
                        
                        if (w_request_in_another_row || (i_write && w_read_pending)) begin
                            r_request_pending <= 1'b1;
                            r_current_word <= 1'b0;
                            if (!(i_write && w_read_pending)) begin
                                if (!w_wr_timing_met) begin
                                    r_wr_wait <= 1'b1;
                                end else begin
                                    r_sdram_cmd <= CMD_PRE;
                                    r_sdram_precharge <= 1'b1;
                                    r_rp_counter <= 1'd0;
                                    r_state <= STATE_PRECHARGING;
                                end
                            end
                        end else begin
                            r_sdram_cmd <= i_write ? CMD_WRITE : CMD_READ;
                            if (i_write) r_wr_counter <= 1'd0;
                            r_current_word <= 1'b1;
                            if (w_next_address_in_another_row) begin
                                r_sdram_precharge <= 1'b1;
                                r_cross_row_request <= 1'b1;
                                if (!i_write) begin
                                    r_rp_counter <= 1'd0;
                                    r_state <= STATE_PRECHARGING;
                                end else begin
                                    r_wr_wait <= 1'b1;
                                end
                            end else begin
                                r_request_pending <= 1'b1;
                            end
                        end
                    end
                end

                STATE_PRECHARGING: begin
                    if (w_rc_timing_met && w_rp_timing_met) begin
                        r_state <= STATE_IDLE;
                        r_busy <= r_request_pending || r_cross_row_request;
                    end
                end

                STATE_REFRESHING: begin
                    if (w_rc_timing_met) begin
                        r_state <= STATE_IDLE;
                        r_busy <= 1'b0;
                    end
                end

                default: begin
                    r_state <= STATE_IDLE;
                    r_busy <= 1'b0;
                end
            endcase
        end
    end

endmodule
