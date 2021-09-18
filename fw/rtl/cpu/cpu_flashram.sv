interface if_flashram ();

    logic [4:0] address;
    logic [31:0] rdata;
    logic [9:0] sector;
    logic operation_pending;
    logic write_or_erase;
    logic sector_or_all;
    logic operation_done;

    modport cpu (
        output address,
        input rdata,
        input sector,
        input operation_pending,
        input write_or_erase,
        input sector_or_all,
        output operation_done
    );

    modport flashram (
        input address,
        output rdata,
        output sector,
        output operation_pending,
        output write_or_erase,
        output sector_or_all,
        input operation_done
    );

endinterface


module cpu_flashram (
    if_system.sys sys,
    if_cpu_bus bus,
    if_flashram.cpu flashram
);

    always_ff @(posedge sys.clk) begin
        bus.ack <= 1'b0;
        if (bus.request) begin
            bus.ack <= 1'b1;
        end
    end

    always_comb begin
        bus.rdata = 32'd0;
        if (bus.ack) begin
            bus.rdata = {
                14'd0,
                flashram.sector,
                4'd0,
                flashram.sector_or_all,
                flashram.write_or_erase,
                1'b0,
                flashram.operation_pending
            };
            if (bus.address[7]) begin
                bus.rdata = {flashram.rdata[7:0], flashram.rdata[15:8], flashram.rdata[23:16], flashram.rdata[31:24]};
            end
        end

        flashram.address = bus.address[6:2];
    end

    always_ff @(posedge sys.clk) begin
        flashram.operation_done <= 1'b0;

        if (bus.request) begin
            if (!bus.address[7] && bus.wmask[0]) begin
                flashram.operation_done <= bus.wdata[1];
            end
        end
    end

endmodule
