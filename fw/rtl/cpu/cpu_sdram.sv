interface if_sdram ();

    logic request;
    logic ack;
    logic write;
    logic [31:0] address;
    logic [15:0] rdata;
    logic [15:0] wdata;

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


module cpu_sdram (
    if_system.sys sys,
    if_cpu_bus bus,
    if_sdram.cpu sdram
);

    logic request;
    logic current_word;
    logic [31:0] rdata;

    always_comb begin
        bus.rdata = 32'd0;
        if (bus.ack) begin
            bus.rdata = rdata;
        end

        sdram.write = current_word ? &bus.wmask[3:2] : &bus.wmask[1:0];
        sdram.address = {1'b0, bus.address[30:2], current_word, 1'b0};
        sdram.wdata = current_word ? bus.wdata[15:0] : bus.wdata[31:16];
    end

    always_ff @(posedge sys.clk) begin
        bus.ack <= 1'b0;

        if (sys.reset) begin
            sdram.request <= 1'b0;
        end else begin
            if (bus.request) begin
                sdram.request <= 1'b1;
                current_word <= 1'b0;
            end

            if (sdram.ack) begin
                if (!current_word) begin
                    current_word <= 1'b1;
                    rdata[31:16] <= sdram.rdata;
                end else begin
                    bus.ack <= 1'b1;
                    sdram.request <= 1'b0;
                    rdata[15:0] <= sdram.rdata;
                end
            end
        end
    end

endmodule
