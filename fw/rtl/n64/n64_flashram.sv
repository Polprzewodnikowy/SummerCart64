module n64_flashram (
    if_system.sys sys,
    if_n64_bus bus,
    if_config.flashram cfg,
    if_flashram.flashram flashram
);

    localparam [31:0] FLASH_TYPE_ID     = 32'h1111_8001;
    localparam [31:0] FLASH_MODEL_ID    = 32'h00C2_001D;

    typedef enum bit [7:0] {
        CMD_STATUS_MODE = 8'hD2,
        CMD_READID_MODE = 8'hE1,
        CMD_READ_MODE = 8'hF0,
        CMD_ERASE_SECTOR = 8'h4B,
        CMD_ERASE_CHIP = 8'h3C,
        CMD_BUFFER_MODE = 8'hB4,
        CMD_ERASE_START = 8'h78,
        CMD_WRITE_START = 8'hA5
    } e_cmd;

    typedef enum bit [0:0] { 
        S_IDLE,
        S_WAIT
    } e_bus_state;

    typedef enum bit [1:0] {
        FS_STATUS,
        FS_ID,
        FS_READ,
        FS_BUFFER
    } e_flashram_state;

    typedef enum bit [1:0] {
        B_WRITE_BUSY,
        B_ERASE_BUSY,
        B_WRITE_DONE,
        B_ERASE_DONE
    } e_flashram_status;

    e_bus_state bus_state;
    e_flashram_state flashram_state;
    logic [3:0] flashram_status;
    logic [7:0] flashram_command;
    logic flashram_erase_enabled;

    logic [31:0] write_buffer [0:31];
    logic [1:0] write_buffer_wmask;
    logic [15:0] high_buffer;

    always_comb begin
        write_buffer_wmask = 2'b00;
        if (bus.request && bus.write && !bus.address[16] && flashram_state == FS_BUFFER) begin
            write_buffer_wmask[0] = !bus.address[1];
            write_buffer_wmask[1] = bus.address[1];
        end
    end

    always_ff @(posedge sys.clk) begin
        if (write_buffer_wmask[0]) high_buffer <= bus.wdata;
    end

    always_ff @(posedge sys.clk) begin
        flashram.rdata <= write_buffer[flashram.address];
        if (write_buffer_wmask[1]) write_buffer[bus.address[6:2]] <= {high_buffer, bus.wdata};
    end

    always_comb begin
        bus.rdata = 16'd0;
        if (bus.ack) begin
            if (bus.address[1]) begin
                bus.rdata = {12'd0, flashram_status};
            end
            if (flashram_state == FS_ID) begin
                case (bus.address[2:1])
                    0: bus.rdata = FLASH_TYPE_ID[31:16];
                    1: bus.rdata = FLASH_TYPE_ID[15:0];
                    2: bus.rdata = FLASH_MODEL_ID[31:16];
                    3: bus.rdata = FLASH_MODEL_ID[15:0];
                endcase
            end
        end

        cfg.flashram_read_mode = flashram_state == FS_READ;
    end

    always_ff @(posedge sys.clk) begin
        bus.ack <= 1'b0;

        if (sys.reset) begin
            bus_state <= S_IDLE;
            flashram_state <= FS_STATUS;
            flashram_status <= 4'b0000;
            flashram_erase_enabled <= 1'b0;
            flashram.operation_pending <= 1'b0;
        end else begin
            if (flashram.operation_done) begin
                flashram.operation_pending <= 1'b0;
                if (flashram.write_or_erase) begin
                    flashram_status[B_ERASE_BUSY] <= 1'b0;
                    flashram_status[B_ERASE_DONE] <= 1'b1;
                end else begin
                    flashram_status[B_WRITE_BUSY] <= 1'b0;
                    flashram_status[B_WRITE_DONE] <= 1'b1;
                end
            end

            case (bus_state)
                S_IDLE: begin
                    if (bus.request) begin
                        bus_state <= S_WAIT;
                        bus.ack <= 1'b1;
                        if (bus.write && !flashram.operation_pending) begin
                            if (bus.address[16]) begin
                                if (!bus.address[1]) begin
                                    flashram_command <= bus.wdata[15:8];
                                end else begin
                                    flashram_erase_enabled <= 1'b0;

                                    case (flashram_command)
                                        CMD_STATUS_MODE: begin
                                            flashram_state <= FS_STATUS;
                                        end

                                        CMD_READID_MODE: begin
                                            flashram_state <= FS_ID;
                                        end

                                        CMD_READ_MODE: begin
                                            flashram_state <= FS_READ;
                                        end

                                        CMD_ERASE_SECTOR: begin
                                            flashram_state <= FS_STATUS;
                                            flashram_erase_enabled <= 1'b1;
                                            flashram.sector <= bus.wdata[9:0];
                                            flashram.sector_or_all <= 1'b0;
                                        end

                                        CMD_ERASE_CHIP: begin
                                            flashram_state <= FS_STATUS;
                                            flashram_erase_enabled <= 1'b1;
                                            flashram.sector <= 10'd0;
                                            flashram.sector_or_all <= 1'b1;
                                        end

                                        CMD_BUFFER_MODE: begin
                                            flashram_state <= FS_BUFFER;
                                        end

                                        CMD_ERASE_START: begin
                                            flashram_state <= FS_STATUS;
                                            if (flashram_erase_enabled) begin
                                                flashram_status[B_ERASE_BUSY] <= 1'b1;
                                                flashram_status[B_ERASE_DONE] <= 1'b0;
                                                flashram.operation_pending <= 1'b1;
                                                flashram.write_or_erase <= 1'b1;
                                            end
                                        end

                                        CMD_WRITE_START: begin
                                            flashram_state <= FS_STATUS;
                                            flashram_status[B_WRITE_BUSY] <= 1'b1;
                                            flashram_status[B_WRITE_DONE] <= 1'b0;
                                            flashram.sector <= bus.wdata[9:0];
                                            flashram.operation_pending <= 1'b1;
                                            flashram.write_or_erase <= 1'b0;
                                            flashram.sector_or_all <= 1'b0;
                                        end
                                    endcase
                                end
                            end
                            // else begin
                            //     if (bus.address[1] && flashram_state == FS_STATUS) begin
                            //         flashram_status[B_ERASE_DONE] <= bus.wdata[B_ERASE_DONE];
                            //         flashram_status[B_WRITE_DONE] <= bus.wdata[B_WRITE_DONE];
                            //     end
                            // end
                        end
                    end
                end

                S_WAIT: begin
                    bus_state <= S_IDLE;
                end
            endcase
        end
    end

endmodule
