module n64_save_counter (
    input clk,
    input reset,

    n64_scb.save_counter n64_scb
);

    logic [15:0] counter;

    always_ff @(posedge clk) begin
        if (reset) begin
            counter <= 16'd0;
        end else begin
            if (n64_scb.eeprom_write || n64_scb.sram_done || n64_scb.flashram_done) begin
                counter <= counter + 1'd1;
            end
        end
    end

    always_ff @(posedge clk) begin
        n64_scb.save_count <= counter;
    end

endmodule
