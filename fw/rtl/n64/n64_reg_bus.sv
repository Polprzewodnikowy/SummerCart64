interface n64_reg_bus ();

    logic dd_select;
    logic flashram_select;
    logic cfg_select;

    logic read;
    logic write;
    logic [16:0] address;
    logic [15:0] rdata;
    logic [15:0] wdata;

    logic [15:0] dd_rdata;
    logic [15:0] flashram_rdata;
    logic [15:0] cfg_rdata;

    modport controller (
        output dd_select,
        output flashram_select,
        output cfg_select,

        output read,
        output write,
        output address,
        input rdata,
        output wdata
    );

    always_comb begin
        rdata = 16'd0;
        if (dd_select) begin
            rdata = dd_rdata;
        end
        if (flashram_select) begin
            rdata = flashram_rdata;
        end
        if (cfg_select) begin
            rdata = cfg_rdata;
        end
    end

    modport dd (
        input .read(read && dd_select),
        input .write(write && dd_select),
        input address,
        output .rdata(dd_rdata),
        input wdata
    );

    modport flashram (
        input .read(read && flashram_select),
        input .write(write && flashram_select),
        input address,
        output .rdata(flashram_rdata),
        input wdata
    );

    modport cfg (
        input .read(read && cfg_select),
        input .write(write && cfg_select),
        input address,
        output .rdata(cfg_rdata),
        input wdata
    );

endinterface
