module n64_pi (
    input clk,
    input reset,

    mem_bus.controller mem_bus,
    n64_reg_bus.controller reg_bus,

    n64_scb.pi n64_scb,

    input n64_reset,
    input n64_nmi,
    input n64_pi_alel,
    input n64_pi_aleh,
    input n64_pi_read,
    input n64_pi_write,
    inout [15:0] n64_pi_ad
);

    // Control signals and input synchronization

    logic [1:0] n64_reset_ff;
    logic [1:0] n64_nmi_ff;
    logic [2:0] n64_pi_alel_ff;
    logic [2:0] n64_pi_aleh_ff;
    logic [1:0] n64_pi_read_ff;
    logic [2:0] n64_pi_write_ff;

    always_ff @(posedge clk) begin
        n64_reset_ff <= {n64_reset_ff[0], n64_reset};
        n64_nmi_ff <= {n64_nmi_ff[0], n64_nmi};
        n64_pi_aleh_ff <= {n64_pi_aleh_ff[1:0], n64_pi_aleh};
        n64_pi_alel_ff <= {n64_pi_alel_ff[1:0], n64_pi_alel};
        n64_pi_read_ff <= {n64_pi_read_ff[0], n64_pi_read};
        n64_pi_write_ff <= {n64_pi_write_ff[1:0], n64_pi_write};
    end

    logic pi_reset;
    logic pi_nmi;
    logic pi_aleh;
    logic pi_alel;
    logic pi_read;
    logic pi_write;

    always_comb begin
        pi_reset = n64_reset_ff[1];
        pi_nmi = n64_nmi_ff[1];
        pi_aleh = n64_pi_aleh_ff[2];
        pi_alel = n64_pi_alel_ff[2];
        pi_read = n64_pi_read_ff[1];
        pi_write = n64_pi_write_ff[2];
    end


    // PI bus state and event generator

    typedef enum bit [1:0] {
        PI_MODE_IDLE    = 2'b10,
        PI_MODE_HIGH    = 2'b11,
        PI_MODE_LOW     = 2'b01,
        PI_MODE_VALID   = 2'b00
    } e_pi_mode;

    typedef enum bit [1:0] {
        PORT_NONE,
        PORT_MEM,
        PORT_REG
    } e_port;

    e_pi_mode pi_mode;

    e_port read_port;
    e_port write_port;

    always_comb begin
        pi_mode = e_pi_mode'({pi_aleh, pi_alel});
    end

    logic last_reset;
    logic last_nmi;
    e_pi_mode last_pi_mode;
    logic last_read;
    logic last_write;

    always_ff @(posedge clk) begin
        last_reset <= pi_reset;
        last_nmi <= pi_nmi;
        last_pi_mode <= pi_mode;
        last_read <= pi_read;
        last_write <= pi_write;
    end

    logic aleh_op;
    logic alel_op;
    logic read_op;
    logic write_op;
    logic end_op;

    always_comb begin
        n64_scb.n64_reset = !last_reset && pi_reset;
        n64_scb.n64_nmi = !last_nmi && pi_nmi;
    end

    always_ff @(posedge clk) begin
        aleh_op <= pi_reset && (last_pi_mode != PI_MODE_HIGH) && (pi_mode == PI_MODE_HIGH);
        alel_op <= pi_reset && (last_pi_mode == PI_MODE_HIGH) && (pi_mode == PI_MODE_LOW);
        read_op <= pi_reset && (pi_mode == PI_MODE_VALID) && (read_port != PORT_NONE) && (last_read && !pi_read);
        write_op <= pi_reset && (pi_mode == PI_MODE_VALID) && (write_port != PORT_NONE) && (last_write && !pi_write);
        end_op <= pi_reset && (last_pi_mode == PI_MODE_VALID) && (pi_mode != PI_MODE_VALID);
    end


    // Input and output data sampling

    logic n64_pi_ad_oe;
    logic [15:0] n64_pi_ad_out;
    logic [15:0] n64_pi_dq_in;
    logic [15:0] n64_pi_dq_out;

    assign n64_pi_ad = n64_pi_ad_oe ? n64_pi_ad_out : 16'hZZZZ;

    always_ff @(posedge clk) begin
        n64_pi_ad_oe <= pi_reset && (pi_mode == PI_MODE_VALID) && !last_read && (read_port != PORT_NONE);
        n64_pi_ad_out <= n64_pi_dq_out;
        n64_pi_dq_in <= n64_pi_ad;
    end


    // Debug: last accessed PI address

    logic [15:0] pi_debug_address_buffer;

    always_ff @(posedge clk) begin
        if (aleh_op) begin
            pi_debug_address_buffer <= n64_pi_dq_in;
        end
        if (alel_op) begin
            n64_scb.pi_debug_address <= {pi_debug_address_buffer, n64_pi_dq_in};
            n64_scb.pi_debug_rw_count <= 17'd0;
        end
        if (pi_reset && (pi_mode == PI_MODE_VALID)) begin
            if ((last_read && !pi_read) || (last_write && !pi_write)) begin
                n64_scb.pi_debug_rw_count <= n64_scb.pi_debug_rw_count + 1'd1;
                n64_scb.pi_debug_direction <= !pi_write;
            end
        end
    end


    // Address decoding

    const bit [31:0] DDIPL_OFFSET       = 32'h03BC_0000;
    const bit [31:0] SAVE_OFFSET        = 32'h03FE_0000;
    const bit [31:0] FLASH_OFFSET       = 32'h0400_0000;
    const bit [31:0] BOOTLOADER_OFFSET  = 32'h04E0_0000;
    const bit [31:0] SHADOW_OFFSET      = 32'h04FE_0000;
    const bit [31:0] BUFFER_OFFSET      = 32'h0500_0000;

    logic [31:0] mem_offset;
    logic sram_selected;

    always_ff @(posedge clk) begin
        if (reset || !pi_reset || end_op) begin
            n64_scb.pi_sdram_active <= 1'b0;
            n64_scb.pi_flash_active <= 1'b0;
        end

        if (reset) begin
            read_port <= PORT_NONE;
            write_port <= PORT_NONE;
            sram_selected <= 1'b0;
            reg_bus.dd_select <= 1'b0;
            reg_bus.flashram_select <= 1'b0;
            reg_bus.cfg_select <= 1'b0;
        end else if (aleh_op) begin
            read_port <= PORT_NONE;
            write_port <= PORT_NONE;
            mem_offset <= 32'd0;
            sram_selected <= 1'b0;
            reg_bus.dd_select <= 1'b0;
            reg_bus.flashram_select <= 1'b0;
            reg_bus.cfg_select <= 1'b0;

            if (n64_scb.dd_enabled) begin
                if (n64_pi_dq_in == 16'h0500) begin
                    read_port <= PORT_REG;
                    write_port <= PORT_REG;
                    reg_bus.dd_select <= 1'b1;
                end
            end

            if (n64_scb.ddipl_enabled) begin
                if (n64_pi_dq_in >= 16'h0600 && n64_pi_dq_in < 16'h0640) begin
                    read_port <= PORT_MEM;
                    write_port <= PORT_NONE;
                    mem_offset <= (-32'h0600_0000) + DDIPL_OFFSET;
                    n64_scb.pi_sdram_active <= 1'b1;
                end
            end

            if (n64_scb.flashram_enabled) begin
                if (n64_pi_dq_in >= 16'h0800 && n64_pi_dq_in < 16'h0802) begin
                    read_port <= PORT_REG;
                    write_port <= PORT_REG;
                    mem_offset <= (-32'h0800_0000) + SAVE_OFFSET;
                    reg_bus.flashram_select <= 1'b1;
                    if (n64_scb.flashram_read_mode) begin
                        read_port <= PORT_MEM;
                        n64_scb.pi_sdram_active <= 1'b1;
                    end
                end
            end else if (n64_scb.sram_enabled) begin
                if (n64_scb.sram_banked) begin
                    if (n64_pi_dq_in >= 16'h0800 && n64_pi_dq_in < 16'h0810) begin
                        if (n64_pi_dq_in[3:2] != 2'b11 && n64_pi_dq_in[1:0] == 2'b00) begin
                            read_port <= PORT_MEM;
                            write_port <= PORT_MEM;
                            mem_offset <= (-32'h0800_0000) - {n64_pi_dq_in[3:2], 18'd0} + {n64_pi_dq_in[3:2], 15'd0} + SAVE_OFFSET;
                            sram_selected <= 1'b1;
                            n64_scb.pi_sdram_active <= 1'b1;
                        end
                    end
                end else begin
                    if (n64_pi_dq_in >= 16'h0800 && n64_pi_dq_in < 16'h0802) begin
                        read_port <= PORT_MEM;
                        write_port <= PORT_MEM;
                        mem_offset <= (-32'h0800_0000) + SAVE_OFFSET;
                        sram_selected <= 1'b1;
                        n64_scb.pi_sdram_active <= 1'b1;
                    end
                end
            end

            if (n64_scb.bootloader_enabled) begin
                if (n64_pi_dq_in >= 16'h1000 && n64_pi_dq_in < 16'h101E) begin
                    read_port <= PORT_MEM;
                    write_port <= PORT_NONE;
                    mem_offset <= (-32'h1000_0000) + BOOTLOADER_OFFSET;
                    n64_scb.pi_flash_active <= 1'b1;
                end
            end else begin
                if (n64_pi_dq_in >= 16'h1000 && n64_pi_dq_in < 16'h1400) begin
                    read_port <= PORT_MEM;
                    write_port <= n64_scb.rom_write_enabled ? PORT_MEM : PORT_NONE;
                    mem_offset <= (-32'h1000_0000);
                    n64_scb.pi_sdram_active <= 1'b1;
                end
            end

            if (n64_scb.rom_shadow_enabled) begin
                if (n64_pi_dq_in >= 16'h13FE && n64_pi_dq_in < 16'h1400) begin
                    read_port <= PORT_MEM;
                    write_port <= PORT_NONE;
                    mem_offset <= (-32'h13FE_0000) + SHADOW_OFFSET;
                    n64_scb.pi_flash_active <= 1'b1;
                end
            end

            if (n64_scb.rom_extended_enabled) begin
                if (n64_pi_dq_in >= 16'h1400 && n64_pi_dq_in < 16'h14E0) begin
                    read_port <= PORT_MEM;
                    write_port <= PORT_NONE;
                    mem_offset <= (-32'h1400_0000) + FLASH_OFFSET;
                    n64_scb.pi_flash_active <= 1'b1;
                end
            end else begin //N64 Modem
                if (n64_pi_dq_in >= 16'h1800 && n64_pi_dq_in < 16'h18E0) begin
                    read_port <= PORT_MEM;
                    write_port <= PORT_NONE;
                    mem_offset <= (-32'h1800_0000) + FLASH_OFFSET;
                    n64_scb.pi_flash_active <= 1'b1;
                end
            end

            if (n64_scb.cfg_unlock) begin
                if (n64_pi_dq_in >= 16'h1FFC && n64_pi_dq_in < 16'h1FFE) begin
                    read_port <= PORT_MEM;
                    write_port <= PORT_NONE;
                    mem_offset <= (-32'h1FFC_0000) + SHADOW_OFFSET;
                    n64_scb.pi_flash_active <= 1'b1;
                end

                if (n64_pi_dq_in >= 16'h1FFE && n64_pi_dq_in < 16'h1FFF) begin
                    read_port <= PORT_MEM;
                    write_port <= PORT_MEM;
                    mem_offset <= (-32'h1FFE_0000) + BUFFER_OFFSET;
                end
            end

            if (n64_pi_dq_in >= 16'h1FFF && n64_pi_dq_in < 16'h2000) begin
                read_port <= n64_scb.cfg_unlock ? PORT_REG : PORT_NONE;
                write_port <= PORT_REG;
                reg_bus.cfg_select <= 1'b1;
            end
        end
    end


    // Mem bus read FIFO controller

    logic read_fifo_full;
    logic read_fifo_write;
    logic [15:0] read_fifo_wdata;

    logic read_fifo_empty;
    logic read_fifo_read;
    logic [15:0] read_fifo_rdata;

    logic read_fifo_wait;

    n64_pi_fifo read_fifo_inst (
        .clk(clk),
        .reset(reset),

        .flush(reset || !pi_reset || alel_op),

        .full(read_fifo_full),
        .write(read_fifo_write),
        .wdata(read_fifo_wdata),

        .empty(read_fifo_empty),
        .read(read_fifo_read),
        .rdata(read_fifo_rdata)
    );

    always_ff @(posedge clk) begin
        read_fifo_read <= 1'b0;

        if (!pi_reset) begin
            n64_scb.pi_debug_fifo_flags[1:0] <= 2'b00;
        end

        if (reset || !pi_reset || alel_op) begin
            read_fifo_wait <= 1'b0;
        end

        if (read_port == PORT_MEM) begin
            if (read_op) begin
                if (read_fifo_empty) begin
                    read_fifo_wait <= 1'b1;
                    n64_scb.pi_debug_fifo_flags[0] <= 1'b1;
                    if (read_fifo_wait) begin
                        n64_scb.pi_debug_fifo_flags[1] <= 1'b1;
                    end
                end else begin
                    read_fifo_read <= 1'b1;
                    n64_pi_dq_out <= read_fifo_rdata;
                end
            end

            if (!read_fifo_empty && read_fifo_wait) begin
                read_fifo_read <= 1'b1;
                read_fifo_wait <= 1'b0;
                n64_pi_dq_out <= read_fifo_rdata;
            end
        end

        if (read_port == PORT_REG) begin
            if (read_op) begin
                n64_pi_dq_out <= reg_bus.rdata;
            end
        end
    end


    // Mem bus write FIFO controller

    logic write_fifo_full;
    logic write_fifo_write;
    logic [15:0] write_fifo_wdata;

    logic write_fifo_empty;
    logic write_fifo_read;
    logic [15:0] write_fifo_rdata;

    logic write_fifo_wait;

    n64_pi_fifo write_fifo_inst (
        .clk(clk),
        .reset(reset),

        .flush(reset),

        .full(write_fifo_full),
        .write(write_fifo_write),
        .wdata(write_fifo_wdata),

        .empty(write_fifo_empty),
        .read(write_fifo_read),
        .rdata(write_fifo_rdata)
    );

    always_ff @(posedge clk) begin
        write_fifo_write <= 1'b0;

        if (!pi_reset) begin
            n64_scb.pi_debug_fifo_flags[3:2] <= 2'b00;
        end

        if (reset) begin
            write_fifo_wait <= 1'b0;
        end

        if (write_port == PORT_MEM) begin
            if (write_op) begin
                if (write_fifo_full) begin
                    write_fifo_wait <= 1'b1;
                    n64_scb.pi_debug_fifo_flags[2] <= 1'b1;
                    if (write_fifo_wait) begin
                        n64_scb.pi_debug_fifo_flags[3] <= 1'b1;
                    end
                end else begin
                    write_fifo_write <= 1'b1;
                    write_fifo_wdata <= n64_pi_dq_in;
                end
            end

            if (!write_fifo_full && write_fifo_wait) begin
                write_fifo_write <= 1'b1;
                write_fifo_wait <= 1'b0;
                write_fifo_wdata <= n64_pi_dq_in;
            end
        end
    end


    // Mem bus controller

    logic [31:0] starting_address;
    logic load_starting_address;
    logic read_enabled;
    logic first_write_op;

    always_ff @(posedge clk) begin
        write_fifo_read <= 1'b0;
        load_starting_address <= 1'b0;
        n64_scb.sram_done <= 1'b0;

        if (reset || !pi_reset) begin
            mem_bus.request <= 1'b0;
            read_enabled <= 1'b0;
        end else begin
            if (aleh_op) begin
                starting_address[31:16] <= n64_pi_dq_in;
            end

            if (alel_op) begin
                starting_address <= {starting_address[31:16], n64_pi_dq_in} + mem_offset;
                load_starting_address <= 1'b1;
                read_enabled <= 1'b1;
                first_write_op <= 1'b1;
            end

            if (load_starting_address) begin
                mem_bus.address <= starting_address;
            end

            if (!mem_bus.request) begin
                if ((write_port == PORT_MEM) && !write_fifo_empty) begin
                    mem_bus.request <= 1'b1;
                    mem_bus.write <= 1'b1;
                    mem_bus.wdata <= write_fifo_rdata;
                    write_fifo_read <= 1'b1;
                    read_enabled <= 1'b0;
                    if (first_write_op) begin
                        mem_bus.address <= starting_address;
                        first_write_op <= 1'b0;
                    end
                end else if ((read_port == PORT_MEM) && !read_fifo_full && read_enabled) begin
                    mem_bus.request <= 1'b1;
                    mem_bus.write <= 1'b0;
                end
            end

            if (mem_bus.ack) begin
                mem_bus.request <= 1'b0;
                mem_bus.address[16:0] <= mem_bus.address[16:0] + 2'd2;
            end

            if (end_op) begin
                read_enabled <= 1'b0;
                n64_scb.sram_done <= sram_selected && !first_write_op;
            end
        end
    end

    always_comb begin
        read_fifo_write = !mem_bus.write && mem_bus.ack;
        read_fifo_wdata = mem_bus.rdata;
        mem_bus.wmask = 2'b11;
    end


    // Reg bus controller

    logic reg_bus_address_increment;

    always_ff @(posedge clk) begin
        reg_bus_address_increment <= read_op || write_op;

        if (aleh_op) begin
            reg_bus.address[16] <= n64_pi_dq_in[0];
        end

        if (alel_op) begin
            reg_bus.address[15:0] <= n64_pi_dq_in;
        end

        if (reg_bus_address_increment) begin
            reg_bus.address <= reg_bus.address + 2'd2;
        end

        reg_bus.read <= read_op && (read_port == PORT_REG);
        reg_bus.write <= write_op && (write_port == PORT_REG);
        reg_bus.wdata <= n64_pi_dq_in;
    end

endmodule
