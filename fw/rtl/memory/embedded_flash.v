module embedded_flash (
    input i_clk,
    input i_reset,

    input i_request,
    output reg o_busy,
    output o_ack,
    input [18:0] i_address,
    output reg [31:0] o_data
);

    localparam ONCHIP_FLASH_END = 16'h59FF;

    reg r_dummy_ack;

    always @(posedge i_clk) begin
        r_dummy_ack <= i_request && (i_address > ONCHIP_FLASH_END);
    end

    reg r_onchip_flash_request;
    wire w_onchip_flash_busy;
    wire w_onchip_flash_ack;
    wire [31:0] w_onchip_flash_o_data;

    assign o_ack = r_dummy_ack || w_onchip_flash_ack;

    always @(*) begin
        r_onchip_flash_request = 1'b0;
        o_busy = 1'b0;
        o_data = 32'h0000_0000;

        if (i_address <= ONCHIP_FLASH_END) begin
            r_onchip_flash_request = i_request;
            o_busy = w_onchip_flash_busy;
            o_data = w_onchip_flash_o_data;
        end
    end

    onchip_flash onchip_flash_inst (
        .clock(i_clk),
        .reset_n(~i_reset),
        .avmm_data_addr(i_address),
        .avmm_data_read(r_onchip_flash_request),
        .avmm_data_readdata(w_onchip_flash_o_data),
        .avmm_data_waitrequest(w_onchip_flash_busy),
        .avmm_data_readdatavalid(w_onchip_flash_ack),
        .avmm_data_burstcount(2'd1)
    );

endmodule
