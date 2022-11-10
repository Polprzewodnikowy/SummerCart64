module sd_clk (
    input clk,
    input reset,

    sd_scb.clk sd_scb,

    output logic sd_clk_rising,
    output logic sd_clk_falling,

    output logic sd_clk
);

    logic [7:0] clock_divider;

    always_ff @(posedge clk) begin
        if (!sd_scb.clock_stop) begin
            clock_divider <= clock_divider + 1'd1;
        end
    end

    logic selected_clock;

    always_comb begin
        selected_clock = 1'b0;
        case (sd_scb.clock_mode)
            2'd0: selected_clock = 1'b0;
            2'd1: selected_clock = clock_divider[7];
            2'd2: selected_clock = clock_divider[1];
            2'd3: selected_clock = clock_divider[0];
        endcase
    end

    logic last_selected_clock;

    always_ff @(posedge clk) begin
        last_selected_clock <= selected_clock;
    end

    always_comb begin
        sd_clk_rising = !last_selected_clock && selected_clock;
        sd_clk_falling = last_selected_clock && !selected_clock;
    end

    always_ff @(posedge clk) begin
        sd_clk <= last_selected_clock;
    end

endmodule
