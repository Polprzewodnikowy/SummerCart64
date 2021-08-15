module cpu_wrapper #(
    parameter [3:0] ENTRY_DEVICE = 4'h0
) (
    if_cpu_bus.cpu bus
);

    typedef enum bit {S_IDLE, S_WAITING} e_bus_state;

    e_bus_state state;

	wire mem_la_read;
	wire mem_la_write;

    always_ff @(posedge bus.clk) begin
        bus.request <= 1'b0;

        if (bus.reset) begin
            state <= S_IDLE;
        end else begin
            if (state == S_IDLE && (mem_la_read || mem_la_write)) begin
                state <= S_WAITING;
                bus.request <= 1'b1;
            end
            if (state == S_WAITING && bus.ack) begin
                state <= S_IDLE;
            end
        end
    end

    picorv32 #(
        .ENABLE_COUNTERS(0),
        .ENABLE_COUNTERS64(0),
        .TWO_STAGE_SHIFT(0),
        .CATCH_MISALIGN(0),
        .CATCH_ILLINSN(0),
        .PROGADDR_RESET({ENTRY_DEVICE, 28'h000_0000})
    ) cpu_inst (
        .clk(bus.clk),
        .resetn(~bus.reset),
        .mem_addr(bus.address),
        .mem_wdata(bus.wdata),
        .mem_wstrb(bus.wmask),
        .mem_ready(bus.ack),
        .mem_rdata(bus.rdata),
        .mem_la_read(mem_la_read),
        .mem_la_write(mem_la_write)
    );

endmodule
