module cpu_i2c (
    if_system.sys system_if,
    if_cpu_bus_out cpu_bus_if,
    if_cpu_bus_in cpu_i2c_if,

    inout scl,
    inout sda
);

    wire request;
    wire [31:0] rdata;

    cpu_bus_glue #(.ADDRESS(4'hC)) cpu_bus_glue_i2c_inst (
        .*,
        .cpu_peripheral_if(cpu_i2c_if),
        .request(request),
        .rdata(rdata)
    );

    reg [1:0] state;
    reg mack;
    reg [8:0] trx_data;

    always_comb begin
        case (cpu_bus_if.address[2])
            0: rdata = {27'd0, |state, ~trx_data[0], mack, 2'b00};
            1: rdata = {23'd0, trx_data[0], trx_data[8:1]};
            default: rdata = 32'd0;
        endcase
    end

    always_ff @(posedge system_if.clk) begin
        if (system_if.reset) begin
            mack <= 1'b0;
        end else if (request && cpu_bus_if.wstrb[0] && !cpu_bus_if.address[2]) begin
            mack <= cpu_bus_if.wdata[2];
        end
    end

    reg [5:0] clock_div;
    reg [3:0] clock_phase_gen;

    wire clock_tick = &clock_div;
    wire [3:0] clock_phase = {4{clock_tick}} & clock_phase_gen;

    always_ff @(posedge system_if.clk) begin
        if (system_if.reset) begin
            clock_div <= 6'd0;
        end else begin
            clock_div <= clock_div + 1'd1;
        end

        if (system_if.reset || state == 2'd0) begin
            clock_phase_gen <= 4'b0001;
        end else if (clock_tick) begin
            clock_phase_gen <= {clock_phase_gen[2:0], clock_phase_gen[3]};
        end
    end

    reg [3:0] bit_counter;

    reg sda_i_ff1, sda_i_ff2;
    reg scl_o;
    reg sda_o;

    assign scl = scl_o ? 1'bZ : 1'b0;
    assign sda = sda_o ? 1'bZ : 1'b0;

    always_ff @(posedge system_if.clk) begin
        {sda_i_ff2, sda_i_ff1} <= {sda_i_ff1, sda};

        if (system_if.reset) begin
            state <= 2'd0;
            scl_o <= 1'b1;
            sda_o <= 1'b1;
        end else begin
            case (state)
                2'd0: begin
                    bit_counter <= 4'd0;

                    if (request && cpu_bus_if.wstrb[0]) begin
                        case (cpu_bus_if.address[2])
                            0: begin
                                if (cpu_bus_if.wdata[1]) state <= 2'd2;
                                if (cpu_bus_if.wdata[0]) state <= 2'd1;
                            end

                            1: begin
                                state <= 2'd3;
                                trx_data <= {cpu_bus_if.wdata[7:0], ~mack};
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
