interface n64_reg_bus ();

    logic read;
    logic write;
    logic [16:0] address;
    logic [15:0] rdata;
    logic [15:0] wdata;

    modport flashram (
        input read,
        input write,
        input address,
        output rdata,
        input wdata
    );

endinterface
