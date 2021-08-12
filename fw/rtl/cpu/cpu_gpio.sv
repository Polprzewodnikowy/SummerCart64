module cpu_gpio (
    if_system.sys system_if,
    if_cpu_bus_out cpu_bus_if,
    if_cpu_bus_in cpu_gpio_if,

    input [7:0] gpio_i,
    output reg [7:0] gpio_o,
    output reg [7:0] gpio_oe
);

    wire request;
    wire [31:0] rdata;

    cpu_bus_glue #(.ADDRESS(4'hE)) cpu_bus_glue_gpio_inst (
        .*,
        .cpu_peripheral_if(cpu_gpio_if),
        .request(request),
        .rdata(rdata)
    );

    reg [7:0] gpio_i_ff1, gpio_i_ff2;
    reg [7:0] gpio_o_value;
    reg [7:0] gpio_oe_value;

    assign rdata = {8'd0, gpio_oe_value, gpio_i_ff2, gpio_o_value};

    always_ff @(posedge system_if.clk) begin
        {gpio_i_ff2, gpio_i_ff1} <= {gpio_i_ff1, gpio_i};
        gpio_o <= gpio_o_value;
        gpio_oe <= gpio_oe_value;

        if (system_if.reset) begin
            gpio_o_value <= 8'd0;
            gpio_oe_value <= 8'd0;
        end else if (request) begin
            if (cpu_bus_if.wstrb[0]) gpio_o_value <= cpu_bus_if.wdata[7:0];
            if (cpu_bus_if.wstrb[2]) gpio_oe_value <= cpu_bus_if.wdata[23:16];
        end
    end

endmodule
