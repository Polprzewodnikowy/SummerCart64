interface mem_bus ();

    logic request;
    logic ack;
    logic write;
    logic [1:0] wmask;
    logic [26:0] address;
    logic [15:0] rdata;
    logic [15:0] wdata;

    modport controller (
        output request,
        input ack,
        output write,
        output wmask,
        output address,
        input rdata,
        output wdata
    );

    modport memory (
        input request,
        output ack,
        input write,
        input wmask,
        input address,
        output rdata,
        input wdata
    );

endinterface
