module n64_pi (
    if_system.sys sys,
    if_config.pi cfg,
    if_n64_bus.n64 bus,

    input n64_pi_alel,
    input n64_pi_aleh,
    input n64_pi_read,
    input n64_pi_write,
    inout [15:0] n64_pi_ad
);

    // FIFOs

    logic read_fifo_flush;

    logic read_fifo_full;
    logic read_fifo_write;
    logic [15:0] read_fifo_wdata;

    logic read_fifo_empty;
    logic read_fifo_read;
    logic [15:0] read_fifo_rdata;

    n64_pi_fifo read_fifo_inst (
        .sys(sys),

        .flush(read_fifo_flush),
        
        .full(read_fifo_full),
        .write(read_fifo_write),
        .wdata(read_fifo_wdata),
        
        .empty(read_fifo_empty),
        .read(read_fifo_read),
        .rdata(read_fifo_rdata)
    );

    logic write_fifo_flush;

    logic write_fifo_full;
    logic write_fifo_write;
    logic [15:0] write_fifo_wdata;

    logic write_fifo_empty;
    logic write_fifo_read;
    logic [15:0] write_fifo_rdata;

    n64_pi_fifo write_fifo_inst (
        .sys(sys),

        .flush(write_fifo_flush),

        .full(write_fifo_full),
        .write(write_fifo_write),
        .wdata(write_fifo_wdata),

        .empty(write_fifo_empty),
        .read(write_fifo_read),
        .rdata(write_fifo_rdata)
    );


    // Control signals and input synchronization

    logic [2:0] n64_pi_alel_ff;
    logic [2:0] n64_pi_aleh_ff;
    logic [2:0] n64_pi_read_ff;
    logic [2:0] n64_pi_write_ff;

    always_ff @(posedge sys.clk) begin
        n64_pi_aleh_ff <= {n64_pi_aleh_ff[1:0], n64_pi_aleh};
        n64_pi_alel_ff <= {n64_pi_alel_ff[1:0], n64_pi_alel};
        n64_pi_read_ff <= {n64_pi_read_ff[1:0], n64_pi_read};
        n64_pi_write_ff <= {n64_pi_write_ff[1:0], n64_pi_write};
    end

    logic pi_reset;
    logic pi_aleh;
    logic pi_alel;
    logic pi_read;
    logic pi_write;

    always_comb begin
        pi_reset = sys.n64_hard_reset;
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

    e_pi_mode pi_mode;
    e_pi_mode last_pi_mode;
    logic last_read;
    logic last_write;

    always_comb begin
        pi_mode = e_pi_mode'({pi_aleh, pi_alel});
    end

    always_ff @(posedge sys.clk) begin
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
        aleh_op = !pi_reset && last_pi_mode != PI_MODE_HIGH && pi_mode == PI_MODE_HIGH;
        alel_op = !pi_reset && last_pi_mode == PI_MODE_HIGH && pi_mode == PI_MODE_LOW;
        read_op = !pi_reset && pi_mode == PI_MODE_VALID && last_read && !pi_read;
        write_op = !pi_reset && pi_mode == PI_MODE_VALID && last_write && !pi_write;
        end_op = !pi_reset && last_pi_mode == PI_MODE_VALID && pi_mode != PI_MODE_VALID;
    end


    // Input and output data sampling

    logic [15:0] n64_pi_ad_input;
    logic [15:0] n64_pi_ad_output;
    logic [15:0] n64_pi_ad_output_data;
    logic n64_pi_ad_output_enable;
    logic n64_pi_ad_output_enable_data;
    
    logic n64_pi_address_valid;
    logic pending_operation;
    logic pending_write;

    always_comb begin
        n64_pi_ad = n64_pi_ad_output_enable ? n64_pi_ad_output : 16'hZZZZ;
        n64_pi_ad_output_enable_data = !pi_reset && pi_mode == PI_MODE_VALID && n64_pi_address_valid && !n64_pi_read_ff[2];
    end

    always_ff @(posedge sys.clk) begin
        n64_pi_ad_input <= n64_pi_ad;
        n64_pi_ad_output <= n64_pi_ad_output_data;
        n64_pi_ad_output_enable <= n64_pi_ad_output_enable_data;
    end
    
    logic wait_for_read_fifo;
    logic wait_for_write_fifo;

    always_comb begin
        read_fifo_write = bus.ack && !bus.write;
        read_fifo_wdata = bus.rdata;
        
        write_fifo_wdata = n64_pi_ad_input;
    end

    always_ff @(posedge sys.clk) begin
        read_fifo_read <= 1'b0;
        write_fifo_write <= 1'b0;

        if (sys.reset || sys.n64_hard_reset) begin
            wait_for_read_fifo <= 1'b0;
            wait_for_write_fifo <= 1'b0;
        end else if (n64_pi_address_valid) begin
            if (read_op || wait_for_read_fifo) begin
                if (read_fifo_empty) begin
                    wait_for_read_fifo <= 1'b1;
                end else begin
                    n64_pi_ad_output_data <= read_fifo_rdata;
                    read_fifo_read <= 1'b1;
                    wait_for_read_fifo <= 1'b0;
                end
            end
            if (write_op || wait_for_write_fifo) begin
                if (write_fifo_full) begin
                    wait_for_write_fifo <= 1'b1;
                end else begin
                    write_fifo_write <= 1'b1;
                    wait_for_write_fifo <= 1'b0;
                end
            end
        end
    end


    // Address decoding

    sc64::e_n64_id next_id;
    logic [31:0] next_offset;
    logic sram_selected;
    logic cfg_selected;

    always_ff @(posedge sys.clk) begin
        if (aleh_op) begin
            n64_pi_address_valid <= 1'b0;
            next_offset <= 32'd0;
            sram_selected <= 1'b0;
            cfg_selected <= 1'b0;
            if (cfg.dd_enabled) begin
                if (n64_pi_ad_input == 16'h0500) begin
                    n64_pi_address_valid <= 1'b1;
                    next_id <= sc64::ID_N64_DD;
                end
                if (n64_pi_ad_input >= 16'h0600 && n64_pi_ad_input < 16'h0640) begin
                    n64_pi_address_valid <= 1'b1;
                    next_id <= sc64::ID_N64_SDRAM;
                    next_offset <= cfg.ddipl_offset + 32'h0A00_0000;
                end
            end
            if (cfg.flashram_enabled) begin
                if (n64_pi_ad_input >= 16'h0800 && n64_pi_ad_input < 16'h0802) begin
                    n64_pi_address_valid <= 1'b1;
                    next_id <= sc64::ID_N64_FLASHRAM;
                    if (cfg.flashram_read_mode) begin
                        next_offset <= cfg.save_offset + 32'h0800_0000;
                    end
                end
            end else if (cfg.sram_enabled) begin
                if (cfg.sram_banked) begin
                    if (n64_pi_ad_input >= 16'h0800 && n64_pi_ad_input < 16'h0810) begin
                        if (n64_pi_ad_input[3:2] != 2'b11 && n64_pi_ad_input[1:0] == 2'b00) begin
                            n64_pi_address_valid <= 1'b1;
                            next_id <= sc64::ID_N64_SDRAM;
                            next_offset <= cfg.save_offset - {n64_pi_ad_input[3:2], 18'd0} + {n64_pi_ad_input[3:2], 15'd0} + 32'h0800_0000;
                            sram_selected <= 1'b1;
                        end
                    end
                end else begin
                    if (n64_pi_ad_input == 16'h0800) begin
                        n64_pi_address_valid <= 1'b1;
                        next_id <= sc64::ID_N64_SDRAM;
                        next_offset <= cfg.save_offset + 32'h0800_0000;
                        sram_selected <= 1'b1;
                    end
                end
            end
            if (n64_pi_ad_input >= 16'h1000 && n64_pi_ad_input < 16'h1400) begin
                n64_pi_address_valid <= 1'b1;
                next_id <= cfg.sdram_switch ? sc64::ID_N64_SDRAM : sc64::ID_N64_BOOTLOADER;
            end
            if (n64_pi_ad_input == 16'h1FFF) begin
                n64_pi_address_valid <= 1'b1;
                next_id <= sc64::ID_N64_CFG;
                cfg_selected <= 1'b1;
            end
        end
        if (alel_op) begin
            if (sram_selected) begin
                if (n64_pi_ad_input[15]) begin
                    n64_pi_address_valid <= 1'b0;
                end
            end
            if (cfg_selected) begin
                if (|n64_pi_ad_input[15:4]) begin
                    n64_pi_address_valid <= 1'b0;
                end
            end
        end
    end


    // Bus controller

    logic can_read;
    logic first_write_op;
    logic load_starting_address;
    sc64::e_n64_id starting_id;
    logic [31:0] starting_address;

    always_ff @(posedge sys.clk) begin
        read_fifo_flush <= 1'b0;

        write_fifo_read <= 1'b0;

        if (sys.reset || sys.n64_hard_reset) begin
            bus.request <= 1'b0;
            read_fifo_flush <= 1'b1;
            write_fifo_flush <= 1'b1;
        end else begin
            write_fifo_flush <= starting_id == sc64::ID_N64_SDRAM && !cfg.sdram_writable && !sram_selected;

            if (aleh_op) begin
                starting_address[31:16] <= n64_pi_ad_input;
            end

            if (alel_op) begin
                read_fifo_flush <= 1'b1;
                can_read <= 1'b1;
                first_write_op <= 1'b1;
                load_starting_address <= 1'b1;
                starting_id <= next_id;
                starting_address <= {starting_address[31:16], n64_pi_ad_input[15:1], 1'b0};
            end

            if (write_op) begin
                can_read <= 1'b0;
                if (first_write_op) begin
                    first_write_op <= 1'b0;
                    load_starting_address <= 1'b1;
                end
            end

            if (!bus.request) begin
                if (!write_fifo_empty) begin
                    bus.request <= 1'b1;
                    bus.write <= 1'b1;
                    if (load_starting_address) begin
                        bus.id <= starting_id;
                        bus.address <= starting_address + next_offset;
                        if (starting_id == sc64::ID_N64_FLASHRAM) begin
                            bus.address <= starting_address;
                        end
                        load_starting_address <= 1'b0;
                    end
                    bus.wdata <= write_fifo_rdata;
                    write_fifo_read <= 1'b1;
                end else if (!read_fifo_full && can_read) begin
                    bus.request <= 1'b1;
                    bus.write <= 1'b0;
                    if (load_starting_address) begin
                        bus.id <= starting_id;
                        bus.address <= starting_address + next_offset;
                        if (starting_id == sc64::ID_N64_FLASHRAM && cfg.flashram_read_mode) begin
                            bus.id <= sc64::ID_N64_SDRAM;
                        end
                        load_starting_address <= 1'b0;
                    end
                end
            end else if (bus.ack) begin
                bus.request <= 1'b0;
                bus.address <= bus.address + 2'd2;
            end

            if (end_op) begin
                can_read <= 1'b0;
            end
        end
    end

endmodule
