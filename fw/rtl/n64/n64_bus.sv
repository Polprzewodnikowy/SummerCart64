interface if_n64_bus ();

    localparam sc64::e_n64_id NUM_DEVICES = sc64::__ID_N64_END;

    sc64::e_n64_id id;
    logic request;
    logic ack;
    logic write;
    logic [31:0] address;
    logic [15:0] wdata;
    logic [15:0] rdata;
    logic n64_active;
    logic [31:0] real_address;
    logic read_op;
    logic write_op;

    logic device_ack [(NUM_DEVICES - 1):0];
    logic [15:0] device_rdata [(NUM_DEVICES - 1):0];

    always_comb begin
        ack = 1'b0;
        rdata = 16'd0;

        for (integer i = 0; i < NUM_DEVICES; i++) begin
            ack = ack | device_ack[i];
            rdata = rdata | device_rdata[i];
        end

        if (id >= NUM_DEVICES) begin
            ack = request;
        end
    end

    modport n64 (
        output id,
        output request,
        input ack,
        output write,
        output address,
        output wdata,
        input rdata,
        output n64_active,
        output real_address,
        output read_op,
        output write_op
    );

    genvar n;
    generate
        for (n = 0; n < NUM_DEVICES; n++) begin : at
            logic device_request;
            logic device_n64_active;

            always_comb begin
                device_request = request && id == sc64::e_n64_id'(n);
                device_n64_active = n64_active && id == sc64::e_n64_id'(n);
            end

            modport device (
                input .request(device_request),
                output .ack(device_ack[n]),
                input .write(write),
                input .address(address),
                input .wdata(wdata),
                output .rdata(device_rdata[n]),
                input .n64_active(device_n64_active),
                input .real_address(real_address),
                input .read_op(read_op),
                input .write_op(write_op)
            );
        end
    endgenerate

endinterface
