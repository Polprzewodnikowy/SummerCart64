module vendor (
    input clk,
    input reset,

    vendor_scb.vendor vendor_scb
);

    logic start;
    logic busy;
    logic [1:0] length;
    logic [5:0] delay;

    logic request;
    logic write;
    logic ack;
    logic [7:0] address;
    logic [7:0] rdata;
    logic [7:0] wdata;

    logic [23:0] wdata_buffer;

    logic ufm_irq;

    always_comb begin
        start = vendor_scb.control_valid && vendor_scb.control_wdata[0] && !busy;
        vendor_scb.control_rdata = {
            16'd0,
            address,
            4'b0000,
            length,
            write,
            busy
        };
    end

    always_ff @(posedge clk) begin
        if (reset) begin
            busy <= 1'b0;
        end else begin
            if (start) begin
                busy <= 1'b1;
            end
            if (length == 2'd0 && ack) begin
                busy <= 1'b0;
            end
        end
    end

    always_ff @(posedge clk) begin
        if (start) begin
            length <= vendor_scb.control_wdata[3:2];
        end
        if (ack && length > 2'd0) begin
            length <= length - 1'd1;
        end
    end

    always_ff @(posedge clk) begin
        if (reset) begin
            delay <= 6'd0;
        end else begin
            if (start && vendor_scb.control_wdata[4]) begin
                delay <= 6'd35;
            end
            if (delay > 6'd0) begin
                delay <= delay - 1'd1;
            end
        end
    end

    always_ff @(posedge clk) begin
        if (reset) begin
            request <= 1'b0;
        end else begin
            if (start) begin
                request <= 1'b1;
            end
            if (busy && !request && delay == 6'd0) begin
                request <= 1'b1;
            end
            if (ack) begin
                request <= 1'b0;
            end
        end
    end

    always_ff @(posedge clk) begin
        if (start) begin
            write <= vendor_scb.control_wdata[1];
        end
    end

    always_ff @(posedge clk) begin
        if (start) begin
            address <= vendor_scb.control_wdata[15:8];
        end
    end

    always_ff @(posedge clk) begin
        if (ack) begin
            vendor_scb.data_rdata <= {vendor_scb.data_rdata[23:0], rdata};
        end
    end

    always_ff @(posedge clk) begin
        if (start) begin
            {wdata, wdata_buffer} <= vendor_scb.data_wdata;
        end
        if (ack) begin
            {wdata, wdata_buffer} <= {wdata_buffer, 8'h00};
        end
    end

    efb_lattice_generated efb_lattice_generated_inst (
        .wb_clk_i(clk),
        .wb_rst_i(reset),
        .wb_cyc_i(request),
        .wb_stb_i(request),
        .wb_we_i(write),
        .wb_adr_i(address),
        .wb_dat_i(wdata),
        .wb_dat_o(rdata),
        .wb_ack_o(ack),
        .wbc_ufm_irq(ufm_irq)
    );

endmodule
