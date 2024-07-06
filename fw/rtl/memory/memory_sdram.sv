module memory_sdram (
    input clk,
    input reset,

    mem_bus.memory mem_bus,

    output logic sdram_cs,
    output logic sdram_ras,
    output logic sdram_cas,
    output logic sdram_we,
    output logic [1:0] sdram_ba,
    output logic [12:0] sdram_a,
    output logic [1:0] sdram_dqm,
    inout [15:0] sdram_dq
);

    // in Hz
    localparam real FREQUENCY = 100_000_000.0;

    // in clocks
    localparam bit [2:0] CAS_LATENCY = 3'd2;

    // in nanoseconds
    localparam real T_INIT = 100_000.0;
    localparam real T_RC = 60.0;
    localparam real T_RP = 15.0;
    localparam real T_RCD = 15.0;
    localparam real T_MRD = 14.0;
    localparam real T_REF = 7_812.5;

    localparam real T_CLK = (1.0 / FREQUENCY) * 1_000_000_000.0;

    const bit [13:0] C_INIT = 14'(int'($ceil(T_INIT / T_CLK)));
    const bit [4:0] C_RC = 5'(int'($ceil(T_RC / T_CLK)));
    const bit [4:0] C_RP = 5'(int'($ceil(T_RP / T_CLK)));
    const bit [4:0] C_RCD = 5'(int'($ceil(T_RCD / T_CLK)));
    const bit [4:0] C_MRD = 5'(int'($ceil(T_MRD / T_CLK)));
    const bit [13:0] C_REF = 14'(int'($ceil(T_REF / T_CLK)));

    const bit [4:0] INIT_PRECHARGE = 5'd0;
    const bit [4:0] INIT_REFRESH_1 = INIT_PRECHARGE + C_RP;
    const bit [4:0] INIT_REFRESH_2 = INIT_REFRESH_1 + C_RC;
    const bit [4:0] INIT_MODE_REG = INIT_REFRESH_2 + C_RC;
    const bit [4:0] INIT_DONE = INIT_MODE_REG + C_MRD;

    // /CS, /RAS, /CAS, /WE
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

    always_ff @(posedge clk) begin
        {sdram_cs, sdram_ras, sdram_cas, sdram_we} <= 4'(sdram_next_cmd);
        {sdram_ba, sdram_a} <= 15'd0;
        sdram_dqm <= 2'b00;
        sdram_dq_input <= sdram_dq;
        sdram_dq_output <= mem_bus.wdata;
        sdram_dq_output_enable <= 1'b0;

        case (sdram_next_cmd)
            CMD_READ, CMD_WRITE: begin
                {sdram_ba, sdram_a} <= {mem_bus.address[25:24], 3'b000, mem_bus.address[10:1]};
                sdram_dqm <= (sdram_next_cmd == CMD_WRITE) ? (~mem_bus.wmask) : 2'b00;
                sdram_dq_output_enable <= (sdram_next_cmd == CMD_WRITE);
            end

            CMD_ACT: begin
                {sdram_ba, sdram_a} <= mem_bus.address[25:11];
                current_active_bank_row <= mem_bus.address[25:11];
            end

            CMD_PRE: begin
                {sdram_ba, sdram_a} <= {
                    2'b00,          // [BA1:BA0] Don't care
                    2'b00,          // [A12:A11] Don't care
                    1'b1,           // [A10] Precharge all banks
                    10'd0           // [A9:A0] Don't care
                };
            end

            CMD_MRS: begin
                {sdram_ba, sdram_a} <= {
                    2'b00,          // [BA1:BA0] Reserved = 0
                    3'b00,          // [A12:A10] Reserved = 0
                    1'b0,           // [A9] Write Burst Mode = Programmed Burst Length
                    2'b00,          // [A8:A7] Operating Mode = Standard Operation
                    CAS_LATENCY,    // [A6:A4] Latency Mode = 2
                    1'b0,           // [A3] Burst Type = Sequential
                    3'b000          // [A2:A0] Burst Length = 1
                };
            end

            default: begin end
        endcase
    end

    assign sdram_dq = sdram_dq_output_enable ? sdram_dq_output : 16'hZZZZ;

    always_comb begin
        mem_bus.rdata = sdram_dq_input;
        request_in_current_active_bank_row = mem_bus.address[25:11] == current_active_bank_row;
    end

    typedef enum bit [2:0] {
        S_POWERUP,
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

    always_ff @(posedge clk) begin
        if (reset) begin
            state <= S_POWERUP;
        end else begin
            state <= next_state;
        end
    end

    logic [13:0] refresh_counter;
    logic [4:0] wait_counter;
    logic powerup_done;
    logic pending_refresh;

    always_ff @(posedge clk) begin
        refresh_counter <= refresh_counter + 1'd1;

        if (refresh_counter == C_INIT) begin
            refresh_counter <= 14'd0;
            powerup_done <= 1'b1;
        end

        if (powerup_done && refresh_counter == C_REF - 14'd1) begin
            refresh_counter <= 14'd0;
            pending_refresh <= 1'b1;
        end

        if (sdram_next_cmd == CMD_REF) begin
            pending_refresh <= 1'b0;
        end

        if (reset) begin
            refresh_counter <= 14'd0;
            powerup_done <= 1'b0;
            pending_refresh <= 1'b0;
        end

        wait_counter <= wait_counter + 1'd1;

        if (state != next_state) begin
            wait_counter <= 5'd0;
        end
    end

    logic [(CAS_LATENCY):0] read_cmd_ack_delay;

    always_ff @(posedge clk) begin
        mem_bus.ack <= 1'b0;

        read_cmd_ack_delay <= {sdram_next_cmd == CMD_READ, read_cmd_ack_delay[(CAS_LATENCY):1]};

        if (sdram_next_cmd == CMD_WRITE || read_cmd_ack_delay[0]) begin
            mem_bus.ack <= 1'b1;
        end
    end

    always_comb begin
        sdram_next_cmd = CMD_NOP;
        next_state = state;

        case (state)
            S_POWERUP: begin
                sdram_next_cmd = CMD_DESL;
                if (powerup_done) begin
                    next_state = S_INIT;
                end
            end

            S_INIT: begin
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
                end else if (mem_bus.request) begin
                    next_state = S_ACTIVATING;
                    sdram_next_cmd = CMD_ACT;
                end
            end

            S_ACTIVATING: begin
                if (wait_counter == C_RCD - 5'd2) begin
                    next_state = S_ACTIVE;
                end
            end

            S_ACTIVE: begin
                if (pending_refresh) begin
                    next_state = S_PRECHARGE;
                    sdram_next_cmd = CMD_PRE;
                end else if (mem_bus.request) begin
                    if (request_in_current_active_bank_row) begin
                        next_state = S_BUSY;
                        sdram_next_cmd = mem_bus.write ? CMD_WRITE : CMD_READ;
                    end else begin
                        next_state = S_PRECHARGE;
                        sdram_next_cmd = CMD_PRE;
                    end
                end
            end

            S_BUSY: begin
                if (mem_bus.ack) begin
                    next_state = S_ACTIVE;
                end
            end

            S_PRECHARGE: begin
                if (wait_counter == C_RP - 5'd2) begin
                    next_state = S_IDLE;
                end
            end

            S_REFRESH: begin
                if (wait_counter == C_RC - 5'd2) begin
                    next_state = S_IDLE;
                end
            end

            default: begin
                next_state = S_IDLE;
            end
        endcase
    end

endmodule
