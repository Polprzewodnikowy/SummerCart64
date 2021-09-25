module cpu_i2c (
    if_system.sys sys,
    if_cpu_bus bus,

    output i2c_scl,
    inout i2c_sda
);

    reg [1:0] state;
    reg mack;
    reg [8:0] trx_data;

    always_comb begin
        bus.rdata = 32'd0;
        if (bus.ack) begin
            case (bus.address[2])
                0: bus.rdata = {27'd0, |state, ~trx_data[0], mack, 2'b00};
                1: bus.rdata = {23'd0, trx_data[0], trx_data[8:1]};
                default: bus.rdata = 32'd0;
            endcase
        end
    end

    always_ff @(posedge sys.clk) begin
        bus.ack <= 1'b0;
        if (bus.request) begin
            bus.ack <= 1'b1;
        end

        if (sys.reset) begin
            mack <= 1'b0;
        end else if (bus.request && bus.wmask[0] && !bus.address[2]) begin
            mack <= bus.wdata[2];
        end
    end

    reg [5:0] clock_div;
    reg [3:0] clock_phase_gen;

    wire clock_tick = &clock_div;
    wire [3:0] clock_phase = {4{clock_tick}} & clock_phase_gen;

    always_ff @(posedge sys.clk) begin
        if (sys.reset) begin
            clock_div <= 6'd0;
        end else begin
            clock_div <= clock_div + 1'd1;
        end

        if (sys.reset || state == 2'd0) begin
            clock_phase_gen <= 4'b0001;
        end else if (clock_tick) begin
            clock_phase_gen <= {clock_phase_gen[2:0], clock_phase_gen[3]};
        end
    end

    reg [3:0] bit_counter;

    reg sda_i_ff1, sda_i_ff2;
    reg scl_o;
    reg sda_o;

    assign i2c_scl = scl_o ? 1'bZ : 1'b0;
    assign i2c_sda = sda_o ? 1'bZ : 1'b0;

    always_ff @(posedge sys.clk) begin
        {sda_i_ff2, sda_i_ff1} <= {sda_i_ff1, i2c_sda};

        if (sys.reset) begin
            state <= 2'd0;
            scl_o <= 1'b1;
            sda_o <= 1'b1;
        end else begin
            case (state)
                2'd0: begin
                    bit_counter <= 4'd0;

                    if (bus.request && bus.wmask[0]) begin
                        case (bus.address[2])
                            0: begin
                                if (bus.wdata[1]) state <= 2'd2;
                                if (bus.wdata[0]) state <= 2'd1;
                            end

                            1: begin
                                state <= 2'd3;
                                trx_data <= {bus.wdata[7:0], ~mack};
                            end
                        endcase
                    end
                end

                2'd1: begin
                    if (clock_phase[0]) begin
                        scl_o <= 1'b1;
                        sda_o <= 1'b1;
                    end

                    if (clock_phase[1]) begin
                        sda_o <= 1'b0;
                    end

                    if (clock_phase[3]) begin
                        state <= 2'd0;
                        scl_o <= 1'b0;
                    end
                end

                2'd2: begin
                    if (clock_phase[0]) begin
                        scl_o <= 1'b0;
                        sda_o <= 1'b0;
                    end

                    if (clock_phase[1]) begin
                        scl_o <= 1'b1;
                    end

                    if (clock_phase[3]) begin
                        state <= 2'd0;
                        sda_o <= 1'b1;
                    end
                end

                2'd3: begin
                    if (clock_phase[0]) begin
                        bit_counter <= bit_counter + 1'd1;
                        scl_o <= 1'b0;
                        sda_o <= trx_data[8];
                    end

                    if (clock_phase[1]) begin
                        scl_o <= 1'b1;
                    end

                    if (clock_phase[3]) begin
                        trx_data <= {trx_data[7:0], sda_i_ff2};
                        scl_o <= 1'b0;
                    end

                    if (bit_counter == 4'b1010) begin
                        state <= 2'd0;
                    end
                end

                default: begin
                    state <= 2'd0;
                    scl_o <= 1'b1;
                    sda_o <= 1'b1;
                end
            endcase
        end
    end

endmodule
