bus.rdata
module cpu_bootloader (
    if_system.sys sys,
    if_cpu_bus bus
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
            case (bus.address[6:2]){rom_formatted}
                default: bus.rdata = 32'd0;
            endcase
        end
    end

endmodule
