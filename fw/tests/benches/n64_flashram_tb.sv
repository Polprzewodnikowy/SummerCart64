`include "n64_reg_bus_mock.sv"

module n64_flashram_tb;

    logic clk;
    logic reset;

    n64_reg_bus reg_bus ();
    n64_scb n64_scb ();

    n64_flashram n64_flashram (
        .clk(clk),
        .reset(reset),
        .reg_bus(reg_bus),
        .n64_scb(n64_scb)
    );

    initial begin
        clk = 1'b0;
        forever begin
            clk = ~clk; #0.5;
        end
    end

    task automatic write_halfword (input logic [16:0] address, input logic [15:0] data);
        @(negedge clk);
        reg_bus.address = address;
        reg_bus.wdata = data;
        reg_bus.write = 1'b1;
        @(posedge clk);
        #0.1;
        @(negedge clk);
        reg_bus.write = 1'b0;
    endtask

    task automatic write_command (input logic [31:0] command);
        write_halfword(17'h10000, command[31:16]);
        write_halfword(17'h10002, command[15:0]);
    endtask

    task automatic read_halfword (input logic [16:0] address, output logic [15:0] data);
        @(negedge clk);
        reg_bus.address = address;
        reg_bus.read = 1'b1;
        #0.1;
        data = reg_bus.rdata;
        @(negedge clk);
        reg_bus.read = 1'b0;
    endtask

    task automatic expect_halfword (
        input logic [16:0] address,
        input logic [15:0] expected,
        input string description
    );
        logic [15:0] actual;
        read_halfword(address, actual);
        if (actual !== expected) begin
            $fatal(1, "%s: expected 0x%04X, got 0x%04X", description, expected, actual);
        end
    endtask

    task automatic read_status_sequence (output logic [15:0] status);
        logic [15:0] first_status;
        write_command(32'hD2000000);
        read_halfword(17'h00002, first_status);
        write_command(32'hD2000000);
        read_halfword(17'h00002, status);
        if (first_status !== status) begin
            $fatal(1, "D2 status reads disagree: first 0x%04X, second 0x%04X", first_status, status);
        end
    endtask

    task automatic complete_operation;
        @(negedge clk);
        n64_scb.flashram_done = 1'b1;
        @(posedge clk);
        #0.1;
        @(negedge clk);
        n64_scb.flashram_done = 1'b0;
    endtask

    task automatic expect_pending (input logic expected, input string description);
        if (n64_scb.flashram_pending !== expected) begin
            $fatal(1, "%s: expected pending %b, got %b", description, expected, n64_scb.flashram_pending);
        end
    endtask

    task automatic expect_status_sequence (input logic [15:0] expected, input string description);
        logic [15:0] actual;
        read_status_sequence(actual);
        if (actual !== expected) begin
            $fatal(1, "%s: expected 0x%04X, got 0x%04X", description, expected, actual);
        end
    endtask

    task automatic clear_status;
        write_halfword(17'h00000, 16'h0000);
        write_halfword(17'h00002, 16'h0000);
    endtask

    initial begin
        $dumpfile("traces/n64_flashram_tb.vcd");
        $dumpvars();

        reset = 1'b1;
        reg_bus.read = 1'b0;
        reg_bus.write = 1'b0;
        reg_bus.address = 17'd0;
        reg_bus.wdata = 16'd0;
        n64_scb.flashram_done = 1'b0;

        repeat (2) @(posedge clk);
        @(negedge clk);
        reset = 1'b0;
        @(posedge clk);
        #0.1;

        // Pokémon Puzzle League enters silicon-ID mode, reads the ID, then
        // enters status mode and requires a nonzero low status byte.
        expect_status_sequence(16'h0080, "Idle FlashRAM status");
        expect_halfword(17'h00000, 16'h0000, "Status mode high halfword at base");
        expect_halfword(17'h00004, 16'h0000, "Status mode high halfword alias");
        expect_halfword(17'h00006, 16'h0080, "Status mode low halfword alias");

        // Ignore stale completion pulses while idle.
        complete_operation();
        expect_halfword(17'h00002, 16'h0080, "Idle completion pulse is ignored");

        // Erase execution without setup is ignored.
        write_command(32'h78000000);
        expect_pending(1'b0, "Erase without setup");
        expect_halfword(17'h00002, 16'h0080, "Ignored erase retains WSM-ready status");

        write_command(32'hE1000000);
        expect_halfword(17'h00000, 16'h1111, "FlashRAM type ID high halfword");
        expect_halfword(17'h00002, 16'h8001, "FlashRAM type ID low halfword");
        expect_halfword(17'h00004, 16'h0032, "FlashRAM model ID high halfword");
        expect_halfword(17'h00006, 16'h00F1, "FlashRAM model ID low halfword");

        expect_status_sequence(16'h0080, "Status after E1 identification");

        // Standard libultra flow erases a sector before programming pages.
        write_command(32'h4B000080);
        expect_pending(1'b0, "Sector erase setup");
        expect_halfword(17'h00002, 16'h0080, "Sector erase setup retains WSM-ready status");

        write_command(32'h78000000);
        expect_pending(1'b1, "Sector erase execution");
        expect_halfword(17'h00002, 16'h0002, "Sector erase busy status");

        // Commands and status-clear writes are ignored while busy.
        write_command(32'hE1000000);
        write_halfword(17'h00000, 16'h0000);
        write_halfword(17'h00002, 16'h0000);
        expect_pending(1'b1, "Commands during sector erase");
        expect_halfword(17'h00000, 16'h0000, "Busy command attempt does not enter silicon-ID mode");
        expect_halfword(17'h00002, 16'h0002, "Busy command attempt preserves erase-busy status");

        complete_operation();

        expect_halfword(17'h00002, 16'h0088, "Completed sector erase status");

        // Mode changes do not alter the completed operation status.
        write_command(32'hE1000000);
        expect_halfword(17'h00000, 16'h1111, "Post-erase silicon-ID mode");
        expect_status_sequence(16'h0088, "Completed erase status after E1/D2");

        clear_status();
        expect_halfword(17'h00002, 16'h0080, "Cleared erase completion status");

        // Page programming clears WSM_READY until completion.
        write_command(32'hB4000000);
        expect_pending(1'b0, "Load-byte-page mode");
        expect_halfword(17'h00002, 16'h0080, "Load-byte-page mode retains WSM-ready status");

        write_command(32'hA5000001);
        expect_pending(1'b1, "Page program execution");
        expect_halfword(17'h00002, 16'h0001, "Page program busy status");

        write_command(32'hF0000000);
        write_halfword(17'h00000, 16'h0000);
        write_halfword(17'h00002, 16'h0000);
        expect_pending(1'b1, "Commands during page program");
        expect_halfword(17'h00002, 16'h0001, "Busy command attempt preserves program-busy status");

        complete_operation();

        expect_halfword(17'h00002, 16'h0084, "Completed page program status");

        write_command(32'hE1000000);
        expect_halfword(17'h00000, 16'h1111, "Post-program silicon-ID mode");
        expect_status_sequence(16'h0084, "Completed program status after E1/D2");

        clear_status();
        expect_halfword(17'h00002, 16'h0080, "Cleared status");

        // Chip erase follows the sector-erase readiness lifecycle.
        write_command(32'h3C000000);
        expect_pending(1'b0, "Chip erase setup");
        expect_halfword(17'h00002, 16'h0080, "Chip erase setup retains WSM-ready status");
        write_command(32'h78000000);
        expect_pending(1'b1, "Chip erase execution");
        if (!n64_scb.flashram_sector_or_all) begin
            $fatal(1, "Chip erase did not select a pending full-chip operation");
        end
        expect_halfword(17'h00002, 16'h0002, "Chip erase busy status");

        complete_operation();
        expect_halfword(17'h00002, 16'h0088, "Completed chip erase status");

        clear_status();
        expect_halfword(17'h00002, 16'h0080, "Cleared chip erase status");

        // WSM_READY remains independent of retained completion flags.
        write_command(32'hB4000000);
        write_command(32'hA5000003);
        complete_operation();
        expect_halfword(17'h00002, 16'h0084, "History test completed page program status");

        write_command(32'h4B000100);
        write_command(32'h78000000);
        expect_halfword(17'h00002, 16'h0006, "Erase busy with retained program-success status");
        complete_operation();
        expect_halfword(17'h00002, 16'h008C, "Erase completion with both success flags");

        clear_status();
        expect_halfword(17'h00002, 16'h0080, "Cleared cross-operation status");

        write_command(32'h4B000180);
        write_command(32'h78000000);
        complete_operation();
        expect_halfword(17'h00002, 16'h0088, "History test completed sector erase status");

        write_command(32'hB4000000);
        write_command(32'hA5000004);
        expect_halfword(17'h00002, 16'h0009, "Program busy with retained erase-success status");
        complete_operation();
        expect_halfword(17'h00002, 16'h008C, "Program completion with both success flags");

        clear_status();
        expect_halfword(17'h00002, 16'h0080, "Cleared reverse cross-operation status");

        // Reset during an operation restores idle status.
        write_command(32'hB4000000);
        write_command(32'hA5000002);
        expect_pending(1'b1, "Reset-recovery page program");
        expect_halfword(17'h00002, 16'h0001, "Pre-reset program busy status");

        @(negedge clk);
        reset = 1'b1;
        repeat (2) @(posedge clk);
        @(negedge clk);
        reset = 1'b0;
        @(posedge clk);
        #0.1;
        expect_pending(1'b0, "Reset recovery");
        expect_status_sequence(16'h0080, "Status after reset recovery");

        $display("PASS: n64_flashram WSM-ready lifecycle and edge cases");
        $finish;
    end

endmodule
