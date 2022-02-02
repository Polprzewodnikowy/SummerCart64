module vendor_flash (
    input clk,
    input reset,

    input erase_start,
    output erase_busy,
    input wp_enable,
    input wp_disable,

    input request,
    output ack,
    input write,
    input [31:0] address,
    input [31:0] wdata,
    output [31:0] rdata
);

    const int FLASH_SECTORS = 3'd4;

    typedef enum bit [1:0] {
        STATE_START,
        STATE_PENDING,
        STATE_ERASING
    } e_erase_state;

    typedef enum bit [0:0] {
        CSR_STATUS = 1'b0,
        CSR_CONTROL = 1'b1
    } e_flash_csr;

    typedef enum bit [1:0] {
        STATUS_IDLE = 2'b00,
        STATUS_BUSY_ERASE = 2'b01,
        STATUS_BUSY_WRITE = 2'b10,
        STATUS_BUSY_READ = 2'b11
    } e_flash_status;

    logic csr_read;
    logic csr_write;
    e_flash_csr csr_address;
    logic [31:0] csr_wdata;
    logic [31:0] csr_rdata;

    logic wp_setting;
    logic [2:0] erase_sector;
    e_erase_state state;

    always_ff @(posedge clk) begin
        csr_read <= 1'b0;
        csr_write <= 1'b0;
        csr_address <= CSR_STATUS;

        if (reset) begin
            erase_busy <= 1'b0;
            wp_setting <= 1'b1;
        end else if (!erase_busy) begin
            if (erase_start) begin
                erase_busy <= 1'b1;
                erase_sector <= 3'd1;
                state <= STATE_START;
            end else if (wp_enable) begin
                csr_write <= 1'b1;
                csr_address <= CSR_CONTROL;
                csr_wdata <= 32'hFFFF_FFFF;
                wp_setting <= 1'b1;
            end else if (wp_disable) begin
                csr_write <= 1'b1;
                csr_address <= CSR_CONTROL;
                csr_wdata <= 32'hF07F_FFFF;
                wp_setting <= 1'b0;
            end
        end else begin
            csr_read <= 1'b1;

            case (state)
                STATE_START: begin
                    if (csr_read && (csr_rdata[1:0] == STATUS_IDLE)) begin
                        csr_read <= 1'b0;
                        csr_write <= 1'b1;
                        csr_address <= CSR_CONTROL;
                        csr_wdata <= {4'hF, {5{wp_setting}}, erase_sector, 20'hFFFFF};
                        state <= STATE_PENDING;
                    end
                end

                STATE_PENDING: begin
                    if (csr_read && (csr_rdata[1:0] == STATUS_BUSY_ERASE)) begin
                        state <= STATE_ERASING;
                    end
                end

                STATE_ERASING: begin
                    if (csr_read && (csr_rdata[1:0] == STATUS_IDLE)) begin
                        if (erase_sector == FLASH_SECTORS) begin
                            erase_busy <= 1'b0;
                        end else begin
                            erase_sector <= erase_sector + 1'd1;
                            state <= STATE_START;
                        end
                    end
                end
            endcase
        end
    end

    logic data_read;
    logic data_write;
    logic data_busy;
    logic data_ack;
    logic [15:0] data_address;
    logic [31:0] data_wdata;
    logic [31:0] data_rdata;

    logic pending;
    logic write_ack;

    always_ff @(posedge clk) begin
        write_ack <= 1'b0;

        if (reset) begin
            data_read <= 1'b0;
            data_write <= 1'b0;
            pending <= 1'b0;
        end else begin
            if (request && !pending && !erase_busy) begin
                pending <= 1'b1;
                if (write && !wp_setting) begin
                    data_write <= 1'b1;
                end else begin
                    data_read <= 1'b1;
                end
            end

            if (data_read && !data_busy) begin
                data_read <= 1'b0;
            end

            if (data_write && !data_busy) begin
                data_write <= 1'b0;
                pending <= 1'b0;
                write_ack <= 1'b1;
            end

            if (data_ack) begin
                pending <= 1'b0;
            end
        end
    end

    always_comb begin
        ack = data_ack || write_ack;
        data_address = address[17:2];
    end

    intel_flash intel_flash_inst (
        .clock(clk),
        .reset_n(~reset),

        .avmm_csr_read(csr_read),
        .avmm_csr_write(csr_write),
        .avmm_csr_addr(csr_address),
        .avmm_csr_writedata(csr_wdata),
        .avmm_csr_readdata(csr_rdata),

        .avmm_data_read(data_read),
        .avmm_data_write(data_write),
        .avmm_data_waitrequest(data_busy),
        .avmm_data_readdatavalid(data_ack),
        .avmm_data_addr(data_address),
        .avmm_data_writedata(wdata),
        .avmm_data_readdata(rdata),
        .avmm_data_burstcount(2'd1)
    );

endmodule
