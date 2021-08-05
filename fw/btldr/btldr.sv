module cpu_bootloader (
    if_system.sys system_if,
    if_cpu_bus_out cpu_bus_if,
    if_cpu_bus_in cpu_bootloader_if
);

    wire request;
    reg ack;
    reg [31:0] rom_data;
    reg [31:0] q;

    assign request = (cpu_bus_if.address[31:24] == 8'h01) && cpu_bus_if.req;
    assign cpu_bootloader_if.ack = ack & request;
    assign cpu_bootloader_if.rdata = cpu_bootloader_if.ack ? q : 32'd0;

    always_comb begin
        case (cpu_bus_if.address[9:2])
            0: rom_data = 32'h0600600b;
            1: rom_data = 32'h0180006f;
            2: rom_data = 32'h00000013;
            3: rom_data = 32'h00000013;
            4: rom_data = 32'h114000ef;
            5: rom_data = 32'h0400000b;
            6: rom_data = 32'h0000006f;
            7: rom_data = 32'h00000093;
            8: rom_data = 32'hff006117;
            9: rom_data = 32'hfd010113;
            10: rom_data = 32'h00000193;
            11: rom_data = 32'h00000213;
            12: rom_data = 32'h00000293;
            13: rom_data = 32'h00000313;
            14: rom_data = 32'h00000393;
            15: rom_data = 32'h00000413;
            16: rom_data = 32'h00000493;
            17: rom_data = 32'h00000513;
            18: rom_data = 32'h00000593;
            19: rom_data = 32'h00000613;
            20: rom_data = 32'h00000693;
            21: rom_data = 32'h00000713;
            22: rom_data = 32'h00000793;
            23: rom_data = 32'h00000813;
            24: rom_data = 32'h00000893;
            25: rom_data = 32'h00000913;
            26: rom_data = 32'h00000993;
            27: rom_data = 32'h00000a13;
            28: rom_data = 32'h00000a93;
            29: rom_data = 32'h00000b13;
            30: rom_data = 32'h00000b93;
            31: rom_data = 32'h00000c13;
            32: rom_data = 32'h00000c93;
            33: rom_data = 32'h00000d13;
            34: rom_data = 32'h00000d93;
            35: rom_data = 32'h00000e13;
            36: rom_data = 32'h00000e93;
            37: rom_data = 32'h00000f13;
            38: rom_data = 32'h00000f93;
            39: rom_data = 32'h00000517;
            40: rom_data = 32'h0a450513;
            41: rom_data = 32'h00000593;
            42: rom_data = 32'h00000613;
            43: rom_data = 32'h00c5dc63;
            44: rom_data = 32'h00052683;
            45: rom_data = 32'h00d5a023;
            46: rom_data = 32'h00450513;
            47: rom_data = 32'h00458593;
            48: rom_data = 32'hfec5c8e3;
            49: rom_data = 32'h00000513;
            50: rom_data = 32'h00400593;
            51: rom_data = 32'h00b55863;
            52: rom_data = 32'h00052023;
            53: rom_data = 32'h00450513;
            54: rom_data = 32'hfeb54ce3;
            55: rom_data = 32'h008000ef;
            56: rom_data = 32'h0000006f;
            57: rom_data = 32'hff010113;
            58: rom_data = 32'h00812623;
            59: rom_data = 32'h01010413;
            60: rom_data = 32'h00002783;
            61: rom_data = 32'h00178693;
            62: rom_data = 32'h00d02023;
            63: rom_data = 32'h00100737;
            64: rom_data = 32'hfff70713;
            65: rom_data = 32'hfee796e3;
            66: rom_data = 32'h800007b7;
            67: rom_data = 32'h0007a703;
            68: rom_data = 32'h800007b7;
            69: rom_data = 32'h00174713;
            70: rom_data = 32'h00e7a023;
            71: rom_data = 32'h00002023;
            72: rom_data = 32'hfd1ff06f;
            73: rom_data = 32'hff010113;
            74: rom_data = 32'h00812623;
            75: rom_data = 32'h01010413;
            76: rom_data = 32'h800007b7;
            77: rom_data = 32'h00100713;
            78: rom_data = 32'h00e7a023;
            79: rom_data = 32'h0000006f;

            default: rom_data = 32'h0000_0000;
        endcase
    end

    always_ff @(posedge system_if.clk) begin
        ack <= 1'b0;
        q <= rom_data;
        if (request) begin
            ack <= 1'b1;
        end
    end

endmodule
