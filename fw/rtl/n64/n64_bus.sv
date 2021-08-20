interface if_n64_bus ();

    localparam sc64::e_n64_id NUM_DEVICES = sc64::__ID_N64_END;

    sc64::e_n64_id id;
    logic request;
    logic ack;
    logic write;
    logic [31:0] address;
    logic [15:0] wdata;
    logic [15:0] rdata;

    logic device_ack [(NUM_DEVICES - 1):0];
    logic [15:0] device_rdata [(NUM_DEVICES - 1):0];

    always_comb begin
        ack = 1'b0;
        rdata = 16'd0;

        for (integer i = 0; i < NUM_DEVICES; i++) begin
            ack = ack | device_ack[i];
            rdata = rdata | device_rdata[i];
        end
    end

    modport n64 (
        output id,
        output request,
        input ack,
        output write,
        output address,
        output wdata,
        input rdata
    );

    genvar n;
    generate
        for (n = 0; n < NUM_DEVICES; n++) begin : at
            logic device_request;

            always_comb begin
                device_request = request && id == sc64::e_n64_id'(n);
            end

            modport device (
                input .request(device_request),
                output .ack(device_ack[n]),
                input .write(write),
                input .address(address),
                input .wdata(wdata),
                output .rdata(device_rdata[n])
            );
        end
    endgenerate

endinterface
