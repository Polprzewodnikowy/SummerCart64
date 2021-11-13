module cpu_wrapper (
    if_system.sys sys,
    if_cpu_bus.cpu bus
);

    typedef enum bit [0:0] {
        S_IDLE,
        S_WAITING
    } e_bus_state;

    e_bus_state state;

    logic mem_la_read;
    logic mem_la_write;

    always_ff @(posedge sys.clk) begin
        bus.request <= 1'b0;
        if (sys.reset) begin
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
        .CATCH_MISALIGN(0),
        .CATCH_ILLINSN(0),
        .PROGADDR_RESET({4'(sc64::ID_CPU_FLASH), 28'h003_5800})
    ) cpu_inst (
        .clk(sys.clk),
        .resetn(~sys.reset),
        .mem_addr(bus.address),
        .mem_wdata(bus.wdata),
        .mem_wstrb(bus.wmask),
        .mem_ready(bus.ack),
        .mem_rdata(bus.rdata),
        .mem_la_read(mem_la_read),
        .mem_la_write(mem_la_write)
    );

endmodule
