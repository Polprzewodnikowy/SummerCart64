module memory_flash (
    if_system.sys sys,

    input request,
    output ack,
    input [31:0] address,
    output [15:0] rdata
);

    logic flash_enable;
    logic flash_request;
    logic flash_busy;
    logic flash_ack;
    logic [31:0] flash_rdata;

    logic dummy_ack;

    always_comb begin
        flash_enable = address < 32'h10016800;

        ack = flash_ack | dummy_ack;

        rdata = 16'd0;
        if (ack && flash_enable) begin
            if (address[1]) rdata = {flash_rdata[23:16], flash_rdata[31:24]};
            else rdata = {flash_rdata[7:0], flash_rdata[15:8]};
        end
    end

    typedef enum bit [0:0] { 
        S_IDLE,
        S_WAIT
    } e_state;

    e_state state;

    always_ff @(posedge sys.clk) begin
        dummy_ack <= 1'b1;

        if (sys.reset) begin
            state <= S_IDLE;
            flash_request <= 1'b0;
        end else begin
            case (state)
                S_IDLE: begin
                    if (request) begin
                        state <= S_WAIT;
                        flash_request <= flash_enable;
                        dummy_ack <= !flash_enable;
                    end
                end

                S_WAIT: begin
                    if (!flash_busy) begin
                        flash_request <= 1'b0;
                    end
                    if (ack) begin
                        state <= S_IDLE;
                    end
                end
            endcase
        end
    end

    intel_flash intel_flash_inst (
        .clock(sys.clk),
        .reset_n(~sys.reset),
        .avmm_data_addr(address[31:2]),
        .avmm_data_read(flash_request),
        .avmm_data_readdata(flash_rdata),
        .avmm_data_waitrequest(flash_busy),
        .avmm_data_readdatavalid(flash_ack),
        .avmm_data_burstcount(2'd1)
    );

endmodule
