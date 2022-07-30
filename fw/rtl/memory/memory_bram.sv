module memory_bram (
    input clk,

    n64_scb.bram n64_scb,

    mem_bus.memory mem_bus
);

    // Request logic

    logic [1:0] last_request;
    logic write;

    always_ff @(posedge clk) begin
        last_request <= {last_request[0], mem_bus.request};
    end

    always_ff @(posedge clk) begin
        mem_bus.ack <= mem_bus.request && last_request[0] && !last_request[1];
    end

    always_comb begin
        write = mem_bus.request && !last_request[0] && mem_bus.write;
    end


    // Address decoding

    logic buffer_selected;
    logic eeprom_selected;
    logic dd_selected;
    logic flashram_selected;

    always_comb begin
        buffer_selected = 1'b0;
        eeprom_selected = 1'b0;
        dd_selected = 1'b0;
        flashram_selected = 1'b0;
        if (mem_bus.address[25:24] == 2'b01 && mem_bus.address[23:14] == 10'd0) begin
            buffer_selected = mem_bus.address[13] == 1'b0;
            eeprom_selected = mem_bus.address[13:11] == 3'b100;
            dd_selected = mem_bus.address[13:8] == 6'b101000;
            flashram_selected = mem_bus.address[13:7] == 7'b1010010;
        end
    end


    // Buffer memory

    logic [15:0] buffer_bram [0:4095];
    logic [15:0] buffer_bram_rdata;

    always_ff @(posedge clk) begin
        if (write && buffer_selected) begin
            if (mem_bus.wmask[1]) buffer_bram[mem_bus.address[12:1]][15:8] <= mem_bus.wdata[15:8];
            if (mem_bus.wmask[0]) buffer_bram[mem_bus.address[12:1]][7:0] <= mem_bus.wdata[7:0];
        end
    end

    always_ff @(posedge clk) begin
        buffer_bram_rdata <= buffer_bram[mem_bus.address[12:1]];
    end


    // EEPROM memory

    logic [7:0] eeprom_bram_high [0:1023];
    logic [7:0] eeprom_bram_low [0:1023];
    logic [7:0] eeprom_bram_high_rdata;
    logic [7:0] eeprom_bram_low_rdata;
    logic [7:0] eeprom_bram_high_n64_rdata;
    logic [7:0] eeprom_bram_low_n64_rdata;
    logic [15:0] eeprom_bram_rdata;

    always_ff @(posedge clk) begin
        if (write && mem_bus.wmask[1] && eeprom_selected) begin
            eeprom_bram_high[mem_bus.address[10:1]] <= mem_bus.wdata[15:8];
        end
        if (n64_scb.eeprom_write && !n64_scb.eeprom_address[0]) begin
            eeprom_bram_high[n64_scb.eeprom_address[10:1]] <= n64_scb.eeprom_wdata;
        end
    end

    always_ff @(posedge clk) begin
        if (write && mem_bus.wmask[0] && eeprom_selected) begin
            eeprom_bram_low[mem_bus.address[10:1]] <= mem_bus.wdata[7:0];
        end
        if (n64_scb.eeprom_write && n64_scb.eeprom_address[0]) begin
            eeprom_bram_low[n64_scb.eeprom_address[10:1]] <= n64_scb.eeprom_wdata;
        end
    end

    always_ff @(posedge clk) begin
        eeprom_bram_high_rdata <= eeprom_bram_high[mem_bus.address[10:1]];
    end

    always_ff @(posedge clk) begin
        eeprom_bram_low_rdata <= eeprom_bram_low[mem_bus.address[10:1]];
    end

    always_ff @(posedge clk) begin
        eeprom_bram_high_n64_rdata <= eeprom_bram_high[n64_scb.eeprom_address[10:1]];
    end

    always_ff @(posedge clk) begin
        eeprom_bram_low_n64_rdata <= eeprom_bram_low[n64_scb.eeprom_address[10:1]];
    end

    always_comb begin
        eeprom_bram_rdata = {eeprom_bram_high_rdata, eeprom_bram_low_rdata};
        n64_scb.eeprom_rdata = n64_scb.eeprom_address[0] ? eeprom_bram_low_n64_rdata : eeprom_bram_high_n64_rdata;
    end


    // DD memory

    logic [15:0] dd_bram [0:127];
    logic [15:0] dd_bram_rdata;

    always_ff @(posedge clk) begin
        if (write && dd_selected) begin
            dd_bram[mem_bus.address[7:1]] <= mem_bus.wdata;
        end
        if (n64_scb.dd_write) begin
            dd_bram[n64_scb.dd_address] <= n64_scb.dd_wdata;
        end
    end

    always_ff @(posedge clk) begin
        dd_bram_rdata <= dd_bram[mem_bus.address[7:1]];
    end

    always_ff @(posedge clk) begin
        n64_scb.dd_rdata <= dd_bram[n64_scb.dd_address];
    end


    // FlashRAM memory

    logic [15:0] flashram_bram [0:63];
    logic [15:0] flashram_bram_rdata;

    always_ff @(posedge clk) begin
        if (n64_scb.flashram_write) begin
            flashram_bram[n64_scb.flashram_address] <= n64_scb.flashram_wdata;
        end
    end

    always_ff @(posedge clk) begin
        flashram_bram_rdata <= flashram_bram[mem_bus.address[6:1]];
    end


    // Output data mux

    always_ff @(posedge clk) begin
        mem_bus.rdata <= 16'd0;
        if (buffer_selected) mem_bus.rdata <= buffer_bram_rdata;
        if (eeprom_selected) mem_bus.rdata <= eeprom_bram_rdata;
        if (dd_selected) mem_bus.rdata <= dd_bram_rdata;
        if (flashram_selected) mem_bus.rdata <= flashram_bram_rdata;
    end

endmodule
