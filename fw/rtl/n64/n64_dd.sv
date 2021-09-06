interface if_dd();

    logic hard_reset;
    logic cmd_request;
    logic cmd_ack;
    logic [7:0] command;
    logic [15:0] status;
    logic [15:0] data_input;
    logic [15:0] data_output;
    logic bm_request;
    logic [15:0] bm_control;
    logic [15:0] bm_status;

    modport n64 (
        output hard_reset,
        output cmd_request,
        input cmd_ack,
        output command,
        input status,
        output data_input,
        input data_output,
        output bm_request,
        output bm_control,
        input bm_status
    );

    modport cpu (
        input hard_reset,
        input cmd_request,
        output cmd_ack,
        input command,
        output status,
        input data_input,
        output data_output,
        input bm_request,
        input bm_control,
        output bm_status
    );

endinterface


module n64_dd (
    if_system.sys sys,
    if_n64_bus bus
);

    typedef enum bit [0:0] {
        S_IDLE,
        S_WAIT
    } e_state;

    e_state state;

    always_comb begin
        bus.rdata = 16'h0000;
        if (bus.ack) begin
            bus.rdata = !bus.address[1] ? 16'h0040 : 16'h0000;
        end
    end

    always_ff @(posedge sys.clk) begin
        bus.ack <= 1'b0;

        if (sys.reset) begin
            state <= S_IDLE;
        end else begin            
            case (state)
                S_IDLE: begin
                    if (bus.request) begin
                        state <= S_WAIT;
                        bus.ack <= 1'b1;
                    end
                end

                S_WAIT: begin
                    state <= S_IDLE;
                end
            endcase
        end
    end

endmodule
