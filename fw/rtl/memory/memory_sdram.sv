module memory_sdram (
    if_system sys,

    input request,
    output ack,
    input write,
    input [25:0] address,
    output [15:0] rdata,
    input [15:0] wdata,

    output sdram_cs,
    output sdram_ras,
    output sdram_cas,
    output sdram_we,
    output [1:0] sdram_ba,
    output [12:0] sdram_a,
    inout [15:0] sdram_dq
);

    parameter [2:0] CAS_LATENCY = 3'd2;

    parameter real T_INIT       = 100_000.0;
    parameter real T_RC         = 60.0;
    parameter real T_RP         = 15.0;
    parameter real T_RCD        = 15.0;
    // parameter real T_RAS        = 37.0;          //TODO: handle this timing
    // parameter real T_WR         = T_RAS - T_RCD; //TODO: handle this timing
    parameter real T_MRD        = 14.0;
    parameter real T_REF        = 7_800.0;

    localparam real T_CLK       = (1.0 / sc64::CLOCK_FREQUENCY) * 1_000_000_000.0;
    localparam int C_INIT       = int'((T_INIT + T_CLK - 1) / T_CLK);
    localparam int C_RC         = int'((T_RC + T_CLK - 1) / T_CLK);
    localparam int C_RP         = int'((T_RP + T_CLK - 1) / T_CLK);
    localparam int C_RCD        = int'((T_RCD + T_CLK - 1) / T_CLK);
    // localparam int C_RAS        = int'((T_RAS + T_CLK - 1) / T_CLK);
    // localparam int C_WR         = int'((T_WR + T_CLK - 1) / T_CLK);
    localparam int C_MRD        = int'((T_MRD + T_CLK - 1) / T_CLK);
    localparam int C_REF        = int'((T_REF + T_CLK - 1) / T_CLK);

    localparam INIT_PRECHARGE   = C_INIT;
    localparam INIT_REFRESH_1   = C_INIT + C_RP;
    localparam INIT_REFRESH_2   = C_INIT + C_RP + C_RC;
    localparam INIT_MODE_REG    = C_INIT + C_RP + (2 * C_RC);
    localparam INIT_DONE        = C_INIT + C_RP + (2 * C_RC) + C_MRD;

    typedef enum bit [3:0] {
        CMD_DESL    = 4'b1111,
        CMD_NOP     = 4'b0111,
        CMD_READ    = 4'b0101,
        CMD_WRITE   = 4'b0100,
        CMD_ACT     = 4'b0011,
        CMD_PRE     = 4'b0010,
        CMD_REF     = 4'b0001,
        CMD_MRS     = 4'b0000
    } e_sdram_cmd;

    e_sdram_cmd sdram_next_cmd;
    logic [15:0] sdram_dq_input;
    logic [15:0] sdram_dq_output;
    logic sdram_dq_output_enable;

    logic [14:0] current_active_bank_row;
    logic request_in_current_active_bank_row;

    always_ff @(posedge sys.clk) begin
        {sdram_cs, sdram_ras, sdram_cas, sdram_we} <= 4'(sdram_next_cmd);

        {sdram_ba, sdram_a} <= 15'd0;

        sdram_dq_input <= sdram_dq;
        sdram_dq_output <= wdata;
    
        sdram_dq_output_enable <= 1'b0;

        case (sdram_next_cmd)
            CMD_READ, CMD_WRITE: begin
                {sdram_ba, sdram_a} <= {address[25:24], 3'b000, address[10:1]};
                sdram_dq_output_enable <= sdram_next_cmd == CMD_WRITE;
            end
            CMD_ACT: begin
                {sdram_ba, sdram_a} <= address[25:11];
                current_active_bank_row <= address[25:11];
            end
            CMD_PRE: {sdram_ba, sdram_a} <= {2'b00, 2'b00, 1'b1, 10'd0};
            CMD_MRS: {sdram_ba, sdram_a} <= {2'b00, 1'b0, 1'b0, 2'b00, CAS_LATENCY, 1'b0, 3'b000};
        endcase
    end

    always_comb begin
        rdata = sdram_dq_input;
        sdram_dq = sdram_dq_output_enable ? sdram_dq_output : 16'hZZZZ;
        request_in_current_active_bank_row = address[25:11] == current_active_bank_row;
    end

    typedef enum bit [2:0] {
        S_INIT,
        S_IDLE,
        S_ACTIVATING,
        S_ACTIVE,
        S_BUSY,
        S_PRECHARGE,
        S_REFRESH
    } e_state;

    e_state state;
    e_state next_state;

    always_ff @(posedge sys.clk) begin
        if (sys.reset) begin
            state <= S_INIT;
        end else begin
            state <= next_state;
        end
    end

    logic [15:0] wait_counter;
    logic [15:0] refresh_counter;
    logic pending_refresh;

    always_ff @(posedge sys.clk) begin
        if (sys.reset || state != next_state) begin
            wait_counter <= 16'd0;
        end else begin
            wait_counter <= wait_counter + 1'd1;
        end

        if (sdram_next_cmd == CMD_REF) begin
            refresh_counter <= 16'd0;
        end else begin
            refresh_counter <= refresh_counter + 1'd1;
        end
    end

    always_comb begin
        pending_refresh = refresh_counter >= C_REF;
    end

    logic [(CAS_LATENCY):0] read_cmd_ack_delay;

    always_ff @(posedge sys.clk) begin
        ack <= 1'b0;
        read_cmd_ack_delay <= {sdram_next_cmd == CMD_READ, read_cmd_ack_delay[(CAS_LATENCY):1]};

        if (sdram_next_cmd == CMD_WRITE || read_cmd_ack_delay[0]) begin
            ack <= 1'b1;
        end
    end

    always_comb begin
        sdram_next_cmd = CMD_NOP;
        next_state = state;

        case (state)
            S_INIT: begin
                if (wait_counter < INIT_PRECHARGE) begin
                    sdram_next_cmd = CMD_DESL;
                end
                if (wait_counter == INIT_PRECHARGE) begin
                    sdram_next_cmd = CMD_PRE;
                end
                if (wait_counter == INIT_REFRESH_1 || wait_counter == INIT_REFRESH_2) begin
                    sdram_next_cmd = CMD_REF;
                end
                if (wait_counter == INIT_MODE_REG) begin
                    sdram_next_cmd = CMD_MRS;
                end
                if (wait_counter == INIT_DONE) begin
                    next_state = S_IDLE;
                end
            end

            S_IDLE: begin
                if (pending_refresh) begin
                    next_state = S_REFRESH;
                    sdram_next_cmd = CMD_REF;
                end else if (request) begin
                    next_state = S_ACTIVATING;
                    sdram_next_cmd = CMD_ACT;
                end
            end

            S_ACTIVATING: begin
                if (wait_counter == C_RCD) begin
                    next_state = S_ACTIVE;
                end
            end

            S_ACTIVE: begin
                if (pending_refresh) begin
                    next_state = S_PRECHARGE;
                    sdram_next_cmd = CMD_PRE;
                end else if (request) begin
                    if (request_in_current_active_bank_row) begin
                        next_state = S_BUSY;
                        sdram_next_cmd = write ? CMD_WRITE : CMD_READ;
                    end else begin
                        next_state = S_PRECHARGE;
                        sdram_next_cmd = CMD_PRE;
                    end
                end
            end

            S_BUSY: begin
                if (ack) begin
                    next_state <= S_ACTIVE;
                end
            end

            S_PRECHARGE: begin
                if (wait_counter == C_RP) begin
                    if (pending_refresh) begin
                        next_state = S_REFRESH;
                        sdram_next_cmd = CMD_REF;
                    end else begin
                        next_state = S_IDLE;
                    end
                end
            end

            S_REFRESH: begin
                if (wait_counter == C_RC) begin
                    next_state = S_IDLE;
                end
            end

            default: begin
                next_state = S_IDLE;
            end
        endcase
    end

endmodule
