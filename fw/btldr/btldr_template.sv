rdata
module cpu_bootloader (
    if_system.sys system_if,
    if_cpu_bus_out cpu_bus_if,
    if_cpu_bus_in cpu_bootloader_if
);

    wire request;
    wire [31:0] rdata;

    cpu_bus_glue #(.ADDRESS(4'hF)) cpu_bus_glue_bootloader_inst (
        .*,
        .cpu_peripheral_if(cpu_bootloader_if),
        .request(request),
        .rdata(rdata)
    );

    always_comb begin
        case (cpu_bus_if.address[6:2]){rom_formatted}
            default: rdata = 32'h0000_0000;
        endcase
    end

endmodule
