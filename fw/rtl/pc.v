module pc (
    input i_clk,
    input i_reset,

    input i_ftdi_clk,
    input i_ftdi_cs,
    input i_ftdi_do,
    output reg o_ftdi_di,

    output reg o_read_rq,
    output reg o_write_rq,
    input i_ack,
    output reg [31:0] o_address,
    input [31:0] i_data,
    output reg [31:0] o_data,

    input i_bus_active,
    output reg o_n64_disable
);

    // Command ids

    localparam [7:0] CMD_STATUS         = 8'h00;
    localparam [7:0] CMD_CONFIG         = 8'h10;
    localparam [7:0] CMD_ADDR           = 8'h20;
    localparam [7:0] CMD_READ_LENGTH    = 8'h30;
    localparam [7:0] CMD_WRITE          = 8'h40;
    localparam [7:0] CMD_READ           = 8'h50;
    localparam [7:0] CMD_CART_RESET     = 8'hFC;
    localparam [7:0] CMD_FLUSH_WRITE    = 8'hFD;
    localparam [7:0] CMD_FLUSH_READ     = 8'hFE;
    localparam [7:0] CMD_SPI_RESET      = 8'hFF;

    // SPI [de]serializer

    reg [4:0] r_spi_bit_counter;
    reg [30:0] r_spi_i_shift;
    reg [7:0] r_spi_cmd;
    reg r_spi_cmd_valid;

    reg r_fifo_pc_to_bus_flush;
    reg r_fifo_bus_to_pc_flush;

    reg r_fifo_pc_to_bus_rq;
    reg r_fifo_bus_to_pc_rq;

    wire [31:0] w_fifo_bus_to_pc_data;

    wire [10:0] w_fifo_pc_to_bus_usedw;
    wire [10:0] w_fifo_bus_to_pc_usedw;

    reg r_n64_disabled_ff1, r_n64_disabled_ff2;
    reg r_address_inc_ff1, r_address_inc_ff2;

    wire [31:0] w_spi_status = {
        8'hAA,                      // Test control byte
        r_address_inc_ff2,
        r_n64_disabled_ff2,
        w_fifo_pc_to_bus_usedw,
        w_fifo_bus_to_pc_usedw,
    };

    // SPI bit control and command stage

    always @(posedge i_ftdi_clk or posedge i_ftdi_cs or posedge i_reset) begin
        if (i_ftdi_cs || i_reset) begin
            r_spi_bit_counter <= 5'd0;
            r_spi_cmd <= 8'd0;
            r_spi_cmd_valid <= 1'b0;
        end else begin
            r_spi_bit_counter <= r_spi_bit_counter + 5'd1;
            if (&r_spi_bit_counter[2:0] && !r_spi_cmd_valid) begin
                r_spi_bit_counter <= 5'd0;
                r_spi_cmd <= {r_spi_i_shift[6:0], i_ftdi_do};
                r_spi_cmd_valid <= 1'b1;
            end
        end
    end

    // SPI input shift register

    always @(posedge i_ftdi_clk) begin
        r_spi_i_shift <= {r_spi_i_shift[29:0], i_ftdi_do};
    end

    // SPI command control signals

    always @(posedge i_ftdi_clk or posedge i_ftdi_cs or posedge i_reset) begin
        if (i_ftdi_cs || i_reset) begin
            r_fifo_pc_to_bus_flush <= 1'b0;
            r_fifo_bus_to_pc_flush <= 1'b0;
            r_fifo_pc_to_bus_rq <= 1'b0;
            r_fifo_bus_to_pc_rq <= 1'b0;
        end else begin
            if (&r_spi_bit_counter[2:0] && !r_spi_cmd_valid) begin
                case ({r_spi_i_shift[6:0], i_ftdi_do})
                    CMD_FLUSH_WRITE: r_fifo_pc_to_bus_flush <= 1'b1;
                    CMD_FLUSH_READ: r_fifo_bus_to_pc_flush <= 1'b1;
                    CMD_SPI_RESET: begin
                        r_fifo_pc_to_bus_flush <= 1'b1;
                        r_fifo_bus_to_pc_flush <= 1'b1;
                    end
                endcase
            end
            if (r_spi_bit_counter == 5'd30) begin
                case (r_spi_cmd)
                    CMD_CONFIG, CMD_ADDR, CMD_READ_LENGTH, CMD_WRITE, CMD_CART_RESET: begin
                        r_fifo_pc_to_bus_rq <= 1'b1;
                    end
                    CMD_READ: r_fifo_bus_to_pc_rq <= 1'b1;
                endcase
            end else begin
                r_fifo_pc_to_bus_rq <= 1'b0;
                r_fifo_bus_to_pc_rq <= 1'b0;
            end
        end
    end

    // SPI output data stage

    always @(negedge i_ftdi_clk or posedge i_ftdi_cs or posedge i_reset) begin
        if (i_ftdi_cs || i_reset) begin
            o_ftdi_di <= 1'b0;
        end else begin
            if (r_spi_cmd_valid) begin
                case (r_spi_cmd)
                    CMD_STATUS: o_ftdi_di <= w_spi_status[5'd31 - r_spi_bit_counter];
                    CMD_READ: o_ftdi_di <= w_fifo_bus_to_pc_data[5'd31 - r_spi_bit_counter];
                    default: o_ftdi_di <= 1'b1;
                endcase
            end else begin
                o_ftdi_di <= 1'b0;
            end
        end
    end

    // sys_clk -> spi_clk signal synchronization

    reg r_address_inc;

    always @(posedge i_ftdi_clk) begin
        {r_n64_disabled_ff2, r_n64_disabled_ff1} <= {r_n64_disabled_ff1, o_n64_disable};
        {r_address_inc_ff2, r_address_inc_ff1} <= {r_address_inc_ff1, r_address_inc};
    end

    // FIFOs

    reg r_fifo_pc_to_bus_rdreq;
    wire [39:0] w_fifo_pc_to_bus_q;
    wire w_fifo_pc_to_bus_rdempty;

    wire [7:0] w_fifo_pc_to_bus_cmd;
    wire [31:0] w_fifo_pc_to_bus_data;

    assign {w_fifo_pc_to_bus_cmd, w_fifo_pc_to_bus_data} = w_fifo_pc_to_bus_q;

    wire w_fifo_bus_to_pc_wrfull;

    reg r_bus_read_in_progress;

    fifo_pc_to_bus fifo_pc_to_bus_inst (
        .aclr(r_fifo_pc_to_bus_flush),

        .wrclk(i_ftdi_clk || i_ftdi_cs),
        .wrreq(r_fifo_pc_to_bus_rq),
        .data({r_spi_cmd, r_spi_i_shift, i_ftdi_do}),
        .wrusedw(w_fifo_pc_to_bus_usedw),
        
        .rdclk(i_clk),
        .rdreq(r_fifo_pc_to_bus_rdreq),
        .q(w_fifo_pc_to_bus_q),
        .rdempty(w_fifo_pc_to_bus_rdempty)
    );

    fifo_bus_to_pc fifo_bus_to_pc_inst (
        .aclr(r_fifo_bus_to_pc_flush),

        .rdclk(i_ftdi_clk || i_ftdi_cs),
        .rdreq(r_fifo_bus_to_pc_rq),
        .q(w_fifo_bus_to_pc_data),
        .rdusedw(w_fifo_bus_to_pc_usedw),

        .wrclk(i_clk),
        .wrreq(i_ack && r_bus_read_in_progress),
        .data(i_data),
        .wrfull(w_fifo_bus_to_pc_wrfull),
    );

    // Bus controller

    reg r_cmd_config;
    reg r_cmd_addr;
    reg r_cmd_read_length;
    reg r_cmd_write;
    reg r_cmd_cart_reset;

    always @(posedge i_clk) begin
        r_cmd_config <= w_fifo_pc_to_bus_cmd == CMD_CONFIG;
        r_cmd_addr <= w_fifo_pc_to_bus_cmd == CMD_ADDR;
        r_cmd_read_length <= w_fifo_pc_to_bus_cmd == CMD_READ_LENGTH;
        r_cmd_write <= w_fifo_pc_to_bus_cmd == CMD_WRITE;
        r_cmd_cart_reset <= w_fifo_pc_to_bus_cmd == CMD_CART_RESET;
    end

    reg r_bus_read_pending_rq;
    reg [23:0] r_bus_read_remaining_words;      // Max 64 MB

    reg r_bus_write_in_progress;

    always @(posedge i_clk or posedge i_reset) begin
        if (i_reset) begin
            o_read_rq <= 1'b0;
            o_write_rq <= 1'b0;
            o_n64_disable <= 1'b0;
            r_address_inc <= 1'b1;
            r_fifo_pc_to_bus_rdreq <= 1'b0;
            r_bus_read_pending_rq <= 1'b0;
            r_bus_read_in_progress <= 1'b0;
            r_bus_write_in_progress <= 1'b0;
        end else begin
            o_read_rq <= 1'b0;
            o_write_rq <= 1'b0;
            r_fifo_pc_to_bus_rdreq <= 1'b0;

            if (!w_fifo_pc_to_bus_rdempty && !r_fifo_pc_to_bus_rdreq && !r_bus_read_in_progress && !r_bus_write_in_progress) begin
                if (r_cmd_config && !i_bus_active) begin
                    {r_address_inc, o_n64_disable} <= w_fifo_pc_to_bus_data[1:0];
                    r_fifo_pc_to_bus_rdreq <= 1'b1;
                end
                if (r_cmd_addr) begin
                    o_address <= w_fifo_pc_to_bus_data;
                    r_fifo_pc_to_bus_rdreq <= 1'b1;
                end
                if (r_cmd_read_length) begin
                    if (o_n64_disable) begin
                        r_bus_read_pending_rq <= 1'b1;
                        r_bus_read_remaining_words <= w_fifo_pc_to_bus_data[23:0];
                        r_bus_read_in_progress <= 1'b1;
                    end else begin
                        r_fifo_pc_to_bus_rdreq <= 1'b1;
                    end
                end
                if (r_cmd_write) begin
                    if (o_n64_disable) begin
                        o_write_rq <= 1'b1;
                        o_data <= w_fifo_pc_to_bus_data;
                        r_bus_write_in_progress <= 1'b1;
                    end else begin
                        r_fifo_pc_to_bus_rdreq <= 1'b1;
                    end
                end
            end

            if (i_ack) o_address[31:2] <= o_address[31:2] + r_address_inc;

            if (!w_fifo_bus_to_pc_wrfull && r_bus_read_pending_rq) begin
                o_read_rq <= 1'b1;
                r_bus_read_pending_rq <= 1'b0;
            end

            if (i_ack && r_bus_read_in_progress) begin
                if (r_bus_read_remaining_words > 24'd0) begin
                    r_bus_read_pending_rq <= 1'b1;
                    r_bus_read_remaining_words <= r_bus_read_remaining_words - 24'd1;
                end else begin
                    r_fifo_pc_to_bus_rdreq <= 1'b1;
                    r_bus_read_in_progress <= 1'b0;
                end
            end

            if (i_ack && r_bus_write_in_progress) begin
                r_fifo_pc_to_bus_rdreq <= 1'b1;
                r_bus_write_in_progress <= 1'b0;
            end

            if (!w_fifo_pc_to_bus_rdempty && !r_fifo_pc_to_bus_rdreq && r_cmd_cart_reset) begin
                o_read_rq <= 1'b0;
                o_write_rq <= 1'b0;
                o_n64_disable <= 1'b0;
                r_address_inc <= 1'b1;
                r_fifo_pc_to_bus_rdreq <= 1'b1;
                r_bus_read_pending_rq <= 1'b0;
                r_bus_read_in_progress <= 1'b0;
                r_bus_write_in_progress <= 1'b0;
            end
        end
    end

endmodule
