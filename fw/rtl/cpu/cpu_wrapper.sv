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

    logic trap;
    logic mem_valid;
    logic mem_instr;
    logic [31:0] mem_la_addr;
    logic [31:0] mem_la_wdata;
    logic [3:0] mem_la_wstrb;
    logic pcpi_valid;
    logic [31:0] pcpi_insn;
    logic [31:0] pcpi_rs1;
    logic [31:0] pcpi_rs2;
    logic [31:0] eoi;
    logic trace_valid;
    logic [35:0] trace_data;

    picorv32 #(
        .ENABLE_COUNTERS(0),
        .ENABLE_COUNTERS64(0),
        .TWO_STAGE_SHIFT(0),
        .TWO_CYCLE_COMPARE(1),
        .TWO_CYCLE_ALU(1),
        .CATCH_MISALIGN(0),
        .CATCH_ILLINSN(0),
        .PROGADDR_RESET(32'h0001_0000)
    ) cpu_inst (
        .clk(sys.clk),
        .resetn(~sys.reset),
        .mem_addr(bus.address),
        .mem_wdata(bus.wdata),
        .mem_wstrb(bus.wmask),
        .mem_ready(bus.ack),
        .mem_rdata(bus.rdata),
        .mem_la_read(mem_la_read),
        .mem_la_write(mem_la_write),

        .trap(trap),
        .mem_valid(mem_valid),
        .mem_instr(mem_instr),
        .mem_la_addr(mem_la_addr),
        .mem_la_wdata(mem_la_wdata),
        .mem_la_wstrb(mem_la_wstrb),
        .pcpi_valid(pcpi_valid),
        .pcpi_insn(pcpi_insn),
        .pcpi_rs1(pcpi_rs1),
        .pcpi_rs2(pcpi_rs2),
        .pcpi_wr(1'b0),
        .pcpi_rd(32'd0),
        .pcpi_wait(1'b0),
        .pcpi_ready(1'b0),
        .irq(32'd0),
        .eoi(eoi),
        .trace_valid(trace_valid),
        .trace_data(trace_data)
    );

endmodule
