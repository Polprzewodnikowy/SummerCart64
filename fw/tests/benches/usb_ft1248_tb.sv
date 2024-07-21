module usb_ft1248_tb;

    logic clk;
    logic reset;

    usb_scb usb_scb ();
    fifo_bus fifo_bus ();

    logic usb_pwrsav;
    logic usb_clk;
    logic usb_cs;
    logic usb_miso;
    logic [7:0] usb_miosi;

    usb_ft1248 usb_ft1248 (
        .clk(clk),
        .reset(reset),
        .usb_scb(usb_scb),
        .fifo_bus(fifo_bus),
        .usb_pwrsav(usb_pwrsav),
        .usb_clk(usb_clk),
        .usb_cs(usb_cs),
        .usb_miso(usb_miso),
        .usb_miosi(usb_miosi)
    );

    initial begin
        clk = 1'b0;
        forever begin
            clk = ~clk; #0.5;
        end
    end

    initial begin
        reset = 1'b1;
        #10;
        reset = 1'b0;
    end

    initial begin
        $dumpfile("traces/usb_ft1248_tb.vcd");

        $dumpvars();

        usb_pwrsav = 1'b1;
        usb_miso = 1'b1;

        #100;

        fifo_bus.tx_write = 1'b1;
        #100;
        fifo_bus.tx_write = 1'b0;

        #103;

        usb_miso = 1'b0;
        #80;
        usb_scb.write_buffer_flush = 1'b1;
        #1;
        usb_scb.write_buffer_flush = 1'b0;
        #20;
        usb_miso = 1'b1;
        #26;
        usb_miso = 1'b0;

        #4430;

        usb_miso = 1'b1;
        #13;
        usb_miso = 1'b0;

        #79;

        fifo_bus.rx_read = 1'b1;
        #1;
        fifo_bus.rx_read = 1'b0;

        #10;

        fifo_bus.rx_read = 1'b1;
        #1;
        fifo_bus.rx_read = 1'b0;

        #80;

        fifo_bus.rx_read = 1'b1;
        #1;
        fifo_bus.rx_read = 1'b0;

        #200;

        usb_scb.reset_on_ack = 1'b1;
        #1;
        usb_scb.reset_on_ack = 1'b0;

        #200;

        usb_scb.reset_off_ack = 1'b1;
        #1;
        usb_scb.reset_off_ack = 1'b0;

        #200;

        usb_scb.fifo_flush = 1'b1;
        #1;
        usb_scb.fifo_flush = 1'b0;

        #3000;

        usb_scb.fifo_flush = 1'b1;
        #1;
        usb_scb.fifo_flush = 1'b0;

        #6000;

        $finish;
    end

endmodule
