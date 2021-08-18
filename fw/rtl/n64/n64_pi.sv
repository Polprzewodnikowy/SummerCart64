module n64_pi (
    if_system.sys sys,

    input n64_pi_alel,
    input n64_pi_aleh,
    input n64_pi_read,
    input n64_pi_write,
    inout [15:0] n64_pi_ad,

    output request,
    input ack,
    output write,
    output [31:0] address,
    output [15:0] wdata,
    input [15:0] rdata
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
    logic pi_write_delayed;

    always_comb begin
        pi_reset = sys.n64_hard_reset;
        pi_aleh = n64_pi_aleh_ff[2];
        pi_alel = n64_pi_alel_ff[2];
        pi_read = n64_pi_read_ff[1];
        pi_read_delayed = n64_pi_read_ff[2];
        pi_write = n64_pi_write_ff[1];
        pi_write_delayed = n64_pi_write_ff[2];
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

    always_comb begin
        n64_pi_ad = n64_pi_ad_output_enable ? n64_pi_ad_output : 16'hZZZZ;
        n64_pi_ad_output_enable_data = !pi_reset && pi_mode == PI_MODE_VALID && !pi_read_delayed;
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

    always_ff @(posedge sys.clk) begin
        request <= 1'b0;

        if (sys.reset || sys.n64_hard_reset || sys.n64_soft_reset) begin
            state <= S_IDLE;
            first_operation <= 1'b0;
            pending_operation <= 1'b0;
        end else begin
            case (state)
                S_IDLE: begin
                    if (aleh_op) address[31:16] <= n64_pi_ad_input;
                    if (alel_op) address[15:0] <= {n64_pi_ad_input[15:1], 1'b0};
                    if (alel_op || read_op || write_op || pending_operation) begin
                        state <= S_WAIT;
                        request <= 1'b1;
                        write <= write_op || (pending_operation && pending_write);
                        if (!alel_op && !(first_operation && write_op)) begin
                            address[31:1] <= address[31:1] + 1'd1;
                        end
                        wdata <= n64_pi_ad_input;
                        first_operation <= alel_op;
                    end
                end

                S_WAIT: begin
                    if (ack) begin
                        state <= S_IDLE;
                        n64_pi_ad_output_data_buffer <= rdata;
                    end
                    if (read_op || write_op) begin
                        pending_operation <= 1'b1;
                        pending_write <= write_op;
                    end
                end

                default: begin
                    state <= S_IDLE;
                    first_operation <= 1'b0;
                    pending_operation <= 1'b0;
                end
            endcase
        end
    end

endmodule
