interface if_cpu_bus_out ();

    logic req;
    logic [3:0] wstrb;
    logic [31:0] address;
    logic [31:0] wdata;

endinterface

interface if_cpu_bus_in ();

    logic ack;
    logic [31:0] rdata;

endinterface
