module sd_clk (
    input i_clk,
    input i_reset,

    output reg o_sd_clk,

    input [1:0] i_sd_clk_config,

    output reg o_sd_clk_strobe_rising,
    output reg o_sd_clk_strobe_falling
);

    // Clock configuration values

    localparam [1:0] SD_CLK_CONFIG_STOP     = 2'd0;
    localparam [1:0] SD_CLK_CONFIG_DIV_256  = 2'd1;
    localparam [1:0] SD_CLK_CONFIG_DIV_4    = 2'd2;
    localparam [1:0] SD_CLK_CONFIG_DIV_2    = 2'd3;


    // Clock configuration change detection

    reg [1:0] r_prev_sd_clk_config;

    wire w_sd_clk_config_changed = r_prev_sd_clk_config != i_sd_clk_config;

    always @(posedge i_clk) begin
        if (i_reset) begin
            r_prev_sd_clk_config <= SD_CLK_CONFIG_STOP;
        end else begin
            r_prev_sd_clk_config <= i_sd_clk_config;
        end
    end


    // Clock divider

    reg [7:0] r_sd_clk_counter;

    always @(posedge i_clk) begin
        if (i_reset || w_sd_clk_config_changed) begin
            r_sd_clk_counter <= 8'd0;
        end else begin
            r_sd_clk_counter <= r_sd_clk_counter + 1'd1;
        end
    end


    // Clock divider selector

    reg r_selected_sd_clk;

    always @(*) begin
        case (r_prev_sd_clk_config)
            SD_CLK_CONFIG_STOP:
                r_selected_sd_clk = 1'b0;
            SD_CLK_CONFIG_DIV_256:
                r_selected_sd_clk = r_sd_clk_counter[7];
            SD_CLK_CONFIG_DIV_4:
                r_selected_sd_clk = r_sd_clk_counter[1];
            SD_CLK_CONFIG_DIV_2:
                r_selected_sd_clk = r_sd_clk_counter[0];
        endcase
    end


    // Clock strobe generation

    reg r_prev_selected_sd_clk;

    always @(posedge i_clk) begin
        o_sd_clk_strobe_rising <= 1'b0;
        o_sd_clk_strobe_falling <= 1'b0;

        if (i_reset) begin
            r_prev_selected_sd_clk <= 1'b0;
        end else begin
            r_prev_selected_sd_clk <= r_selected_sd_clk;
            
            if (!r_prev_selected_sd_clk && r_selected_sd_clk) begin
                o_sd_clk_strobe_rising <= 1'b1;
            end

            if (r_prev_selected_sd_clk && !r_selected_sd_clk) begin
                o_sd_clk_strobe_falling <= 1'b1;
            end
        end
    end


    // SD clock generation

    always @(posedge i_clk) begin
        if (i_reset) begin
            o_sd_clk <= 1'b0;
        end else begin
            if (o_sd_clk_strobe_rising) begin
                o_sd_clk <= 1'b1;
            end

            if (o_sd_clk_strobe_falling) begin
                o_sd_clk <= 1'b0;
            end
        end
    end

endmodule
