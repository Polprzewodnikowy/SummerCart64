interface if_cpu_bus #(
    parameter NUM_DEVICES = 1
) (
    input clk,
    input reset
);

    logic request;
    logic ack;
    logic [3:0] wmask;
    logic [31:0] address;
    logic [31:0] wdata;
    logic [31:0] rdata;

    logic device_ack [(NUM_DEVICES - 1):0];
    logic [31:0] device_rdata [(NUM_DEVICES - 1):0];

    always_comb begin
        ack = 1'b0;
        rdata = 32'd0;

        for (integer i = 0; i < NUM_DEVICES; i++) begin
            ack = ack | device_ack[i];
            rdata = rdata | device_rdata[i];
        end
    end

    modport cpu (
        input clk,
        input reset,
        output request,
        input ack,
        output wmask,
        output address,
        output wdata,
        input rdata
    );

    genvar n;
    generate
        for (n = 0; n < NUM_DEVICES; n++) begin : at
            wire device_request = request && address[31:28] == n[3:0];

            modport device (
                input .clk(clk),
                input .reset(reset),
                input .request(device_request),
                output .ack(device_ack[n]),
                input .wmask(wmask),
                input .address(address),
                input .wdata(wdata),
                output .rdata(device_rdata[n])
            );
        end
    endgenerate

endinterface
