// module cpu_uart (
//     if_system.sys system_if,
//     if_cpu_bus_out cpu_bus_if,
//     if_cpu_bus_in cpu_uart_if,

//     output ftdi_clk,
//     output ftdi_si,
//     input ftdi_so,
//     input ftdi_cts
// );

//     wire request;
//     wire [31:0] rdata;

//     cpu_bus_glue #(.ADDRESS(4'hD)) cpu_bus_glue_uart_inst (
//         .*,
//         .cpu_peripheral_if(cpu_uart_if),
//         .request(request),
//         .rdata(rdata)
//     );

//     reg rx_ready;    
//     wire tx_busy;
//     reg [7:0] rx_data;
//     reg [7:0] tx_data;

//     always_comb begin
//         case (cpu_bus_if.address[3:2])
//             0: rdata = {30'd0, ~tx_busy, ~rx_ready};
//             1: rdata = {24'd0, rx_data};
//             2: rdata = {24'd0, tx_data};
//             default: rdata = 32'd0;
//         endcase
//     end

//     wire rx_valid;
//     reg tx_valid;
//     wire [7:0] f_rx_data;

//     always_ff @(posedge system_if.clk) begin
//         tx_valid <= 1'b0;

//         if (rx_valid) begin
//             rx_ready <= 1'b0;
//             rx_data <= f_rx_data;
//         end

//         if (system_if.reset) begin
//             rx_ready <= 1'b1;
//         end else if (request) begin
//             if (cpu_bus_if.wstrb[0] && cpu_bus_if.address[3:2] == 2'd2 && !tx_busy) begin
//                 tx_valid <= 1'b1;
//                 tx_data <= cpu_bus_if.wdata[7:0];
//             end
//             if (cpu_bus_if.address[3:2] == 2'd1) begin
//                 rx_ready <= 1'b1;
//             end
//         end
//     end

//     usb_ftdi_fsi usb_ftdi_fsi_inst (
//         .i_clk(system_if.clk),
//         .i_reset(system_if.reset),

//         .o_ftdi_clk(ftdi_clk),
//         .o_ftdi_si(ftdi_si),
//         .i_ftdi_so(ftdi_so),
//         .i_ftdi_cts(ftdi_cts),

//         .i_rx_ready(rx_ready),
//         .o_rx_valid(rx_valid),
//         // .o_rx_channel(1'bX),
//         .o_rx_data(f_rx_data),

//         .o_tx_busy(tx_busy),
//         .i_tx_valid(tx_valid),
//         .i_tx_channel(1'b1),
//         .i_tx_data(tx_data)
//     );

// endmodule
