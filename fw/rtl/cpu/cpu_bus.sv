interface if_cpu_bus_out ();

    logic req;
    logic [3:0] wstrb;
    logic [31:0] address;
    logic [31:0] wdata;

endinterface

interface if_cpu_bus_in ();

    logic ack;
    logic [31:0] rdata;

endinterface

module cpu_bus_glue #(
    parameter [3:0] ADDRESS = 4'h0
) (
    if_system.sys system_if,
    if_cpu_bus_out cpu_bus_if,
    if_cpu_bus_in cpu_peripheral_if,

    output request,
    input [31:0] rdata
);
    reg ack;

    assign request = (cpu_bus_if.address[31:28] == ADDRESS) && cpu_bus_if.req;
    assign cpu_peripheral_if.ack = ack & request;
    assign cpu_peripheral_if.rdata = cpu_peripheral_if.ack ? rdata : 32'd0;

    always_ff @(posedge system_if.clk) begin
        ack <= 1'b0;

        if (!system_if.reset && request) begin
            ack <= 1'b1;
        end
    end

endmodule
