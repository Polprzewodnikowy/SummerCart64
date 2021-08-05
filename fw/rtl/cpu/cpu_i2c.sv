module cpu_i2c (
    if_system.sys system_if,
    if_cpu_bus_out cpu_bus_if,
    if_cpu_bus_in cpu_i2c_if,

    inout scl,
    inout sda
);

    // wire request;
    // reg ack;
    // // reg led_value;

    // assign request = (cpu_bus_if.address[31:3] == (32'h8001_0000 >> 3)) && cpu_bus_if.req;
    // assign cpu_led_if.ack = ack & request;
    // assign cpu_led_if.rdata = cpu_led_if.ack ? {32'd0} : 32'd0;

    // always_ff @(posedge system_if.clk) begin
    //     ack <= 1'b0;
    //     // led <= led_value;

    //     if (system_if.reset) begin
    //         // led_value <= 1'b0;
    //     end else if (request) begin
    //         ack <= 1'b1;
    //         if (cpu_bus_if.wstrb[0]) begin
    //             // led_value <= cpu_bus_if.wdata[0];
    //         end
    //     end
    // end

endmodule
