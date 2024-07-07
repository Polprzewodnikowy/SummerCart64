module memory_sdram_mock (
    input clk,
    input reset,

    mem_bus.memory mem_bus
);

    logic sdram_cs;
    logic sdram_ras;
    logic sdram_cas;
    logic sdram_we;
    logic [1:0] sdram_ba;
    logic [12:0] sdram_a;
    logic [1:0] sdram_dqm;
    logic [15:0] sdram_dq;

    memory_sdram memory_sdram_inst (
        .clk(clk),
        .reset(reset),

        .mem_bus(mem_bus),

        .sdram_cs(sdram_cs),
        .sdram_ras(sdram_ras),
        .sdram_cas(sdram_cas),
        .sdram_we(sdram_we),
        .sdram_ba(sdram_ba),
        .sdram_a(sdram_a),
        .sdram_dqm(sdram_dqm),
        .sdram_dq(sdram_dq)
    );

    logic [1:0] cas_delay;
    logic [15:0] data_from_sdram;
    logic [15:0] data_to_sdram;
    logic [15:0] sdram_dq_driven;

    assign sdram_dq = sdram_dq_driven;

    always_ff @(posedge clk) begin
        if (reset) begin
            cas_delay <= 2'b00;
            data_from_sdram <= 16'h0102;
            data_to_sdram <= 16'hFFFF;
        end else begin
            cas_delay <= {cas_delay[0], 1'b0};

            if ({sdram_cs, sdram_ras, sdram_cas, sdram_we} == 4'b0101) begin
                cas_delay[0] <= 1'b1;
            end

            if (cas_delay[1]) begin
                data_from_sdram <= data_from_sdram + 16'h0202;
            end

            if ({sdram_cs, sdram_ras, sdram_cas, sdram_we} == 4'b0100) begin
                if (!sdram_dqm[0]) data_to_sdram[7:0] <= sdram_dq[7:0];
                if (!sdram_dqm[1]) data_to_sdram[15:8] <= sdram_dq[15:8];
            end
        end
    end

    always_comb begin
        sdram_dq_driven = 16'hXXXX;
        if (cas_delay[1]) begin
            sdram_dq_driven = data_from_sdram;
        end
    end

endmodule
