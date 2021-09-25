module cpu_gpio (
    if_system.sys sys,
    if_cpu_bus bus,

    input [7:0] gpio_i,
    output [7:0] gpio_o,
    output [7:0] gpio_oe
);

    logic [1:0][7:0] gpio_i_ff;
    logic [7:0] gpio_o_value;
    logic [7:0] gpio_oe_value;

    always_ff @(posedge sys.clk) begin
        bus.ack <= 1'b0;
        if (bus.request) begin
            bus.ack <= 1'b1;
        end
    end

    always_comb begin
        bus.rdata = 32'd0;
        if (bus.ack) begin
            bus.rdata = {8'd0, gpio_oe_value, gpio_i_ff[1], gpio_o_value};
        end
    end

    always_ff @(posedge sys.clk) begin
        gpio_i_ff <= {gpio_i_ff[0], gpio_i};
        gpio_o <= gpio_o_value;
        gpio_oe <= gpio_oe_value;

        if (sys.reset) begin
            gpio_o_value <= 8'd0;
            gpio_oe_value <= 8'd0;
        end else if (bus.request) begin
            if (bus.wmask[0]) gpio_o_value <= bus.wdata[7:0];
            if (bus.wmask[2]) gpio_oe_value <= bus.wdata[23:16];
        end
    end

endmodule
