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

    // Control signals input synchronization

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
    logic pi_read_delayed;
    logic pi_write;

    always_comb begin
        pi_reset = sys.n64_hard_reset;
        pi_aleh = n64_pi_aleh_ff[2];
        pi_alel = n64_pi_alel_ff[2];
        pi_read = n64_pi_read_ff[1];
        pi_read_delayed = n64_pi_read_ff[2];
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

    always_comb begin
        aleh_op = !pi_reset && last_pi_mode != PI_MODE_HIGH && pi_mode == PI_MODE_HIGH;
        alel_op = !pi_reset && last_pi_mode == PI_MODE_HIGH && pi_mode == PI_MODE_LOW;
        read_op = !pi_reset && pi_mode == PI_MODE_VALID && last_read && !pi_read;
        write_op = !pi_reset && pi_mode == PI_MODE_VALID && last_write && !pi_write;
    end

    // Input and output data sampling

    logic [15:0] n64_pi_ad_input;
    logic [15:0] n64_pi_ad_output;
    logic [15:0] n64_pi_ad_output_data;
    logic [15:0] n64_pi_ad_output_data_buffer;
    logic n64_pi_ad_output_enable;
    logic n64_pi_ad_output_enable_data;
    
    logic n64_pi_address_valid;

    always_comb begin
        n64_pi_ad = n64_pi_ad_output_enable ? n64_pi_ad_output : 16'hZZZZ;
        n64_pi_ad_output_enable_data = !pi_reset && pi_mode == PI_MODE_VALID && n64_pi_address_valid && !pi_read_delayed;
    end

    always_ff @(posedge sys.clk) begin
        n64_pi_ad_input <= n64_pi_ad;
        n64_pi_ad_output <= n64_pi_ad_output_data;
        n64_pi_ad_output_enable <= n64_pi_ad_output_enable_data;
        if (read_op) begin
            n64_pi_ad_output_data <= n64_pi_ad_output_data_buffer;
        end
    end

    // Internal bus controller

    typedef enum bit [0:0] {
        S_IDLE,
        S_WAIT
    } e_state;

    e_state state;
    logic first_operation;
    logic pending_operation;
    logic pending_write;

    sc64::e_n64_id next_id;
    logic [25:0] next_offset;

    always_ff @(posedge sys.clk) begin
        if (aleh_op) begin
            n64_pi_address_valid <= 1'b0;
            next_offset <= 32'd0;
            if (cfg.dd_enabled) begin
                if (n64_pi_ad_input == 16'h0500) begin
                    n64_pi_address_valid <= 1'b1;
                    next_id <= sc64::ID_N64_DDREGS;
                end
                if (n64_pi_ad_input >= 16'h0600 && n64_pi_ad_input < 16'h0640) begin
                    n64_pi_address_valid <= 1'b1;
                    next_id <= sc64::ID_N64_SDRAM;
                    next_offset <= cfg.dd_offset;
                end
            end
            if (n64_pi_ad_input >= 16'h0800 && n64_pi_ad_input < 16'h0802) begin
                if (cfg.sram_enabled) begin
                    n64_pi_address_valid <= 1'b1;
                    next_id <= sc64::ID_N64_SDRAM;
                    next_offset <= cfg.save_offset;
                end else if (cfg.flashram_enabled) begin
                    n64_pi_address_valid <= 1'b1;
                    next_id <= sc64::ID_N64_FLASHRAM;
                    if (cfg.flashram_read_mode) begin
                        next_id <= sc64::ID_N64_SDRAM;
                        next_offset <= cfg.save_offset;
                    end
                end
            end
            if (n64_pi_ad_input >= 16'h1000 && n64_pi_ad_input < 16'h1400) begin
                n64_pi_address_valid <= 1'b1;
                next_id <= cfg.sdram_switch ? sc64::ID_N64_SDRAM : sc64::ID_N64_BOOTLOADER;
            end
            if (n64_pi_ad_input == 16'h1FFF) begin
                n64_pi_address_valid <= 1'b1;
                next_id <= sc64::ID_N64_CPU;
            end
        end
    end

    always_ff @(posedge sys.clk) begin
        // bus.request <= 1'b0;

        if (sys.reset || sys.n64_hard_reset || sys.n64_soft_reset) begin
            state <= S_IDLE;
            bus.request <= 1'b0;
            pending_operation <= 1'b0;
        end else begin
            case (state)
                S_IDLE: begin
                    if (aleh_op) begin
                        bus.address[31:16] <= n64_pi_ad_input;
                    end
                    if (alel_op) begin
                        bus.address <= {bus.address[31:16], n64_pi_ad_input[15:1], 1'b0} + next_offset;
                    end
                    if (n64_pi_address_valid && (alel_op || read_op || write_op || pending_operation)) begin
                        state <= S_WAIT;
                        bus.id <= next_id;
                        bus.request <= 1'b1;
                        bus.write <= write_op || (pending_operation && pending_write);
                        if (!alel_op && !(first_operation && write_op)) begin
                            bus.address[31:1] <= bus.address[31:1] + 1'd1;
                        end
                        bus.wdata <= n64_pi_ad_input;
                        first_operation <= alel_op;
                    end
                end

                S_WAIT: begin
                    if (bus.ack) begin
                        state <= S_IDLE;
                        bus.request <= 1'b0;
                        n64_pi_ad_output_data_buffer <= bus.rdata;
                    end
                    if (read_op || write_op) begin
                        pending_operation <= 1'b1;
                        pending_write <= write_op;
                    end
                end

                default: begin
                    state <= S_IDLE;
                    bus.request <= 1'b0;
                    pending_operation <= 1'b0;
                end
            endcase
        end
    end

endmodule
