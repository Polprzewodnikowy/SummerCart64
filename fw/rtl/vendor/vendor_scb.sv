interface vendor_scb ();

    logic control_valid;
    logic [31:0] control_rdata;
    logic [31:0] control_wdata;
    logic [31:0] data_rdata;
    logic [31:0] data_wdata;

    modport controller (
        output control_valid,
        input control_rdata,
        output control_wdata,
        input data_rdata,
        output data_wdata
    );

    modport vendor (
        input control_valid,
        output control_rdata,
        input control_wdata,
        output data_rdata,
        input data_wdata
    );

endinterface
