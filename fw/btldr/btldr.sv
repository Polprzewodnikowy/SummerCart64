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
        case (cpu_bus_if.address[6:2])
            0: rdata = 32'hb0000737;
            1: rdata = 32'h00074783;
            2: rdata = 32'h0027f793;
            3: rdata = 32'hfe078ce3;
            4: rdata = 32'h03e00793;
            5: rdata = 32'h00f70423;
            6: rdata = 32'hb00006b7;
            7: rdata = 32'h00000793;
            8: rdata = 32'h00006637;
            9: rdata = 32'h0006c703;
            10: rdata = 32'h00177713;
            11: rdata = 32'h00070a63;
            12: rdata = 32'h0046c703;
            13: rdata = 32'h00178793;
            14: rdata = 32'h0ff77713;
            15: rdata = 32'hfee78fa3;
            16: rdata = 32'hfec792e3;
            17: rdata = 32'h10000097;
            18: rdata = 32'hfbc080e7;
            19: rdata = 32'hfd9ff06f;
            default: rdata = 32'h0000_0000;
        endcase
    end

endmodule
