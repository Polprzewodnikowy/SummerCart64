module n64_cfg (
    input clk,
    input reset,

    n64_reg_bus.cfg reg_bus,

    n64_scb.cfg n64_scb,

    output logic irq
);

    typedef enum bit [3:0] {
        REG_STATUS,
        REG_COMMAND,
        REG_DATA_0_H,
        REG_DATA_0_L,
        REG_DATA_1_H,
        REG_DATA_1_L,
        REG_IDENTIFIER_H,
        REG_IDENTIFIER_L,
        REG_KEY_H,
        REG_KEY_L,
        REG_IRQ_H,
        REG_IRQ_L,
        REG_AUX_H,
        REG_AUX_L
    } e_reg;

    logic cmd_error;
    logic cmd_irq_request;
    logic cmd_irq;

    logic btn_irq;
    logic usb_irq;
    logic aux_irq;

    logic usb_irq_enabled;
    logic aux_irq_enabled;

    always_ff @(posedge clk) begin
        irq <= (
            cmd_irq ||
            btn_irq ||
            (usb_irq_enabled && usb_irq) ||
            (aux_irq_enabled && aux_irq)
        );
    end

    always_comb begin
        reg_bus.rdata = 16'd0;
        if (reg_bus.address[16] && (reg_bus.address[15:5] == 11'd0)) begin
            case (reg_bus.address[4:1])
                REG_STATUS: reg_bus.rdata = {
                    n64_scb.cfg_pending,
                    cmd_error,
                    btn_irq,
                    cmd_irq,
                    usb_irq,
                    aux_irq,
                    10'd0
                };
                REG_COMMAND: reg_bus.rdata = {7'd0, cmd_irq_request, n64_scb.cfg_cmd};
                REG_DATA_0_H: reg_bus.rdata = n64_scb.cfg_wdata[0][31:16];
                REG_DATA_0_L: reg_bus.rdata = n64_scb.cfg_wdata[0][15:0];
                REG_DATA_1_H: reg_bus.rdata = n64_scb.cfg_wdata[1][31:16];
                REG_DATA_1_L: reg_bus.rdata = n64_scb.cfg_wdata[1][15:0];
                REG_IDENTIFIER_H: reg_bus.rdata = n64_scb.cfg_identifier[31:16];
                REG_IDENTIFIER_L: reg_bus.rdata = n64_scb.cfg_identifier[15:0];
                REG_KEY_H: reg_bus.rdata = 16'd0;
                REG_KEY_L: reg_bus.rdata = 16'd0;
                REG_IRQ_H: reg_bus.rdata = {
                    8'd0,
                    1'b1,
                    1'b1,
                    usb_irq_enabled,
                    aux_irq_enabled,
                    4'd0
                };
                REG_IRQ_L: reg_bus.rdata = 16'd0;
                REG_AUX_H: reg_bus.rdata = n64_scb.aux_wdata[31:16];
                REG_AUX_L: reg_bus.rdata = n64_scb.aux_wdata[15:0];
            endcase
        end
    end

    logic unlock_flag;
    logic lock_sequence_counter;

    always_ff @(posedge clk) begin
        n64_scb.aux_pending <= 1'b0;

        if (n64_scb.cfg_pending && n64_scb.cfg_done) begin
            n64_scb.cfg_pending <= 1'b0;
            cmd_irq <= cmd_irq_request;
            cmd_error <= n64_scb.cfg_error;
        end

        if (n64_scb.cfg_unlock) begin
            if (n64_scb.btn_irq) begin
                btn_irq <= 1'b1;
            end

            if (n64_scb.usb_irq) begin
                usb_irq <= 1'b1;
            end

            if (n64_scb.aux_irq) begin
                aux_irq <= 1'b1;
            end
        end

        if (unlock_flag) begin
            n64_scb.cfg_unlock <= 1'b1;
        end

        if (reset) begin
            n64_scb.cfg_unlock <= 1'b0;
            n64_scb.cfg_pending <= 1'b0;
            n64_scb.cfg_cmd <= 8'h00;
            cmd_error <= 1'b0;
            cmd_irq_request <= 1'b0;
            cmd_irq <= 1'b0;
            btn_irq <= 1'b0;
            usb_irq <= 1'b0;
            aux_irq <= 1'b0;
            usb_irq_enabled <= 1'b0;
            aux_irq_enabled <= 1'b0;
            lock_sequence_counter <= 1'd0;
        end else if (n64_scb.n64_reset || n64_scb.n64_nmi) begin
            n64_scb.cfg_unlock <= 1'b0;
            cmd_irq_request <= 1'b0;
            cmd_irq <= 1'b0;
            btn_irq <= 1'b0;
            usb_irq <= 1'b0;
            aux_irq <= 1'b0;
            usb_irq_enabled <= 1'b0;
            aux_irq_enabled <= 1'b0;
            lock_sequence_counter <= 1'd0;
        end else if (n64_scb.cfg_unlock) begin
            if (reg_bus.write && reg_bus.address[16] && (reg_bus.address[15:5] == 11'd0)) begin
                case (reg_bus.address[4:1])
                    REG_COMMAND: begin
                        if (!n64_scb.cfg_pending) begin
                            n64_scb.cfg_pending <= 1'b1;
                            cmd_error <= 1'b0;
                            cmd_irq_request <= reg_bus.wdata[8];
                            n64_scb.cfg_cmd <= reg_bus.wdata[7:0];
                        end
                    end

                    REG_DATA_0_H: begin
                        if (!n64_scb.cfg_pending) begin
                            n64_scb.cfg_rdata[0][31:16] <= reg_bus.wdata;
                        end
                    end

                    REG_DATA_0_L: begin
                        if (!n64_scb.cfg_pending) begin
                            n64_scb.cfg_rdata[0][15:0] <= reg_bus.wdata;
                        end
                    end

                    REG_DATA_1_H: begin
                        if (!n64_scb.cfg_pending) begin
                            n64_scb.cfg_rdata[1][31:16] <= reg_bus.wdata;
                        end
                    end

                    REG_DATA_1_L: begin
                        if (!n64_scb.cfg_pending) begin
                            n64_scb.cfg_rdata[1][15:0] <= reg_bus.wdata;
                        end
                    end

                    REG_IDENTIFIER_H: begin
                        btn_irq <= 1'b0;
                    end

                    REG_KEY_H, REG_KEY_L: begin
                        lock_sequence_counter <= lock_sequence_counter + 1'd1;
                        if (reg_bus.wdata != 16'hFFFF) begin
                            lock_sequence_counter <= 1'd0;
                        end
                        if (lock_sequence_counter == 1'd1) begin
                            if (reg_bus.wdata == 16'hFFFF) begin
                                n64_scb.cfg_unlock <= 1'b0;
                                cmd_irq_request <= 1'b0;
                                cmd_irq <= 1'b0;
                                btn_irq <= 1'b0;
                                usb_irq <= 1'b0;
                                aux_irq <= 1'b0;
                                usb_irq_enabled <= 1'b0;
                                aux_irq_enabled <= 1'b0;
                            end
                        end
                    end

                    REG_IRQ_H: begin
                        btn_irq <= (reg_bus.wdata[15] ? 1'b0 : btn_irq);
                        cmd_irq <= (reg_bus.wdata[14] ? 1'b0 : cmd_irq);
                        usb_irq <= (reg_bus.wdata[13] ? 1'b0 : usb_irq);
                        aux_irq <= (reg_bus.wdata[12] ? 1'b0 : aux_irq);
                    end

                    REG_IRQ_L: begin
                        if (reg_bus.wdata[11]) begin
                            usb_irq_enabled <= 1'b0;
                        end
                        if (reg_bus.wdata[10]) begin
                            usb_irq_enabled <= 1'b1;
                        end
                        if (reg_bus.wdata[9]) begin
                            aux_irq_enabled <= 1'b0;
                        end
                        if (reg_bus.wdata[8]) begin
                            aux_irq_enabled <= 1'b1;
                        end
                    end

                    REG_AUX_H: begin
                        n64_scb.aux_rdata[31:16] <= reg_bus.wdata;
                    end

                    REG_AUX_L: begin
                        n64_scb.aux_pending <= 1'b1;
                        n64_scb.aux_rdata[15:0] <= reg_bus.wdata;
                    end
                endcase
            end
        end
    end

    const bit [15:0] UNLOCK_SEQUENCE [4] = {
        16'h5F55,
        16'h4E4C,
        16'h4F43,
        16'h4B5F
    };

    logic [1:0] unlock_sequence_counter;

    always_ff @(posedge clk) begin
        unlock_flag <= 1'b0;

        if (reset || n64_scb.n64_reset || n64_scb.n64_nmi) begin
            unlock_sequence_counter <= 2'd0;
        end else if (!n64_scb.cfg_unlock) begin
            if (reg_bus.write && reg_bus.address[16] && (reg_bus.address[15:5] == 11'd0)) begin
                case (reg_bus.address[4:1])
                    REG_KEY_H, REG_KEY_L: begin
                        for (int index = 0; index < $size(UNLOCK_SEQUENCE); index++) begin
                            if (index == unlock_sequence_counter) begin
                                if (reg_bus.wdata == UNLOCK_SEQUENCE[index]) begin
                                    unlock_sequence_counter <= unlock_sequence_counter + 1'd1;
                                    if (index == ($size(UNLOCK_SEQUENCE) - 1'd1)) begin
                                        unlock_flag <= 1'b1;
                                        unlock_sequence_counter <= 2'd0;
                                    end
                                end else begin
                                    unlock_sequence_counter <= 2'd0;
                                end
                            end
                        end
                    end
                endcase
            end
        end
    end

endmodule
