module cpu_gpio (
    if_cpu_bus bus,

    input [7:0] gpio_i,
    output reg [7:0] gpio_o,
    output reg [7:0] gpio_oe
);

    reg [7:0] gpio_i_ff1, gpio_i_ff2;
    reg [7:0] gpio_o_value;
    reg [7:0] gpio_oe_value;

    always_comb begin
        bus.rdata = 32'd0;
        if (bus.ack) begin
            bus.rdata = {8'd0, gpio_oe_value, gpio_i_ff2, gpio_o_value};
        end
    end

    always_ff @(posedge bus.clk) begin
        {gpio_i_ff2, gpio_i_ff1} <= {gpio_i_ff1, gpio_i};
        gpio_o <= gpio_o_value;
        gpio_oe <= gpio_oe_value;

        bus.ack <= 1'b0;
        if (bus.request) begin
            bus.ack <= 1'b1;
        end

        if (bus.reset) begin
            gpio_o_value <= 8'd0;
            gpio_oe_value <= 8'd0;
        end else if (bus.request) begin
            if (bus.wmask[0]) gpio_o_value <= bus.wdata[7:0];
            if (bus.wmask[2]) gpio_oe_value <= bus.wdata[23:16];
        end
    end

endmodule
