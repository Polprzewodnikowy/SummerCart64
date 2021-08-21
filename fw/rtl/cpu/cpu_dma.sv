interface if_dma ();

    logic request;
    logic ack;
    logic write;
    logic [31:0] address;
    logic [15:0] rdata;
    logic [15:0] wdata;

    modport cpu (
        output request,
        input ack,
        output write,
        output address,
        input rdata,
        output wdata
    );

    modport memory (
        input request,
        output ack,
        input write,
        input address,
        output rdata,
        input wdata
    );

endinterface
