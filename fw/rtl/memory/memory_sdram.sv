module memory_sdram (
    if_system sys,
    if_n64_bus bus,

    output sdram_clk,
    output sdram_cs,
    output sdram_ras,
    output sdram_cas,
    output sdram_we,
    output [1:0] sdram_ba,
    output [12:0] sdram_a,
    inout [15:0] sdram_dq
);

    intel_gpio_ddro sdram_clk_ddro (
        .outclock(sys.sdram.sdram_clk),
        .din({1'b0, 1'b1}),
        .pad_out(sdram_clk)
    );

    parameter bit [2:0] CAS_LATENCY = 3'd2;

    localparam bit [12:0] MODE_REGISTER = {2'b00, 1'b0, 1'b0, 2'b00, CAS_LATENCY, 1'b0, 3'b000};

    typedef enum bit [3:0] {
        CMD_DESL   = 4'b1111;
        CMD_NOP    = 4'b0111;
        CMD_READ   = 4'b0101;
        CMD_WRITE  = 4'b0100;
        CMD_ACT    = 4'b0011;
        CMD_PRE    = 4'b0010;
        CMD_REF    = 4'b0001;
        CMD_MRS    = 4'b0000;
    } e_sdram_cmd;

    e_sdram_cmd sdram_next_cmd;
    logic [15:0] sdram_dq_input;
    logic [15:0] sdram_dq_output;
    logic sdram_dq_output_enable;

    always_ff @(posedge sys.clk) begin
        {o_sdram_cs, o_sdram_ras, o_sdram_cas, o_sdram_we} <= 4'(sdram_next_cmd);

        {sdram_ba, sdram_a} <= 15'd0;

        sdram_dq_input <= sdram_dq;
        sdram_dq_output <= bus.wdata;
    
        sdram_dq_output_enable <= 1'b0;

        case (sdram_next_cmd)
            CMD_READ, CMD_WRITE: begin
                {sdram_ba, sdram_a} <= {bus.address[25:24], 3'b000, bus.address[10:1]};
                sdram_dq_output_enable <= sdram_next_cmd == CMD_WRITE;
            end
            CMD_ACT: {sdram_ba, sdram_a} <= bus.address[25:11];
            CMD_PRE: {sdram_ba, sdram_a} <= {2'b00, 2'b00, 1'b1, 10'd0};
            CMD_MRS: {sdram_ba, sdram_a} <= MODE_REGISTER;
        endcase
    end

    always_comb begin
        sdram_dq = sdram_dq_output_enable ? sdram_dq_output : 16'hZZZZ;
    end

endmodule
