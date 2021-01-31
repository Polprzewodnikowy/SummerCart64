module cart_led (
    input i_clk,
    input i_reset,

    input i_trigger,

    output o_led
);

    localparam COUNTER_LOAD_VALUE = 5'd31;

    reg [4:0] r_counter;

    assign o_led = r_counter > 5'd0;

    always @(posedge i_clk) begin
        if (i_reset) begin
            r_counter <= 5'd0;
        end else begin
            if (i_trigger) begin
                r_counter <= COUNTER_LOAD_VALUE;
            end else if (r_counter > 5'd0) begin
                r_counter <= r_counter - 1'd1;
            end
        end 
    end

endmodule
