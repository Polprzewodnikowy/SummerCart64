interface if_flash ();

    logic request;
    logic ack;
    logic write;
    logic [31:0] address;
    logic [31:0] rdata;
    logic [31:0] wdata;

    modport cpu (
        output request,
        input ack,
        output write,
        output address,
        input rdata,
        output wdata
    );

    modport memory (
        input request,
        output ack,
        input write,
        input address,
        output rdata,
        input wdata
    );

endinterface


module cpu_flash (
    if_system.sys sys,
    if_cpu_bus bus,
    if_flash.cpu flash
);

    logic request;

    always_comb begin
        bus.ack = flash.ack;
        bus.rdata = flash.rdata;
        flash.request = bus.request || request;
        flash.write = &bus.wmask;
        flash.address = bus.address;
        flash.wdata = bus.wdata;
    end

    always_ff @(posedge sys.clk) begin
        if (sys.reset) begin
            request <= 1'b0;
        end else begin
            if (bus.request) begin
                request <= 1'b1;
            end
            if (flash.ack) begin
                request <= 1'b0;
            end
        end
    end

endmodule
