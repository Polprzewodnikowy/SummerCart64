////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	wbsdram.v
//
// Project:	ArrowZip, a demonstration of the Arrow MAX1000 FPGA board
//
// Purpose:	Provide 32-bit wishbone access to the SDRAM memory on a MAX1000
//		board.  Specifically, on each access, the controller will
//	activate an appropriate bank of RAM (the SDRAM has four banks), and
//	then issue the read/write command.  In the case of walking off the
//	bank, the controller will activate the next bank before you get to it.
//	Upon concluding any wishbone access, all banks will be precharged and
//	returned to idle.
//
//	This particular implementation represents a second generation version
//	because my first version was too complex.  To speed things up, this
//	version includes an extra wait state where the wishbone inputs are
//	clocked into a flip flop before any action is taken on them.
//
// Creator:	Dan Gisselquist, Ph.D.
//		Gisselquist Technology, LLC
//
////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2015-2019, Gisselquist Technology, LLC
//
// This program is free software (firmware): you can redistribute it and/or
// modify it under the terms of  the GNU General Public License as published
// by the Free Software Foundation, either version 3 of the License, or (at
// your option) any later version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTIBILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
// for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program.  (It's in the $(ROOT)/doc directory.  Run make with no
// target there if the PDF file isn't present.)  If not, see
// <http://www.gnu.org/licenses/> for a copy.
//
// License:	GPL, v3, as defined and found on www.gnu.org,
//		http://www.gnu.org/licenses/gpl.html
//
//
////////////////////////////////////////////////////////////////////////////////
//
//
`default_nettype	none
//
`define	DMOD_GETINPUT	1'b0
`define	DMOD_PUTOUTPUT	1'b1
`define	RAM_OPERATIONAL	2'b00
`define	RAM_POWER_UP	2'b01
`define	RAM_SET_MODE	2'b10
`define	RAM_INITIAL_REFRESH	2'b11
//
module	wbsdram(i_clk,
		i_wb_cyc, i_wb_stb, i_wb_we, i_wb_addr, i_wb_data, i_wb_sel,
			o_wb_ack, o_wb_stall, o_wb_data,
		o_ram_cs_n, o_ram_cke, o_ram_ras_n, o_ram_cas_n, o_ram_we_n,
			o_ram_bs, o_ram_addr,
			o_ram_dmod, i_ram_data, o_ram_data, o_ram_dqm,
		o_debug);
	parameter	RDLY = 6;
	localparam	NCA=10, NRA=13, AW=(NCA+NRA+2)-1, DW=32;
	localparam	[NCA-2:0] COL_THRESHOLD = -16;
	input	wire			i_clk;
	// Wishbone
	//	inputs
	input	wire			i_wb_cyc, i_wb_stb, i_wb_we;
	input	wire	[(AW-1):0]	i_wb_addr;
	input	wire	[(DW-1):0]	i_wb_data;
	input	wire	[(DW/8-1):0]	i_wb_sel;
	//	outputs
	output	wire		o_wb_ack;
	output	reg		o_wb_stall;
	output	wire [31:0]	o_wb_data;
	// SDRAM control
	output	wire		o_ram_cke;
	output	reg		o_ram_cs_n,
				o_ram_ras_n, o_ram_cas_n, o_ram_we_n;
	output	reg	[1:0]	o_ram_bs;
	output	reg	[12:0]	o_ram_addr;
	output	reg		o_ram_dmod;
	input		[15:0]	i_ram_data;
	output	reg	[15:0]	o_ram_data;
	output	reg	[1:0]	o_ram_dqm;
	output	wire [(DW-1):0]	o_debug;


	// Calculate some metrics

	//
	// First, do we *need* a refresh now --- i.e., must we break out of
	// whatever we are doing to issue a refresh command?
	//
	// The step size here must be such that 8192 charges may be done in
	// 64 ms.  Thus for a clock of:
	//	ClkRate(MHz)	(64ms/1000(ms/s)*ClkRate)/8192
	//	100 MHz		781
	//	 96 MHz		750
	//	 92 MHz		718
	//	 88 MHz		687
	//	 84 MHz		656
	//	 80 MHz		625
	//
	// However, since we do two refresh cycles everytime we need a refresh,
	// this standard is close to overkill--but we'll use it anyway.  At
	// some later time we should address this, once we are entirely
	// convinced that the memory is otherwise working without failure.  Of
	// course, at that time, it may no longer be a priority ...
	//
	reg		need_refresh;
	reg	[9:0]	refresh_clk;
	wire	refresh_cmd;
	assign	refresh_cmd = (!o_ram_cs_n)&&(!o_ram_ras_n)&&(!o_ram_cas_n)&&(o_ram_we_n);
	initial	refresh_clk = 0;
	always @(posedge i_clk)
	begin
		if (refresh_cmd)
			refresh_clk <= 10'd703; // Make suitable for 90 MHz clk
		else if (|refresh_clk)
			refresh_clk <= refresh_clk - 10'h1;
	end
	initial	need_refresh = 1'b0;
	always @(posedge i_clk)
		need_refresh <= (refresh_clk == 10'h00)&&(!refresh_cmd);

	reg	in_refresh;
	reg	[2:0]	in_refresh_clk;
	initial	in_refresh_clk = 3'h0;
	always @(posedge i_clk)
		if (refresh_cmd)
			in_refresh_clk <= 3'h6;
		else if (|in_refresh_clk)
			in_refresh_clk <= in_refresh_clk - 3'h1;
	initial	in_refresh = 0;
	always @(posedge i_clk)
		in_refresh <= (in_refresh_clk != 3'h0)||(refresh_cmd);
`ifdef	FORMAL
	always @(posedge i_clk)
		if (in_refresh)
			assert((refresh_cmd)||($past(in_refresh_clk) <= 3'h6));
	always @(posedge i_clk)
		if (in_refresh)
			assert(refresh_clk==10'd619+{{(7){1'b0}},in_refresh_clk});
`endif


	reg	[2:0]		bank_active	[0:3];
	reg	[(RDLY-1):0]	r_barrell_ack;
	reg			r_pending;
	reg			r_we;
	reg	[(AW-1):0]	r_addr;
	reg	[31:0]		r_data;
	reg	[3:0]		r_sel;
	reg	[(AW-NCA-2):0]	bank_row	[0:3];


	//
	// Second, do we *need* a precharge now --- must be break out of
	// whatever we are doing to issue a precharge command?
	//
	// Keep in mind, the number of clocks to wait has to be reduced by
	// the amount of time it may take us to go into a precharge state.
	// You may also notice that the precharge requirement is tighter
	// than this one, so ... perhaps this isn't as required?
	//

	reg	[2:0]	clocks_til_idle;
	reg	[1:0]	m_state;
	wire		bus_cyc;
	assign	bus_cyc  = ((i_wb_cyc)&&(i_wb_stb)&&(!o_wb_stall));
	reg	nxt_dmod;

	// Pre-process pending operations
	wire	pending;
	initial	r_pending = 1'b0;
	reg	[(AW-1):0]	fwd_addr;
	initial	r_addr = 0;
	initial	fwd_addr = { {(AW-(NCA)){1'b0}}, 1'b1, {(NCA-1){1'b0}} };
	always @(posedge i_clk)
	begin
		fwd_addr[NCA-2:0] <= 0;
		if (bus_cyc)
		begin
			r_pending <= 1'b1;
			r_we      <= i_wb_we;
			r_addr    <= i_wb_addr;
			r_data    <= i_wb_data;
			r_sel     <= i_wb_sel;
			fwd_addr[AW-1:NCA-1]<=i_wb_addr[(AW-1):(NCA-1)] + 1'b1;
		end else if ((!o_ram_cs_n)&&(o_ram_ras_n)&&(!o_ram_cas_n))
			r_pending <= 1'b0;
		else if (!i_wb_cyc)
			r_pending <= 1'b0;
	end

`ifdef	FORMAL
	always @(*)
		assert(fwd_addr[AW-1:NCA-1] == r_addr[(AW-1):(NCA-1)] + 1'b1);
	always @(*)
		assert(fwd_addr[NCA-3:0] == 0);
`endif

	wire	[1:0]	wb_bs, r_bs, fwd_bs;	// Bank select
	assign	wb_bs = i_wb_addr[NCA:NCA-1];
	assign	r_bs  =    r_addr[NCA:NCA-1];
	assign fwd_bs =  fwd_addr[NCA:NCA-1];
	wire	[NRA-1:0]	wb_row, r_row, fwd_row;
	assign	wb_row = i_wb_addr[AW-1:NCA+1];
	assign	 r_row =    r_addr[AW-1:NCA+1];
	assign fwd_row =  fwd_addr[AW-1:NCA+1];

	reg	r_bank_valid;
	initial	r_bank_valid = 1'b0;
	always @(posedge i_clk)
		if (bus_cyc)
			r_bank_valid <=((bank_active[wb_bs][2])
					&&(bank_row[wb_bs] == wb_row));
		else
			r_bank_valid <= ((bank_active[r_bs][2])
				&&(bank_row[r_bs] == r_row));

	reg	fwd_bank_valid;
	initial	fwd_bank_valid = 0;
	always @(posedge i_clk)
		fwd_bank_valid <= ((bank_active[fwd_bs][2])
				&&(bank_row[fwd_bs] == fwd_row));

	assign	pending = (r_pending)&&(o_wb_stall);

	//
	//
	// Maintenance mode (i.e. startup) wires and logic
	reg	maintenance_mode;
	reg	m_ram_cs_n, m_ram_ras_n, m_ram_cas_n, m_ram_we_n, m_ram_dmod;
	reg	[(NRA-1):0]	m_ram_addr;
	//
	//
	//

	// Address MAP:
	//	22-bits bits in, 23-bits out
	//
	//	22 1111 1111 1100 0000 0000
	//	10 9876 5432 1098 7654 3210
	//	rr rrrr rrrr rrBB cccc cccc 0
	//	                  8765 4321 0
	//
	initial r_barrell_ack = 0;
	initial	clocks_til_idle = 3'h0;
	initial o_wb_stall = 1'b1;
	initial	o_ram_dmod = `DMOD_GETINPUT;
	initial	nxt_dmod = `DMOD_GETINPUT;
	initial o_ram_cs_n  = 1'b0;
	initial o_ram_ras_n = 1'b1;
	initial o_ram_cas_n = 1'b1;
	initial o_ram_we_n  = 1'b1;
	initial	o_ram_dqm   = 2'b11;
	assign	o_ram_cke   = 1'b1;
	initial bank_active[0] = 3'b000;
	initial bank_active[1] = 3'b000;
	initial bank_active[2] = 3'b000;
	initial bank_active[3] = 3'b000;
	always @(posedge i_clk)
	if (maintenance_mode)
	begin
		bank_active[0] <= 0;
		bank_active[1] <= 0;
		bank_active[2] <= 0;
		bank_active[3] <= 0;
		r_barrell_ack[(RDLY-1):0] <= 0;
		o_wb_stall  <= 1'b1;
		//
		o_ram_cs_n  <= m_ram_cs_n;
		o_ram_ras_n <= m_ram_ras_n;
		o_ram_cas_n <= m_ram_cas_n;
		o_ram_we_n  <= m_ram_we_n;
		o_ram_dmod  <= m_ram_dmod;
		o_ram_addr  <= m_ram_addr;
		o_ram_bs    <= 2'b00;
		nxt_dmod <= `DMOD_GETINPUT;
	end else begin
		o_wb_stall <= (r_pending)||(bus_cyc);
		if (!i_wb_cyc)
			r_barrell_ack <= 0;
		else
			r_barrell_ack <= r_barrell_ack >> 1;
		nxt_dmod <= `DMOD_GETINPUT;
		o_ram_dmod <= nxt_dmod;

		//
		// We assume that, whatever state the bank is in, that it
		// continues in that state and set up a series of shift
		// registers to contain that information.  If it will not
		// continue in that state, all that therefore needs to be
		// done is to set bank_active[?][2] below.
		//
		bank_active[0] <= { bank_active[0][2], bank_active[0][2:1] };
		bank_active[1] <= { bank_active[1][2], bank_active[1][2:1] };
		bank_active[2] <= { bank_active[2][2], bank_active[2][2:1] };
		bank_active[3] <= { bank_active[3][2], bank_active[3][2:1] };
		//
		o_ram_cs_n <= (!i_wb_cyc);
		// o_ram_cke  <= 1'b1;
		if (|clocks_til_idle[2:0])
			clocks_til_idle[2:0] <= clocks_til_idle[2:0] - 3'h1;

		// Default command is a
		//	NOOP if (i_wb_cyc)
		//	Device deselect if (!i_wb_cyc)
		// o_ram_cs_n  <= (!i_wb_cyc) above, NOOP
		o_ram_ras_n <= 1'b1;
		o_ram_cas_n <= 1'b1;
		o_ram_we_n  <= 1'b1;

		// o_ram_data <= r_data[15:0];

		if (nxt_dmod)
			;
		else
		if ((!i_wb_cyc)||(need_refresh))
		begin // Issue a precharge all command (if any banks are open),
		// otherwise an autorefresh command
			if ((bank_active[0][2:1]==2'b10)
					||(bank_active[1][2:1]==2'b10)
					||(bank_active[2][2:1]==2'b10)
					||(bank_active[3][2:1]==2'b10)
				||(|clocks_til_idle[2:0]))
			begin
				// Do nothing this clock
				// Can't precharge a bank immediately after
				// activating it
			end else if (bank_active[0][2]
				||(bank_active[1][2])
				||(bank_active[2][2])
				||(bank_active[3][2]))
			begin  // Close all active banks
				o_ram_cs_n  <= 1'b0;
				o_ram_ras_n <= 1'b0;
				o_ram_cas_n <= 1'b1;
				o_ram_we_n  <= 1'b0;
				o_ram_addr[10] <= 1'b1;
				bank_active[0][2] <= 1'b0;
				bank_active[1][2] <= 1'b0;
				bank_active[2][2] <= 1'b0;
				bank_active[3][2] <= 1'b0;
			end else if ((|bank_active[0])
					||(|bank_active[1])
					||(|bank_active[2])
					||(|bank_active[3]))
				// Can't precharge yet, the bus is still busy
			begin end else if ((!in_refresh)&&((refresh_clk[9:8]==2'b00)||(need_refresh)))
			begin // Send autorefresh command
				o_ram_cs_n  <= 1'b0;
				o_ram_ras_n <= 1'b0;
				o_ram_cas_n <= 1'b0;
				o_ram_we_n  <= 1'b1;
			end // Else just send NOOP's, the default command
		end else if (in_refresh)
		begin
			// NOOPS only here, until we are out of refresh
		end else if ((pending)&&(!r_bank_valid)&&(bank_active[r_bs]==3'h0))
		begin // Need to activate the requested bank
			o_ram_cs_n  <= 1'b0;
			o_ram_ras_n <= 1'b0;
			o_ram_cas_n <= 1'b1;
			o_ram_we_n  <= 1'b1;
			o_ram_addr  <= r_row;
			o_ram_bs    <= r_bs;
			// clocks_til_idle[2:0] <= 1;
			bank_active[r_bs][2] <= 1'b1;
			bank_row[r_bs] <= r_row;
			//
		end else if ((pending)&&(!r_bank_valid)
				&&(bank_active[r_bs]==3'b111))
		begin // Need to close an active bank
			o_ram_cs_n  <= 1'b0;
			o_ram_ras_n <= 1'b0;
			o_ram_cas_n <= 1'b1;
			o_ram_we_n  <= 1'b0;
			// o_ram_addr  <= r_addr[(AW-1):(NCA+2)];
			o_ram_addr[10]<= 1'b0;
			o_ram_bs    <= r_bs;
			// clocks_til_idle[2:0] <= 1;
			bank_active[r_bs][2] <= 1'b0;
			// bank_row[r_bs] <= r_row;
		end else if ((pending)&&(!r_we)
				&&(bank_active[r_bs][2])
				&&(r_bank_valid)
				&&(clocks_til_idle[2:0] < 4))
		begin // Issue the read command
			o_ram_cs_n  <= 1'b0;
			o_ram_ras_n <= 1'b1;
			o_ram_cas_n <= 1'b0;
			o_ram_we_n  <= 1'b1;
			o_ram_addr  <= { 4'h0, r_addr[NCA-2:0], 1'b0 };
			o_ram_bs    <= r_bs;
			clocks_til_idle[2:0] <= 4;

			o_wb_stall <= 1'b0;
			r_barrell_ack[(RDLY-1)] <= 1'b1;
		end else if ((pending)&&(r_we)
			&&(bank_active[r_bs][2])
			&&(r_bank_valid)
			&&(clocks_til_idle[2:0] == 0))
		begin // Issue the write command
			o_ram_cs_n  <= 1'b0;
			o_ram_ras_n <= 1'b1;
			o_ram_cas_n <= 1'b0;
			o_ram_we_n  <= 1'b0;
			o_ram_addr  <= { 4'h0, r_addr[NCA-2:0], 1'b0 };
			o_ram_bs    <= r_bs;
			clocks_til_idle[2:0] <= 3'h1;

			o_wb_stall <= 1'b0;
			r_barrell_ack[1] <= 1'b1;
			// o_ram_data <= r_data[31:16];
			//
			o_ram_dmod <= `DMOD_PUTOUTPUT;
			nxt_dmod <= `DMOD_PUTOUTPUT;
		end else if ((r_pending)&&(r_addr[(NCA-2):0] >= COL_THRESHOLD)
				&&(!fwd_bank_valid))
		begin
			// Do I need to close the next bank I'll need?
			if (bank_active[fwd_bs][2:1]==2'b11)
			begin // Need to close the bank first
				o_ram_cs_n <= 1'b0;
				o_ram_ras_n <= 1'b0;
				o_ram_cas_n <= 1'b1;
				o_ram_we_n  <= 1'b0;
				o_ram_addr[10] <= 1'b0;
				o_ram_bs       <= fwd_bs;
				bank_active[fwd_bs][2] <= 1'b0;
			end else if (bank_active[fwd_bs]==0)
			begin
				// Need to (pre-)activate the next bank
				o_ram_cs_n  <= 1'b0;
				o_ram_ras_n <= 1'b0;
				o_ram_cas_n <= 1'b1;
				o_ram_we_n  <= 1'b1;
				o_ram_addr  <= fwd_row;
				o_ram_bs    <= fwd_bs;
				// clocks_til_idle[3:0] <= 1;
				bank_active[fwd_bs] <= 3'h4;
				bank_row[fwd_bs] <= fwd_row;
			end
		end
		if (!i_wb_cyc)
			r_barrell_ack <= 0;
	end

	reg		startup_hold;
	reg	[15:0]	startup_idle;
	initial	startup_idle = 16'd20500;
	initial	startup_hold = 1'b1;
	always @(posedge i_clk)
		if (|startup_idle)
			startup_idle <= startup_idle - 1'b1;
	always @(posedge i_clk)
		startup_hold <= |startup_idle;
`ifdef	FORMAL
	always @(*)
		if (startup_hold)
			assert(maintenance_mode);
	always @(*)
		if (|startup_idle)
			assert(startup_hold);
`endif

	reg	[3:0]	maintenance_clocks;
	reg		maintenance_clocks_zero;
	initial	maintenance_mode = 1'b1;
	initial	maintenance_clocks = 4'hf;
	initial	maintenance_clocks_zero = 1'b0;
	initial	m_ram_addr  = { 2'b00, 1'b0, 2'b00, 3'b010, 1'b0, 3'b001 };
	initial	m_state = `RAM_POWER_UP;
	initial	m_ram_cs_n  = 1'b1;
	initial	m_ram_ras_n = 1'b1;
	initial	m_ram_cas_n = 1'b1;
	initial	m_ram_we_n  = 1'b1;
	initial	m_ram_dmod  = `DMOD_GETINPUT;
	always @(posedge i_clk)
	begin
		if (!maintenance_clocks_zero)
		begin
			maintenance_clocks <= maintenance_clocks - 4'h1;
			maintenance_clocks_zero <= (maintenance_clocks == 4'h1);
		end
		// The only time the RAM address matters is when we set
		// the mode.  At other times, addr[10] matters, but the rest
		// is ignored.  Hence ... we'll set it to a constant.
		m_ram_addr  <= { 2'b00, 1'b0, 2'b00, 3'b010, 1'b0, 3'b001 };
		if (m_state == `RAM_POWER_UP)
		begin
			// All signals must be held in NOOP state during powerup
			// m_ram_cke <= 1'b1;
			m_ram_cs_n  <= 1'b1;
			m_ram_ras_n <= 1'b1;
			m_ram_cas_n <= 1'b1;
			m_ram_we_n  <= 1'b1;
			m_ram_dmod  <= `DMOD_GETINPUT;
			if (!startup_hold)
			begin
				m_state <= `RAM_SET_MODE;
				maintenance_clocks <= 4'h3;
				maintenance_clocks_zero <= 1'b0;
				// Precharge all cmd
				m_ram_cs_n  <= 1'b0;
				m_ram_ras_n <= 1'b0;
				m_ram_cas_n <= 1'b1;
				m_ram_we_n  <= 1'b0;
				m_ram_addr[10] <= 1'b1;
			end
		end else if (m_state == `RAM_SET_MODE)
		begin
			// Wait
			m_ram_cs_n     <= 1'b1;
			m_ram_cs_n     <= 1'b1;
			m_ram_ras_n    <= 1'b1;
			m_ram_cas_n    <= 1'b1;
			m_ram_we_n     <= 1'b1;
			m_ram_addr[10] <= 1'b1;

			if (maintenance_clocks_zero)
			begin
				// Set mode cycle
				m_ram_cs_n  <= 1'b0;
				m_ram_ras_n <= 1'b0;
				m_ram_cas_n <= 1'b0;
				m_ram_we_n  <= 1'b0;
				m_ram_dmod  <= `DMOD_GETINPUT;
				m_ram_addr[10] <= 1'b0;

				m_state <= `RAM_INITIAL_REFRESH;
				maintenance_clocks <= 4'hc;
				maintenance_clocks_zero <= 1'b0;
			end
		end else if (m_state == `RAM_INITIAL_REFRESH)
		begin
			// Refresh command
			if (maintenance_clocks > 4'ha)
				// Wait two clocks first
				m_ram_cs_n <= 1'b1;
			else if (maintenance_clocks > 4'h2)
				m_ram_cs_n  <= 1'b0;
			else
				m_ram_cs_n <= 1'b1;
			m_ram_ras_n <= 1'b0;
			m_ram_cas_n <= 1'b0;
			m_ram_we_n  <= 1'b1;
			m_ram_dmod  <= `DMOD_GETINPUT;
			// m_ram_addr  <= { 3'b000, 1'b0, 2'b00, 3'b010, 1'b0, 3'b001 };
			if (maintenance_clocks_zero)
				maintenance_mode <= 1'b0;
		end
	end

	always @(posedge i_clk)
	if (nxt_dmod)
		o_ram_data <= r_data[15:0];
	else
		o_ram_data <= r_data[31:16];

	always @(posedge i_clk)
	if (maintenance_mode)
		o_ram_dqm <= 2'b11;
	else if (r_we)
	begin
		if (nxt_dmod)
			o_ram_dqm <= ~r_sel[1:0];
		else
			o_ram_dqm <= ~r_sel[3:2];
	end else
		o_ram_dqm <= 2'b00;

`ifdef	VERILATOR
	// While I hate to build something that works one way under Verilator
	// and another way in practice, this really isn't that.  The problem
	// \/erilator is having is resolved in toplevel.v---one file that
	// \/erilator doesn't implement.  In toplevel.v, there's not only a
	// single clocked latch but two taking place.  Here, we replicate one
	// of those.  The second takes place (somehow) within the sdramsim.cpp
	// file.
	reg	[15:0]	ram_data, last_ram_data;
	always @(posedge i_clk)
		ram_data <= i_ram_data;
	always @(posedge i_clk)
		last_ram_data <= ram_data;
	assign	o_wb_data = { last_ram_data, ram_data };
`else
	reg	[15:0]	last_ram_data;
	always @(posedge i_clk)
		last_ram_data <= i_ram_data;
	assign	o_wb_data = { last_ram_data, i_ram_data };
`endif
	assign	o_wb_ack  = r_barrell_ack[0];

	//
	// The following outputs are not necessary for the functionality of
	// the SDRAM, but they can be used to feed an external "scope" to
	// get an idea of what the internals of this SDRAM are doing.
	//
	// Just be aware of the r_we: it is set based upon the currently pending
	// transaction, or (if none is pending) based upon the last transaction.
	// If you want to capture the first value "written" to the device,
	// you'll need to write a nothing value to the device to set r_we.
	// The first value "written" to the device can be caught in the next
	// interaction after that.
	//
	reg	trigger;
	always @(posedge i_clk)
		trigger <= ((o_wb_data[15:0]==o_wb_data[31:16])
			&&(o_wb_ack)&&(!i_wb_we));


	assign	o_debug = { i_wb_cyc, i_wb_stb, i_wb_we, o_wb_ack, o_wb_stall, // 5
		o_ram_cs_n, o_ram_ras_n, o_ram_cas_n, o_ram_we_n, o_ram_bs,//6
			o_ram_dmod, r_pending, 				//  2
			trigger,					//  1
			o_ram_addr[9:0],				// 10 more
			(r_we) ? { o_ram_data[7:0] }			//  8 values
				: { o_wb_data[23:20], o_wb_data[3:0] }
			// i_ram_data[7:0]
			 };

	// Make Verilator happy
	// verilator lint_off UNUSED
	wire	[NCA-1:0]	unused;
	assign	unused = { fwd_addr[NCA-1:0] };
	// verilator lint_on  UNUSED
`ifdef	FORMAL
	localparam	REFRESH_CLOCKS = 6;
	localparam	ACTIVATE_CLOCKS = 6;

	// This device is 23MB, assert such
	always @(*)
		assert(AW == 21);
	always @(*)
		assert(NRA+NCA+2 == AW+1);

	wire	[(5-1):0]	f_nreqs, f_nacks, f_outstanding;
	reg	f_past_valid;
	wire	f_reset;

	always @(*)
		if (o_ram_dmod)
			assume(i_ram_data == o_ram_data);

	initial	f_past_valid = 1'b0;
	always @(posedge i_clk)
		f_past_valid <= 1'b1;

	assign	f_reset = !f_past_valid;

	always @(*)
		if (o_ram_dmod)
			assert(i_ram_data == o_ram_data);

	// Properties
	// 1. Wishbone
	fwb_slave #( .AW(AW), .DW(DW),
			.F_MAX_STALL(ACTIVATE_CLOCKS + REFRESH_CLOCKS
					+ ACTIVATE_CLOCKS + RDLY
					+ACTIVATE_CLOCKS),
			.F_MAX_ACK_DELAY(REFRESH_CLOCKS
				+ ACTIVATE_CLOCKS
				+ ACTIVATE_CLOCKS
				+ ACTIVATE_CLOCKS+RDLY),
			.F_LGDEPTH(5))
		fwb(i_clk, f_reset,
			i_wb_cyc, i_wb_stb, i_wb_we, i_wb_addr,
				i_wb_data, i_wb_sel,
			o_wb_ack, o_wb_stall, o_wb_data, 1'b0,
			f_nreqs, f_nacks, f_outstanding);

	// 2. Proper startup ...
	// 3. Operation
	//   4. Refresh
	//   4. SDRAM request == WB request
	//

	// Once we leave maintenance mode (i.e. startup sequence), we *cannot*
	// go back into it.
	always @(posedge i_clk)
	if ((f_past_valid)&&(!$past(maintenance_mode)))
		assert(!maintenance_mode);

	// On the very first clock, we must always start up in maintenance mode
	always @(posedge i_clk)
		if (!f_past_valid)
			assert(maintenance_mode);

	// Just to make things simpler, assume no accesses to the core during
	// maintenance mode.  Such accesses might violate our minimum
	// acknowledgement time criteria for the wishbone above
	always @(posedge i_clk)
	if ((f_past_valid)&&(maintenance_mode))
		assume(!i_wb_stb);

	// Likewise, assert that there are *NO* outstanding transactions in
	// this maintenance mode
	always @(posedge i_clk)
	if ((f_past_valid)&&(maintenance_mode))
		assert(f_outstanding == 0);

	// ... and that while we are in maintenance mode, any incoming request
	// is stalled.  This guarantees that our assumptions above are kept
	// valid.
	always @(posedge i_clk)
	if ((f_past_valid)&&(maintenance_mode))
		assume(o_wb_stall);

	// If there are no attempts to access memory while in maintenance
	// mode, then there should never be any pending operations upon
	// completion of maintenance mode
	always @(posedge i_clk)
	if ((f_past_valid)&&(maintenance_mode))
		assert(!r_pending);

	wire	[(2+AW+DW+DW/8-1):0]	f_pending, f_request;
	assign	f_pending = { r_pending, r_we, r_addr, r_data, r_sel };
	assign	f_request = {  i_wb_stb, i_wb_we, i_wb_addr, i_wb_data, i_wb_sel };

	always @(posedge i_clk)
	if ((f_past_valid)&&($past(r_pending))&&($past(i_wb_cyc))
			&&(($past(o_ram_cs_n))
			||(!$past(o_ram_ras_n))
			||($past(o_ram_cas_n))) )
		assert($stable(f_pending));

	wire	[4:0]	f_cmd;
	assign	f_cmd = { o_ram_addr[10],
			o_ram_cs_n, o_ram_ras_n, o_ram_cas_n, o_ram_we_n };

`define	F_MODE_SET		5'b?0000
`define	F_BANK_PRECHARGE	5'b00010
`define	F_PRECHARGE_ALL		5'b10010
`define	F_BANK_ACTIVATE		5'b?0011
`define	F_WRITE			5'b00100
`define	F_READ			5'b00101
`define	F_REFRESH		5'b?0001
`define	F_NOOP			5'b?0111

`define	F_BANK_ACTIVATE_S	4'b0011
`define	F_REFRESH_S		4'b0001
`define	F_NOOP_S		4'b0111

	reg	[(AW-1):0]	f_next_addr;
	always @(*)
	begin
		f_next_addr = 0;
		f_next_addr[(AW-1):NCA-1] = r_addr[(AW-1):NCA-1] + 1'b1;
	end

	wire	[NRA-1:0]	f_next_row, f_this_row;
	wire	[1:0]	f_next_bank, f_this_bank;
	assign	f_next_row  = f_next_addr[(AW-1):(NCA+1)];
	assign	f_next_bank = f_next_addr[NCA:NCA-1];
	assign	f_this_bank = r_bs;
	assign	f_this_row  = r_row;

	always @(*)
	if (o_ram_cs_n==1'b0) casez(f_cmd)
	`F_MODE_SET:       begin end
	`F_BANK_PRECHARGE: begin end
	`F_PRECHARGE_ALL:  begin end
	`F_BANK_ACTIVATE:  begin end
	`F_WRITE:          begin end
	`F_READ:           begin end
	`F_REFRESH:        begin end
	default: assert(f_cmd[3:0] == `F_NOOP_S);
	endcase

	always @(posedge i_clk)
	if ((f_past_valid)&&(!maintenance_mode))
	casez(f_cmd)
	`F_BANK_ACTIVATE:	begin
		// Can only activate de-activated banks
		assert(bank_active[o_ram_bs][1:0] == 0);
		// Need to activate the right bank
		if (o_ram_bs == $past(f_this_bank))
			assert($past(f_this_row)==o_ram_addr);
		else if (o_ram_bs != 0)
		begin
			assert(o_ram_bs == $past(f_next_bank));
			assert($past(f_this_row)==o_ram_addr);
		end else begin
			assert(o_ram_bs == $past(f_next_bank));
			assert($past(f_next_row)==o_ram_addr);
		end end
	`F_BANK_PRECHARGE:	begin
		// Can only precharge (de-active) a fully active bank
		assert(bank_active[o_ram_bs] == 3'b011);
		end
	`F_PRECHARGE_ALL:	begin
		// If pre-charging all, one of the banks must be active and in
		// need of a pre-charge
		assert(
			(bank_active[0] == 3'b011)
			||(bank_active[1] == 3'b011)
			||(bank_active[2] == 3'b011)
			||(bank_active[3] == 3'b011) );
		end
	`F_WRITE:	begin
		assert($past(r_we));
		assert(bank_active[o_ram_bs] == 3'b111);
		assert(bank_row[o_ram_bs] == $past(f_this_row));
		assert(o_ram_bs == $past(f_this_bank));
		assert(o_ram_addr[0] == 1'b0);
		end
	`F_READ:	begin
		assert(!$past(r_we));
		assert(bank_active[o_ram_bs] == 3'b111);
		assert(bank_row[o_ram_bs] == $past(f_this_row));
		assert(o_ram_bs == $past(f_this_bank));
		assert(o_ram_addr[0] == 1'b0);
		end
	`F_REFRESH:	begin
		// When giving a reset command, *all* banks must be inactive
		assert( (bank_active[0] == 3'h0)
			&&(bank_active[1] == 3'h0)
			&&(bank_active[2] == 3'h0)
			&&(bank_active[3] == 3'h0) );
		end
	default: assert((o_ram_cs_n)||(f_cmd[3:0] == `F_NOOP_S));
	endcase

	integer	f_k;
	always @(posedge i_clk)
	if ((f_past_valid)&&(!$past(maintenance_mode)))
	begin
		for(f_k=0; f_k<4; f_k=f_k+1)
			if (((f_cmd[3:0] != `F_BANK_ACTIVATE_S))
		 			||(o_ram_bs != f_k[1:0]))
				assert($stable(bank_row[f_k[1:0]]));
	end

	always @(posedge i_clk)
	if ((f_past_valid)&&(!$past(maintenance_mode))
		&&($past(f_cmd) != `F_READ)
		&&($past(f_cmd) != `F_WRITE) )
	begin
		if (($past(r_pending))&&($past(i_wb_cyc)))
			assert($stable(f_pending));
	end

	always @(posedge i_clk)
	if ((f_past_valid)&&(!maintenance_mode))
		if ((r_pending)&&(f_cmd != `F_READ)&&(f_cmd != `F_WRITE))
			assert(o_wb_stall);

	always @(posedge i_clk)
	if ((f_past_valid)&&(!$past(maintenance_mode)))
	casez($past(f_cmd))
	`F_BANK_ACTIVATE: begin
		assert(bank_active[$past(o_ram_bs)] == 3'b110);
		assert(bank_row[$past(o_ram_bs)] == $past(o_ram_addr));
		end
	`F_BANK_PRECHARGE: begin
		assert(bank_active[$past(o_ram_bs)] == 3'b001);
		end
	`F_PRECHARGE_ALL: begin
		assert(bank_active[0][2] == 1'b0);
		assert(bank_active[1][2] == 1'b0);
		assert(bank_active[2][2] == 1'b0);
		assert(bank_active[3][2] == 1'b0);
		end
	// `F_WRITE:
	// `F_READ:
	`F_REFRESH: begin
		assert(r_barrell_ack == 0);
	end
	default: begin end
	endcase

	always @(posedge i_clk)
	if ((f_past_valid)&&(!$past(maintenance_mode)))
	begin
		assert(bank_active[0][1:0] == $past(bank_active[0][2:1]));
		assert(bank_active[1][1:0] == $past(bank_active[1][2:1]));
		assert(bank_active[2][1:0] == $past(bank_active[2][2:1]));
		assert(bank_active[3][1:0] == $past(bank_active[3][2:1]));
	end

	always @(*)
`ifdef	VERIFIC
	if (in_refresh)
`else
	if ((in_refresh)||(maintenance_mode))
`endif
	begin
		assert(bank_active[0] == 0);
		assert(bank_active[1] == 0);
		assert(bank_active[2] == 0);
		assert(bank_active[3] == 0);
	end

	always @(posedge i_clk)
	if ((f_past_valid)&&($past(o_wb_ack)))
		assert(!o_wb_ack);

	reg	[3:0]	f_acks_pending;
	always @(*)
	begin
		f_acks_pending = 0;
		for(f_k=0; f_k<RDLY; f_k = f_k + 1)
		if (r_barrell_ack[f_k])
			f_acks_pending = f_acks_pending + 1'b1;
	end

	always @(posedge i_clk)
	if ((f_past_valid)&&(!$past(i_wb_cyc)))
		assert(r_barrell_ack == 0);


	wire	f_ispending;
	assign	f_ispending = (r_pending)&&((f_cmd != `F_READ)&&(f_cmd != `F_WRITE));
	always @(posedge i_clk)
	if ((f_past_valid)&&(i_wb_cyc))
		assert(f_outstanding == (f_ispending ? 1'b1:1'b0) + f_acks_pending);

//	always @(posedge i_clk)
//	if (!i_wb_stb)
//	begin
//		restrict($stable(i_wb_we));
//		restrict($stable(i_wb_addr));
//		restrict($stable(i_wb_data));
//		if ((i_wb_stb)&&(!i_wb_we))
//			restrict(i_wb_data == 0);
//	end
//	always @(posedge i_clk)
//	if ((f_past_valid)&&(!$past(i_wb_stb))&&(i_wb_stb))
//		restrict(!$past(i_wb_cyc));

`ifdef	SHADOW_MEMORY
	reg	[15:0]		f_shadow_memory	[0:((1<<26)-1)];
	reg	[13+13-1:0]	f_shadow_addr;
	always @(*)
		f_shadow_addr <= { bank_row[o_ram_bs], o_ram_addr[8:0] };
	always @(posedge i_clk)
	if ((f_past_valid)&&($past(f_past_valid,4)
			&&(!$past(maintenance_mode,4)))
		if (f_cmd == `F_WRITE)
		begin
			f_shadow_memory[f_shadow_addr] <= o_ram_data;
			assert(o_ram_dmod==`DMOD_PUTOUTPUT);
			assert(o_ram_addr[0] == 1'b0);
		end else if ($past(f_cmd)== `F_WRITE)
		begin
			f_shadow_memory[f_shadow_addr[26-1:1], 1'b1}] <= data;
			assert(o_ram_dmod==`DMOD_PUTOUTPUT);
		end else if ($past(f_cmd,4)==`F_READ)
		begin
			assert($past(o_ram_dmod)==`DMOD_GETINPUT);
			assert(o_ram_dmod==`DMOD_GETINPUT);
			assume(o_wb_data[31:16] == f_shadow_memory[$past(f_shadow_addr,4)]);
			assume(o_wb_data[15: 0] == f_shadow_memory[{ $past(f_shadow_addr[25:1],4), 1'b1}]);
			assert(r_barrell_ack[0]);
		end
`endif
	always @(posedge i_clk)
	if (startup_hold)
		assert(m_state == `RAM_POWER_UP);

	always @(posedge i_clk)
	if ((f_past_valid)&&(m_state != `RAM_SET_MODE)
			&&($past(m_state) != `RAM_POWER_UP)
			&&($past(maintenance_clocks)!=0))
		assert(m_state == $past(m_state));

	always @(posedge i_clk)
	if ((f_past_valid)&&($past(m_state)== `RAM_POWER_UP))
		assert((m_state == `RAM_POWER_UP)
			||(m_state == `RAM_SET_MODE));

	always @(posedge i_clk)
	if ((f_past_valid)&&($past(m_state)== `RAM_SET_MODE))
		assert((m_state == `RAM_SET_MODE)
			||(m_state == `RAM_INITIAL_REFRESH));

	always @(posedge i_clk)
	if ((f_past_valid)&&($past(m_state)== `RAM_INITIAL_REFRESH))
		assert((m_state == `RAM_INITIAL_REFRESH)
			||(m_state == `RAM_OPERATIONAL));

	always @(posedge i_clk)
	assert((m_state == `RAM_POWER_UP)
		||(m_state == `RAM_INITIAL_REFRESH)
		||(m_state == `RAM_SET_MODE));

	always @(posedge i_clk)
	if (maintenance_mode)
		assert(clocks_til_idle ==0);
	always @(posedge i_clk)
	if (maintenance_clocks_zero)
		assert(maintenance_clocks == 0);

	always @(posedge i_clk)
	if (maintenance_clocks == 0)
		assert(maintenance_clocks_zero);

	always @(posedge i_clk)
	if (maintenance_clocks != 0)
		assert(!maintenance_clocks_zero);

	always @(posedge i_clk)
	if (!maintenance_mode)
		assert(m_state == `RAM_INITIAL_REFRESH);

	always @(posedge i_clk)
	if (m_state == `RAM_INITIAL_REFRESH)
		assert(maintenance_clocks <= 4'hc);

	always @(posedge i_clk)
	if (m_state == `RAM_SET_MODE)
		assert(maintenance_clocks <= 4'h3);

	always @(*)
	assert( (m_state == `RAM_POWER_UP)
		||(m_state == `RAM_INITIAL_REFRESH)
		||(m_state == `RAM_SET_MODE));
		// ||(m_state == `RAM_OPERATIONAL) );

	always @(posedge i_clk)
	if ((f_past_valid)&&(startup_hold))
	begin
		assert(o_ram_cke);
		assert(o_ram_cs_n);
		assert(o_ram_ras_n);
		assert(o_ram_cas_n);
		assert(o_ram_we_n);
		assert(o_ram_dqm);
	end
`endif
endmodule
