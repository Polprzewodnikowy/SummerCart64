module n64_flashram (
    input clk,
    input reset,

    n64_reg_bus.flashram reg_bus,

    n64_scb.flashram n64_scb
);

    localparam [31:0] FLASH_TYPE_ID     = 32'h1111_8001;
    localparam [31:0] FLASH_MODEL_ID    = 32'h0032_00F1;

    typedef enum bit [7:0] {
        CMD_STATUS_MODE     = 8'hD2,
        CMD_READID_MODE     = 8'hE1,
        CMD_READ_MODE       = 8'hF0,
        CMD_ERASE_SECTOR    = 8'h4B,
        CMD_ERASE_CHIP      = 8'h3C,
        CMD_BUFFER_MODE     = 8'hB4,
        CMD_ERASE_START     = 8'h78,
        CMD_WRITE_START     = 8'hA5
    } e_cmd;

    typedef enum bit [1:0] {
        STATE_STATUS,
        STATE_ID,
        STATE_READ,
        STATE_BUFFER
    } e_state;

    typedef enum bit [1:0] {
        WRITE_BUSY,
        ERASE_BUSY,
        WRITE_DONE,
        ERASE_DONE
    } e_status_bits;

    e_state state;
    logic [3:0] status;
    logic [7:0] cmd;
    logic erase_enabled;

    always_comb begin
        n64_scb.flashram_read_mode = (state == STATE_READ);

        reg_bus.rdata = 16'd0;
        if (state == STATE_ID) begin
            case (reg_bus.address[2:1])
                0: reg_bus.rdata = FLASH_TYPE_ID[31:16];
                1: reg_bus.rdata = FLASH_TYPE_ID[15:0];
                2: reg_bus.rdata = FLASH_MODEL_ID[31:16];
                3: reg_bus.rdata = FLASH_MODEL_ID[15:0];
            endcase
        end else if (reg_bus.address[1]) begin
            reg_bus.rdata = {12'd0, status};
        end
    end

    always_ff @(posedge clk) begin
        if (reset) begin
            state <= STATE_STATUS;
            status <= 4'b0000;
            erase_enabled <= 1'b0;
            n64_scb.flashram_pending <= 1'b0;
        end else begin
            if (n64_scb.flashram_done) begin
                n64_scb.flashram_pending <= 1'b0;
                if (n64_scb.flashram_write_or_erase) begin
                    status[ERASE_BUSY] <= 1'b0;
                    status[ERASE_DONE] <= 1'b1;
                end else begin
                    status[WRITE_BUSY] <= 1'b0;
                    status[WRITE_DONE] <= 1'b1;
                end
            end

            if (reg_bus.write && !n64_scb.flashram_pending) begin
                if (reg_bus.address[16]) begin
                    if (!reg_bus.address[1]) begin
                        cmd <= reg_bus.wdata[15:8];
                    end else begin
                        erase_enabled <= 1'b0;

                        case (cmd)
                            CMD_STATUS_MODE: begin
                                state <= STATE_STATUS;
                            end

                            CMD_READID_MODE: begin
                                state <= STATE_ID;
                            end

                            CMD_READ_MODE: begin
                                state <= STATE_READ;
                            end

                            CMD_ERASE_SECTOR: begin
                                state <= STATE_STATUS;
                                erase_enabled <= 1'b1;
                                n64_scb.flashram_page <= reg_bus.wdata[9:0];
                                n64_scb.flashram_sector_or_all <= 1'b0;
                            end

                            CMD_ERASE_CHIP: begin
                                state <= STATE_STATUS;
                                erase_enabled <= 1'b1;
                                n64_scb.flashram_page <= 10'd0;
                                n64_scb.flashram_sector_or_all <= 1'b1;
                            end

                            CMD_BUFFER_MODE: begin
                                state <= STATE_BUFFER;
                            end

                            CMD_ERASE_START: begin
                                state <= STATE_STATUS;
                                if (erase_enabled) begin
                                    status[ERASE_BUSY] <= 1'b1;
                                    status[ERASE_DONE] <= 1'b0;
                                    n64_scb.flashram_pending <= 1'b1;
                                    n64_scb.flashram_write_or_erase <= 1'b1;
                                end
                            end

                            CMD_WRITE_START: begin
                                state <= STATE_STATUS;
                                status[WRITE_BUSY] <= 1'b1;
                                status[WRITE_DONE] <= 1'b0;
                                n64_scb.flashram_page <= reg_bus.wdata[9:0];
                                n64_scb.flashram_pending <= 1'b1;
                                n64_scb.flashram_write_or_erase <= 1'b0;
                                n64_scb.flashram_sector_or_all <= 1'b0;
                            end
                        endcase
                    end
                end else begin
                    if (reg_bus.address[1] && state == STATE_STATUS) begin
                        status[ERASE_DONE] <= 1'b0;
                        status[WRITE_DONE] <= 1'b0;
                    end
                end
            end
        end
    end

    always_comb begin
        n64_scb.flashram_write = reg_bus.write && !reg_bus.address[16] && state == STATE_BUFFER;
        n64_scb.flashram_address = reg_bus.address[6:1];
        n64_scb.flashram_wdata = reg_bus.wdata;
    end

endmodule
