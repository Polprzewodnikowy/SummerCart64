#!/usr/bin/env python3
import struct

with open('btldr.bin', mode='rb') as code:
    with open('btldr.sv', mode='w') as rom:
        rom_formatted = ''
        index = 0

        for line in iter(lambda: code.read(4), ''):
            if (not line):
                break
            value = format(struct.unpack('<I', line)[0], '08x')
            rom_formatted += f'            {index}: rom_data = 32\'h{value};\n'
            index += 1


        rom.write(f'''module cpu_bootloader (
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
{rom_formatted}
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
''')
