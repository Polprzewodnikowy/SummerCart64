module cpu_usb (
    if_system.sys system_if,
    if_cpu_bus_out cpu_bus_if,
    if_cpu_bus_in cpu_usb_if,

    output usb_clk,
    output usb_cs,
    input usb_miso,
    inout [3:0] usb_miosi
);

    wire request;
    wire [31:0] rdata;

    cpu_bus_glue #(.ADDRESS(4'hB)) cpu_bus_glue_usb_inst (
        .*,
        .cpu_peripheral_if(cpu_usb_if),
        .request(request),
        .rdata(rdata)
    );

    reg rx_ready;    
    wire tx_busy;
    reg [7:0] rx_data;
    reg [7:0] tx_data;

    always_comb begin
        case (cpu_bus_if.address[3:2])
            0: rdata = {30'd0, ~tx_busy, ~rx_ready};
            1: rdata = {24'd0, rx_data};
            2: rdata = {24'd0, tx_data};
            default: rdata = 32'd0;
        endcase
    end

    reg usb_request;
    reg usb_write;
    wire usb_busy;
    wire usb_ack;
    wire [7:0] usb_wdata;
    wire [7:0] usb_rdata;
    wire usb_rx_available;
    wire usb_tx_available;

    assign tx_busy = usb_busy || !usb_tx_available;
    assign usb_wdata = tx_data;

    // wire rx_valid;
    // reg tx_valid;
    // wire [7:0] f_rx_data;

    always_ff @(posedge system_if.clk) begin
        // tx_valid <= 1'b0;
        usb_request <= 1'b0;

        if (usb_ack) begin
            rx_ready <= 1'b0;
            rx_data <= usb_rdata;
        end

        if (system_if.reset) begin
            rx_ready <= 1'b1;
        end else if (request) begin
            if (cpu_bus_if.wstrb[0] && cpu_bus_if.address[3:2] == 2'd2 && !tx_busy) begin
                // tx_valid <= 1'b1;
                usb_request <= 1'b1;
                usb_write <= 1'b1;
                tx_data <= cpu_bus_if.wdata[7:0];
            end
            if (cpu_bus_if.address[3:2] == 2'd1) begin
                rx_ready <= 1'b1;
            end
        end else if (usb_rx_available && rx_ready) begin
            usb_request <= 1'b1;
            usb_write <= 1'b0;
        end
    end

    usb_ft1248 usb_ft1248_inst (
        .system_if(system_if),

        .usb_clk(usb_clk),
        .usb_cs(usb_cs),
        .usb_miso(usb_miso),
        .usb_miosi(usb_miosi),

        .request(usb_request),
        .write(usb_write),
        .busy(usb_busy),
        .ack(usb_ack),
        .wdata(usb_wdata),
        .rdata(usb_rdata),
        .rx_available(usb_rx_available),
        .tx_available(usb_tx_available)
    );

endmodule
