interface flash_scb ();

    logic erase_pending;
    logic erase_done;
    logic [7:0] erase_block;

    modport controller (
        output erase_pending,
        input erase_done,
        output erase_block
    );

    modport flash (
        input erase_pending,
        output erase_done,
        input erase_block
    );

endinterface


module flash_qspi (
    input clk,
    input reset,

    input start,
    input finish,
    output logic busy,
    output logic valid,
    input output_enable,
    input quad_enable,
    output logic [7:0] rdata,
    input [7:0] wdata,

    output logic flash_clk,
    output logic flash_cs,
    inout [3:0] flash_dq
);

    logic flash_dq_oe_s;
    logic flash_dq_oe_q;
    logic [3:0] flash_dq_out;

    assign flash_dq[0] = flash_dq_oe_s ? flash_dq_out[0] : 1'bZ;
    assign flash_dq[3:1] = flash_dq_oe_q ? flash_dq_out[3:1] : 3'bZZZ;

    logic ff_clk;
    logic ff_cs;
    logic ff_dq_oe_s;
    logic ff_dq_oe_q;
    logic [3:0] ff_dq_out;
    logic [3:0] ff_dq_in;

    always_ff @(posedge clk) begin
        flash_clk <= ff_clk;
        flash_cs <= ff_cs;
        flash_dq_oe_s <= ff_dq_oe_s;
        flash_dq_oe_q <= ff_dq_oe_q;
        flash_dq_out <= ff_dq_out;
        ff_dq_in <= flash_dq;
    end

    logic running;
    logic exit;
    logic valid_enable;
    logic quad_mode;
    logic [2:0] counter;
    logic [7:0] output_shift;
    logic [2:0] sample_s;
    logic [2:0] sample_q;
    logic [2:0] valid_ff;

    assign ff_dq_out = quad_mode ? output_shift[7:4] : {3'bXXX, output_shift[7]};

    always_ff @(posedge clk) begin
        sample_s <= {sample_s[1:0], 1'b0};
        sample_q <= {sample_q[1:0], 1'b0};
        valid_ff <= {valid_ff[1:0], 1'b0};
        if (reset) begin
            ff_clk <= 1'b0;
            ff_cs <= 1'b1;
            ff_dq_oe_s <= 1'b0;
            ff_dq_oe_q <= 1'b0;
            busy <= 1'b0;
            running <= 1'b0;
        end else begin
            if (running) begin
                ff_clk <= ~ff_clk;
                if (!ff_clk) begin
                    if (counter == 3'd0) begin
                        busy <= 1'b0;
                        valid_ff[0] <= valid_enable;
                    end
                    if (!quad_mode) begin
                        sample_s[0] <= 1'b1;
                    end else begin
                        sample_q[0] <= 1'b1;
                    end
                end else begin
                    counter <= counter - 1'd1;
                    if (counter == 3'd0) begin
                        running <= 1'b0;
                    end
                    if (!quad_mode) begin
                        output_shift <= {output_shift[6:0], 1'bX};
                    end else begin
                        output_shift <= {output_shift[3:0], 4'bXXXX};
                    end
                end
            end

            if (exit) begin
                ff_cs <= 1'b1;
                counter <= counter - 1'd1;
                if (counter == 3'd0) begin
                    busy <= 1'b0;
                    exit <= 1'b0;
                end
            end

            if (!busy) begin
                if (start) begin
                    ff_clk <= 1'b0;
                    ff_cs <= 1'b0;
                    ff_dq_oe_s <= !quad_enable || (quad_enable && output_enable);
                    ff_dq_oe_q <= quad_enable && output_enable;
                    busy <= 1'b1;
                    running <= 1'b1;
                    valid_enable <= !output_enable;
                    quad_mode <= quad_enable;
                    counter <= quad_enable ? 3'd1 : 3'd7;
                    output_shift <= wdata;
                end else if (finish) begin
                    busy <= 1'b1;
                    exit <= 1'b1;
                    counter <= wdata[2:0];
                end
            end
        end
    end

    always_ff @(posedge clk) begin
        valid <= 1'b0;
        if (sample_s[2]) begin
            rdata <= {rdata[6:0], ff_dq_in[1]};
        end
        if (sample_q[2]) begin
            rdata <= {rdata[3:0], ff_dq_in};
        end
        if (valid_ff[2]) begin
            valid <= 1'b1;
        end
    end

endmodule


module memory_flash (
    input clk,
    input reset,

    flash_scb.flash flash_scb,

    mem_bus.memory mem_bus,

    output flash_clk,
    output flash_cs,
    inout [3:0] flash_dq
);

    logic start;
    logic finish;
    logic busy;
    logic valid;
    logic output_enable;
    logic quad_enable;
    logic [7:0] rdata;
    logic [7:0] wdata;

    flash_qspi flash_qspi_inst (
        .clk(clk),
        .reset(reset),

        .start(start),
        .finish(finish),
        .busy(busy),
        .valid(valid),
        .output_enable(output_enable),
        .quad_enable(quad_enable),
        .rdata(rdata),
        .wdata(wdata),

        .flash_clk(flash_clk),
        .flash_cs(flash_cs),
        .flash_dq(flash_dq)
    );

    typedef enum bit [7:0] {
        FLASH_CMD_PAGE_PROGRAM      = 8'h02,
        FLASH_CMD_READ_STATUS_1     = 8'h05,
        FLASH_CMD_WRITE_ENABLE      = 8'h06,
        FLASH_CMD_BLOCK_ERASE_64KB  = 8'hD8,
        FLASH_CMD_FAST_READ_QUAD_IO = 8'hEB
    } e_flash_cmd;

    typedef enum {
        FLASH_STATUS_1_BUSY = 0
    } e_flash_status_1;

    typedef enum bit [3:0] {
        STATE_IDLE,
        STATE_WRITE_ENABLE,
        STATE_ERASE,
        STATE_PROGRAM_START,
        STATE_PROGRAM,
        STATE_PROGRAM_END,
        STATE_WAIT,
        STATE_READ_START,
        STATE_READ,
        STATE_READ_END
    } e_state;

    e_state state;
    logic [2:0] counter;
    logic valid_counter;
    logic [23:0] current_address;

    always_ff @(posedge clk) begin
        start <= 1'b0;
        finish <= 1'b0;
        flash_scb.erase_done <= 1'b0;
        mem_bus.ack <= 1'b0;

        if (reset) begin
            state <= STATE_IDLE;
        end else begin
            if (!busy && (start || finish)) begin
                counter <= counter + 1'd1;
            end

            case (state)
                STATE_IDLE: begin
                    output_enable <= 1'b1;
                    quad_enable <= 1'b0;
                    counter <= 3'd0;
                    if (flash_scb.erase_pending) begin
                        state <= STATE_WRITE_ENABLE;
                    end else if (mem_bus.request) begin
                        current_address <= {mem_bus.address[23:1], 1'b0};
                        if (mem_bus.write) begin
                            state <= STATE_WRITE_ENABLE;
                        end else begin
                            state <= STATE_READ_START;
                        end
                    end
                end

                STATE_WRITE_ENABLE: begin
                    case (counter)
                        3'd0: begin
                            start <= 1'b1;
                            wdata <= FLASH_CMD_WRITE_ENABLE;
                        end
                        3'd1: begin
                            finish <= 1'b1;
                            wdata <= 8'd4;
                            if (!busy) begin
                                counter <= 3'd0;
                                if (flash_scb.erase_pending) begin
                                    state <= STATE_ERASE;
                                end else begin
                                    state <= STATE_PROGRAM_START;
                                end
                            end
                        end
                    endcase
                end

                STATE_ERASE: begin
                    case (counter)
                        3'd0: begin
                            start <= 1'b1;
                            wdata <= FLASH_CMD_BLOCK_ERASE_64KB;
                        end
                        3'd1: begin
                            start <= 1'b1;
                            wdata <= flash_scb.erase_block;
                        end
                        3'd2: begin
                            start <= 1'b1;
                            wdata <= 8'd0;
                        end
                        3'd3: begin
                            start <= 1'b1;
                            wdata <= 8'd0;
                        end
                        3'd4: begin
                            finish <= 1'b1;
                            wdata <= 8'd4;
                            if (!busy) begin
                                flash_scb.erase_done <= 1'b1;
                                counter <= 3'd0;
                                state <= STATE_WAIT;
                            end
                        end
                    endcase
                end

                STATE_PROGRAM_START: begin
                    case (counter)
                        3'd0: begin
                            start <= 1'b1;
                            wdata <= FLASH_CMD_PAGE_PROGRAM;
                        end
                        3'd1: begin
                            start <= 1'b1;
                            wdata <= mem_bus.address[23:16];
                        end
                        3'd2: begin
                            start <= 1'b1;
                            wdata <= mem_bus.address[15:8];
                        end
                        3'd3: begin
                            start <= 1'b1;
                            wdata <= mem_bus.address[7:0];
                            if (!busy) begin
                                counter <= 3'd0;
                                state <= STATE_PROGRAM;
                            end
                        end
                    endcase
                end

                STATE_PROGRAM: begin
                    case (counter)
                        3'd0: begin
                            start <= 1'b1;
                            wdata <= mem_bus.wdata[15:8];
                        end
                        3'd1: begin
                            start <= 1'b1;
                            wdata <= mem_bus.wdata[7:0];
                            if (!busy) begin
                                mem_bus.ack <= 1'b1;
                                current_address <= current_address + 2'd2;
                            end
                        end
                        3'd2: begin
                            if (current_address[7:0] == 8'h00) begin
                                state <= STATE_PROGRAM_END;
                            end else if (flash_scb.erase_pending) begin
                                state <= STATE_PROGRAM_END;
                            end else if (mem_bus.request && !mem_bus.ack) begin
                                if (!mem_bus.write || (mem_bus.address[23:0] != current_address)) begin
                                    state <= STATE_PROGRAM_END;
                                end else begin
                                    counter <= 3'd0;
                                end
                            end
                        end
                    endcase
                end

                STATE_PROGRAM_END: begin
                    finish <= 1'b1;
                    wdata <= 8'd4;
                    if (!busy) begin
                        counter <= 3'd0;
                        state <= STATE_WAIT;
                    end
                end

                STATE_WAIT: begin
                    case (counter)
                        3'd0: begin
                            start <= 1'b1;
                            output_enable <= 1'b1;
                            wdata <= FLASH_CMD_READ_STATUS_1;
                        end
                        3'd1: begin
                            start <= 1'b1;
                            output_enable <= 1'b0;
                        end
                        3'd2: begin
                            finish <= 1'b1;
                            wdata <= 8'd0;
                        end
                        3'd3: begin
                            counter <= counter;
                        end
                    endcase
                    if (valid) begin
                        if (rdata[FLASH_STATUS_1_BUSY]) begin
                            counter <= 3'd0;
                        end else begin
                            state <= STATE_IDLE;
                        end
                    end
                end

                STATE_READ_START: begin
                    case (counter)
                        3'd0: begin
                            start <= 1'b1;
                            wdata <= FLASH_CMD_FAST_READ_QUAD_IO;
                        end
                        3'd1: begin
                            start <= 1'b1;
                            quad_enable <= 1'b1;
                            wdata <= mem_bus.address[23:16];
                        end
                        3'd2: begin
                            start <= 1'b1;
                            wdata <= mem_bus.address[15:8];
                        end
                        3'd3: begin
                            start <= 1'b1;
                            wdata <= mem_bus.address[7:0];
                        end
                        3'd4: begin
                            start <= 1'b1;
                            wdata <= 8'hFF;
                        end
                        3'd5: begin
                            start <= 1'b1;
                        end
                        3'd6: begin
                            start <= 1'b1;
                            if (!busy) begin
                                counter <= 3'd0;
                                valid_counter <= 1'b0;
                                state <= STATE_READ;
                            end
                        end
                    endcase
                end

                STATE_READ: begin
                    case (counter)
                        3'd0: begin
                            start <= 1'b1;
                            output_enable <= 1'b0;
                        end
                        3'd1: begin
                            start <= 1'b1;
                        end
                        3'd2: begin end
                        3'd3: begin
                            if (flash_scb.erase_pending) begin
                                state <= STATE_READ_END;
                            end else if (mem_bus.request && !mem_bus.ack) begin
                                if (mem_bus.write || (mem_bus.address[23:0] != current_address)) begin
                                    state <= STATE_READ_END;
                                end else begin
                                    start <= 1'b1;
                                    counter <= 3'd0;
                                end
                            end
                        end
                    endcase
                    if (valid) begin
                        valid_counter <= ~valid_counter;
                        if (valid_counter) begin
                            mem_bus.ack <= 1'b1;
                            counter <= counter + 1'd1;
                            current_address <= current_address + 2'd2;
                        end
                    end
                end

                STATE_READ_END: begin
                    finish <= 1'b1;
                    wdata <= 8'd0;
                    if (!busy) begin
                        state <= STATE_IDLE;
                    end
                end

                default: begin
                    state <= STATE_IDLE;
                end
            endcase
        end
    end

    always_ff @(posedge clk) begin
        if (valid) begin
            mem_bus.rdata <= {mem_bus.rdata[7:0], rdata};
        end
    end

endmodule
