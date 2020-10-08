////////////////////////////////////////////////////////////////////////////////
//
// Filename:	qflexpress.v
//
// Project:	A Set of Wishbone Controlled SPI Flash Controllers
//
// Purpose:	To provide wishbone controlled read access (and read access
//		*only*) to the QSPI flash, using a flash clock equal to the
//	system clock, and nothing more.  Indeed, this is designed to be a
//	*very* stripped down version of a flash driver, with the goal of
//	providing 1) very fast access for 2) very low logic count.
//
//	Three modes/states of operation:
//	1. Startup/maintenance, places the device in the Quad XIP mode
//	2. Normal operations, takes 12+8N clocks to read a value
//	3. Configuration--useful to allow an external controller issue erase
//		or program commands (or other) without requiring us to
//		clutter up the logic with a giant state machine
//
//	STARTUP
//	 1. Waits for the flash to come on line
//		Start out idle for 300 uS
//	 2. Sends a signal to remove the flash from any QSPI read mode.  In our
//		case, we'll send several clocks of an empty command.  In SPI
//		mode, it'll get ignored.  In QSPI mode, it'll remove us from
//		QSPI mode.
//	 3. Explicitly places and leaves the flash into QSPI mode
//		0xEB 3(0x00) 0xa0 6(0x00)
//	 4. All done
//
// Creator:	Dan Gisselquist, Ph.D.
//		Gisselquist Technology, LLC
//
////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2018-2019, Gisselquist Technology, LLC
//
// This file is part of the set of Wishbone controlled SPI flash controllers
// project
//
// The Wishbone SPI flash controller project is free software (firmware):
// you can redistribute it and/or modify it under the terms of the GNU Lesser
// General Public License as published by the Free Software Foundation, either
// version 3 of the License, or (at your option) any later version.
//
// The Wishbone SPI flash controller project is distributed in the hope
// that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
// warranty of MERCHANTIBILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  (It's in the $(ROOT)/doc directory.  Run make
// with no target there if the PDF file isn't present.)  If not, see
// <http://www.gnu.org/licenses/> for a copy.
//
// License:	LGPL, v3, as defined and found on www.gnu.org,
//		http://www.gnu.org/licenses/lgpl.html
//
//
////////////////////////////////////////////////////////////////////////////////
//
//
`default_nettype	none
//
// 290 raw, 372 w/ pipe, 410 cfg, 499 cfg w/pipe
module	qflexpress(i_clk, i_reset,
		i_wb_cyc, i_wb_stb, i_cfg_stb, i_wb_we, i_wb_addr, i_wb_data,
			o_wb_ack, o_wb_stall, o_wb_data,
		o_qspi_sck, o_qspi_cs_n, o_qspi_mod, o_qspi_dat, i_qspi_dat);
	//
	// LGFLASHSZ is the size of the flash memory.  It defines the number
	// of bits in the address register and more.  This controller will support
	// flash sizes up to 2^LGFLASHSZ, where LGFLASHSZ goes up to 32.
	parameter	LGFLASHSZ=24;
	//
	// OPT_PIPE makes it possible to string multiple requests together,
	// with no intervening need to shutdown the QSPI connection and send a
	// new address
	parameter [0:0]	OPT_PIPE    = 1'b1;
	//
	// OPT_CFG enables the configuration logic port, and hence the
	// ability to erase and program the flash, as well as the ability
	// to perform other commands such as read-manufacturer ID, adjust
	// configuration registers, etc.
	parameter [0:0]	OPT_CFG     = 1'b1;
	//
	// OPT_STARTUP enables the startup logic
	parameter [0:0]	OPT_STARTUP = 1'b1;
	//
	// OPT_ADDR32 enables 32 bit addressing, rather than 24bit
	// Control this by controlling the LGFLASHSZ parameter above.  Anything
	// greater than 24 will use 32-bit addressing, otherwise the regular
	// 24-bit addressing
	localparam [0:0]	OPT_ADDR32 = (LGFLASHSZ > 24);
	//
	parameter	OPT_CLKDIV = 0;
	//
	// Normally, I place the first byte read from the flash, and the lowest
	// flash address, into bits [7:0], and then shift it up--to where upon
	// return it is found in bits [31:24].  This is ideal for a big endian
	// systems, not so much for little endian systems.  The endian swap
	// allows the bus to swap the return values in order to support little
	// endian systems.
	parameter [0:0]	OPT_ENDIANSWAP = 1'b1;
	//
	// OPT_ODDR will be true any time the clock has no clock division
	localparam [0:0]	OPT_ODDR = (OPT_CLKDIV == 0);
	//
	// CKDV_BITS is the number of bits necessary to represent a counter
	// that can do the CLKDIV division
	localparam 	CKDV_BITS = (OPT_CLKDIV == 0) ? 0
				: ((OPT_CLKDIV <   2) ? 1
				: ((OPT_CLKDIV <   4) ? 2
				: ((OPT_CLKDIV <   8) ? 3
				: ((OPT_CLKDIV <  16) ? 4
				: ((OPT_CLKDIV <  32) ? 5
				: ((OPT_CLKDIV <  64) ? 6
				: ((OPT_CLKDIV < 128) ? 7
				: ((OPT_CLKDIV < 256) ? 8 : 9))))))));
	//
	// RDDELAY is the number of clock cycles from when o_qspi_dat is valid
	// until i_qspi_dat is valid.  Read delays from 0-4 have been verified.
	// DDR Registered I/O on a Xilinx device can be done with a RDDELAY=3
	// On Intel/Altera devices, RDDELAY=2 works
	// I'm using RDDELAY=0 for my iCE40 devices
	//
	parameter	RDDELAY = 0;
	//
	// NDUMMY is the number of "dummy" clock cycles between the 24-bits of
	// the Quad I/O address and the first data bits.  This includes the
	// two clocks of the Quad output mode byte, 0xa0.  The default is 10
	// for a Micron device.  Windbond seems to want 2.  Note your flash
	// device carefully when you choose this value.
	//
	parameter	NDUMMY = 6;
	//
	// For dealing with multiple flash devices, the OPT_STARTUP_FILE allows
	// a hex file to be provided containing the necessary script to place
	// the design into the proper initial configuration.
	parameter	OPT_STARTUP_FILE="";
	//
	//
	//
	//
	localparam [4:0]	CFG_MODE =	12;
	localparam [4:0]	QSPEED_BIT = 	11;
	localparam [4:0]	DSPEED_BIT = 	10; // Not supported
	localparam [4:0]	DIR_BIT	= 	 9;
	localparam [4:0]	USER_CS_n = 	 8;
	//
	localparam [1:0]	NORMAL_SPI = 	2'b00;
	localparam [1:0]	QUAD_WRITE = 	2'b10;
	localparam [1:0]	QUAD_READ = 	2'b11;
	// localparam [7:0] DIO_READ_CMD = 8'hbb;
	localparam [7:0] QIO_READ_CMD = OPT_ADDR32 ? 8'hec : 8'heb;
	//
	localparam	AW=LGFLASHSZ-1;
	localparam	DW=32;
	//
`ifdef	FORMAL
	localparam	F_LGDEPTH=$clog2(3+RDDELAY+(OPT_ADDR32 ? 2:0));
	reg	f_past_valid;
`endif
	//
	//
	input	wire			i_clk, i_reset;
	//
	input	wire			i_wb_cyc, i_wb_stb, i_cfg_stb, i_wb_we;
	input	wire	[(AW-1):0]	i_wb_addr;
	input	wire	[(DW-1):0]	i_wb_data;
	//
	output	reg			o_wb_ack, o_wb_stall;
	output	reg	[(DW-1):0]	o_wb_data;
	//
	output	reg		o_qspi_sck;
	output	reg		o_qspi_cs_n;
	output	reg	[1:0]	o_qspi_mod;
	output	wire	[3:0]	o_qspi_dat;
	input	wire	[3:0]	i_qspi_dat;
	//
	// Debugging port
	// output	wire		o_dbg_trigger;
	// output	wire	[31:0]	o_debug;

	reg		dly_ack, read_sck, xtra_stall;
	// clk_ctr must have enough bits for ...
	//	8		address clocks, 4-bits each
	//	NDUMMY		dummy clocks, including two mode bytes
	//	8		data clocks
	//	(RDDELAY clocks not counted here)
	reg	[4:0]	clk_ctr;

	//
	// User override logic
	//
	reg	cfg_mode, cfg_speed, cfg_dir, cfg_cs;
	wire	cfg_write, cfg_hs_write, cfg_ls_write, cfg_hs_read,
		user_request, bus_request, pipe_req, cfg_noop, cfg_stb;
	//
	assign	bus_request  = (i_wb_stb)&&(!o_wb_stall)
					&&(!i_wb_we)&&(!cfg_mode);
	assign	cfg_stb      = (OPT_CFG)&&(i_cfg_stb)&&(!o_wb_stall);
	assign	cfg_noop     = ((cfg_stb)&&((!i_wb_we)||(!i_wb_data[CFG_MODE])
					||(i_wb_data[USER_CS_n])))
				||((!OPT_CFG)&&(i_cfg_stb)&&(!o_wb_stall));
	assign	user_request = (cfg_stb)&&(i_wb_we)&&(i_wb_data[CFG_MODE]);

	assign	cfg_write    = (user_request)&&(!i_wb_data[USER_CS_n]);
	assign	cfg_hs_write = (cfg_write)&&(i_wb_data[QSPEED_BIT])
					&&(i_wb_data[DIR_BIT]);
	assign	cfg_hs_read  = (cfg_write)&&(i_wb_data[QSPEED_BIT])
					&&(!i_wb_data[DIR_BIT]);
	assign	cfg_ls_write = (cfg_write)&&(!i_wb_data[QSPEED_BIT]);


	reg		ckstb, ckpos, ckneg, ckpre;
	reg		maintenance;
	reg	[1:0]	m_mod;
	reg		m_cs_n;
	reg		m_clk;
	reg	[3:0]	m_dat;


	generate if (OPT_ODDR)
	begin

		always @(*)
		begin
			ckstb = 1'b1;
			ckpos = 1'b1;
			ckneg = 1'b1;
			ckpre = 1'b1;
		end

	end else if (OPT_CLKDIV == 1)
	begin : CKSTB_ONE

		reg	clk_counter;

		initial	clk_counter = 1'b1;
		always @(posedge i_clk)
		if (i_reset)
			clk_counter <= 1'b1;
		else if (clk_counter != 0)
			clk_counter <= 1'b0;
		else if (bus_request)
			clk_counter <= (pipe_req);
		else if ((maintenance)||(!o_qspi_cs_n && o_wb_stall))
			clk_counter <= 1'b1;

		always @(*)
		begin
			ckpre = (clk_counter == 1);
			ckstb = (clk_counter == 0);
			ckpos = (clk_counter == 1);
			ckneg = (clk_counter == 0);
		end

	end else begin : CKSTB_GEN

		reg	[CKDV_BITS-1:0]	clk_counter;

		initial	clk_counter = OPT_CLKDIV;
		always @(posedge i_clk)
		if (i_reset)
			clk_counter <= OPT_CLKDIV;
		else if (clk_counter != 0)
			clk_counter <= clk_counter - 1;
		else if (bus_request)
			clk_counter <= (pipe_req ? OPT_CLKDIV : 0);
		else if ((maintenance)||(!o_qspi_cs_n && o_wb_stall))
			clk_counter <= OPT_CLKDIV;

		initial	ckpre = (OPT_CLKDIV == 1);
		initial	ckstb = 1'b0;
		initial	ckpos = (OPT_CLKDIV == 1);
		always @(posedge i_clk)
		if (i_reset)
		begin
			ckpre <= (OPT_CLKDIV == 1);
			ckstb <= 1'b0;
			ckpos <= (OPT_CLKDIV == 1);
		end else // if (OPT_CLKDIV > 1)
		begin
			ckpre <= (clk_counter == 2);
			ckstb <= (clk_counter == 1);
			ckpos <= (clk_counter == (OPT_CLKDIV+1)/2+1);
		end

		always @(*)
			ckneg = ckstb;
`ifdef	FORMAL
		always @(*)
			assert(!ckpos || !ckneg);

		always @(posedge i_clk)
		if ((f_past_valid)&&(!$past(i_reset))&&($past(ckpre)))
			assert(ckstb);
`endif
	end endgenerate

	//
	//
	// Maintenance / startup portion
	//
	//
	generate if (OPT_STARTUP)
	begin : GEN_STARTUP
		localparam	M_WAITBIT=10;
		localparam	M_LGADDR=5;
`ifdef	FORMAL
		// For formal, jump into the middle of the startup
		localparam	M_FIRSTIDX=9;
`else
		localparam	M_FIRSTIDX=0;
`endif
		reg	[M_WAITBIT:0]	m_this_word;
		reg	[M_WAITBIT:0]	m_cmd_word	[0:(1<<M_LGADDR)-1];
		reg	[M_LGADDR-1:0]	m_cmd_index;

		reg	[M_WAITBIT-1:0]	m_counter;
		reg			m_midcount;
		reg	[3:0]		m_bitcount;
		reg	[7:0]		m_byte;

		// Let's script our startup with a series of commands.
		// These commands are specific to the Micron Serial NOR flash
		// memory that was on the original Arty A7 board.  Switching
		// from one memory to another should only require adjustments
		// to this startup sequence, and to the flashdrvr.cpp module
		// found in sw/host.
		//
		// The format of the data words is ...
		//	1'bit (MSB) to indicate this is a counter word.
		//		Counter words count a number of idle cycles,
		//		in which the port is unused (CSN is high)
		//
		//	2'bit mode.  This is either ...
		//	    NORMAL_SPI, for a normal SPI interaction:
		//			MOSI, MISO, WPn and HOLD
		//	    QUAD_READ, all four pins set as inputs.  In this
		//			startup, the input values will be
		//			ignored.
		//	or  QUAD_WRITE, all four pins are outputs.  This is
		//			important for getting the flash into
		//			an XIP mode that we can then use for
		//			all reads following.
		//
		//	8'bit data	To be sent 1-bit at a time in NORMAL_SPI
		//			mode, or 4-bits at a time in QUAD_WRITE
		//			mode.  Ignored otherwis
		//
		integer k;
		initial
		// if (OPT_STARTUP_FILE)
		//	$readmemh(OPT_STARTUP_FILE, m_cmd_word); else
		begin
		for(k=0; k<(1<<M_LGADDR); k=k+1)
			m_cmd_word[k] = -1;
		// cmd_word= m_ctr_flag, m_mod[1:0],
		//			m_cs_n, m_clk, m_data[3:0]
		// Start off idle
		//	This is really redundant since all of our commands are
		//	idle's.
		m_cmd_word[5'h07] = -1;
		//
		// Since we don't know what mode we started in, whether the
		// device was left in XIP mode or some other mode, we'll start
		// by exiting any mode we might have been in.
		//
		// The key to doing this is to issue a non-command, that can
		// also be interpreted as an XIP address with an incorrect
		// mode bit.  That will get us out of any XIP mode, and back
		// into a SPI mode we might use.  The command is issued in
		// NORMAL_SPI mode, however, since we don't know if the device
		// is initially in XIP or not.
		//
		// Exit any QSPI mode we might've been in
		m_cmd_word[5'h08] = { 1'b0, NORMAL_SPI, 8'hff }; // Addr 1
		m_cmd_word[5'h09] = { 1'b0, NORMAL_SPI, 8'hff }; // Addr 2
		m_cmd_word[5'h0a] = { 1'b0, NORMAL_SPI, 8'hff }; // Addr 2
		// Idle
		m_cmd_word[5'h0b] = { 1'b1, 10'h3f };
		//
		// Write configuration register
		//
		// The write enable must come first: 06
		// m_cmd_word[5'h0c] = { 1'b0, NORMAL_SPI, 8'h06 };
		m_cmd_word[5'h0c] = { 1'b0, NORMAL_SPI, 8'h50 };
		//
		// Idle
		m_cmd_word[5'h0d] = { 1'b1, 10'h3ff };
		//
		// Write configuration register, follows a write-register
		m_cmd_word[5'h0e] = { 1'b0, NORMAL_SPI, 8'h01 };	// WRR
		m_cmd_word[5'h0f] = { 1'b0, NORMAL_SPI, 8'h00 };	// status register
		m_cmd_word[5'h10] = { 1'b0, NORMAL_SPI, 8'h02 };	// Config register
		//
		// Idle
		m_cmd_word[5'h11] = { 1'b1, 10'h3ff };
		m_cmd_word[5'h12] = { 1'b1, 10'h3ff };
		//
		//
		// WRDI: write disable: 04
		m_cmd_word[5'h13] = { 1'b0, NORMAL_SPI, 8'h04 };
		//
		// Idle
		m_cmd_word[5'h14] = { 1'b1, 10'h3ff };
		//
		// Enter into QSPI mode, 0xeb, 0,0,0
		// 0xeb
		m_cmd_word[5'h15] = { 1'b0, NORMAL_SPI, 8'heb };
		// Addr #1
		m_cmd_word[5'h16] = { 1'b0, QUAD_WRITE, 8'h00 };
		// Addr #2
		m_cmd_word[5'h17] = { 1'b0, QUAD_WRITE, 8'h00 };
		// Addr #3
		m_cmd_word[5'h18] = { 1'b0, QUAD_WRITE, 8'h00 };
		// Mode byte
		m_cmd_word[5'h19] = { 1'b0, QUAD_WRITE, 8'ha0 };
		// Dummy clocks, x4 for this flash
		m_cmd_word[5'h1a] = { 1'b0, QUAD_WRITE, 8'h00 };
		m_cmd_word[5'h1b] = { 1'b0, QUAD_WRITE, 8'h00 };
		// Now read a byte for form
		m_cmd_word[5'h1c] = { 1'b0, QUAD_READ, 8'h00 };
		//
		// Idle -- These last two idles are *REQUIRED* and not optional
		// (although they might be able to be trimmed back a bit...)
		m_cmd_word[5'h1d] = -1;
		m_cmd_word[5'h1e] = -1;
		// Then we are in business!
		end

		reg	m_final;

		wire	m_ce, new_word;
		assign	m_ce = (!m_midcount)&&(ckstb);
		assign	new_word = (m_ce && m_bitcount == 0);

		//
		initial	maintenance = 1'b1;
		initial	m_cmd_index = M_FIRSTIDX;
		always @(posedge i_clk)
		if (i_reset)
		begin
			m_cmd_index <= M_FIRSTIDX;
			maintenance <= 1'b1;
		end else if (new_word)
		begin
			maintenance <= (maintenance)&&(!m_final);
			if (!(&m_cmd_index))
				m_cmd_index <= m_cmd_index + 1'b1;
		end

		initial	m_this_word = -1;
		always @(posedge i_clk)
		if (new_word)
			m_this_word <= m_cmd_word[m_cmd_index];

		initial	m_final = 1'b0;
		always @(posedge i_clk)
		if (i_reset)
			m_final <= 1'b0;
		else if (new_word)
			m_final <= (m_final || (&m_cmd_index));

		//
		// m_midcount .. are we in the middle of a counter/pause?
		//
		initial	m_midcount = 1;
		initial	m_counter   = -1;
		always @(posedge i_clk)
		if (i_reset)
		begin
			m_midcount <= 1'b1;
`ifdef	FORMAL
			m_counter <= 3;
`else
			m_counter <= -1;
`endif
		end else if (new_word)
		begin
			m_midcount <= m_this_word[M_WAITBIT]
					&& (|m_this_word[M_WAITBIT-1:0]);
			if (m_this_word[M_WAITBIT])
			begin
				m_counter <= m_this_word[M_WAITBIT-1:0];
`ifdef	FORMAL
				if (m_this_word[M_WAITBIT-1:0] > 3)
					m_counter <= 3;
`endif
			end
		end else begin
			m_midcount <= (m_counter > 1);
			if (m_counter > 0)
				m_counter <= m_counter - 1'b1;
		end

		initial	m_cs_n      = 1'b1;
		initial	m_mod       = NORMAL_SPI;
		always @(posedge i_clk)
		if (i_reset)
		begin
			m_cs_n <= 1'b1;
			m_mod  <= NORMAL_SPI;
			m_bitcount <= 0;
		end else if (ckstb)
		begin
			if (m_bitcount != 0)
				m_bitcount <= m_bitcount - 1;
			else if ((m_ce)&&(m_final))
			begin
				m_cs_n <= 1'b1;
				m_mod  <= NORMAL_SPI;
				m_bitcount <= 0;
			end else if ((m_midcount)||(m_this_word[M_WAITBIT]))
			begin
				m_cs_n <= 1'b1;
				m_mod  <= NORMAL_SPI;
				m_bitcount <= 0;
			end else begin
				m_cs_n <= 1'b0;
				m_mod  <= m_this_word[M_WAITBIT-1:M_WAITBIT-2];
				m_bitcount <= (!OPT_ODDR && m_cs_n) ? 4'h2 : 4'h1;
				if (!m_this_word[M_WAITBIT-1])
					m_bitcount <= (!OPT_ODDR && m_cs_n) ? 4'h8 : 4'h7;//i.e.7
			end
		end

		always @(posedge i_clk)
		if (m_ce)
		begin
			if (m_bitcount == 0)
			begin
				if (!OPT_ODDR && m_cs_n)
				begin
					m_dat <= {(4){m_this_word[7]}};
					m_byte <= m_this_word[7:0];
				end else begin
					m_dat <= m_this_word[7:4];
					m_byte <= { m_this_word[3:0], 4'h0};
					if (!m_this_word[M_WAITBIT-1])
					begin
						// Slow speed
						m_dat[0]  <= m_this_word[7];
						m_byte <= { m_this_word[6:0], 1'b0 };
					end
				end
			end else begin
				m_dat <= m_byte[7:4];
				m_byte <= { m_byte[3:0], 4'h0 };
				if (!m_mod[1])
				begin
					// Slow speed
					m_dat[0] <= m_byte[7];
					m_byte <= { m_byte[6:0], 1'b0 };
				end else begin
					m_byte <= { m_byte[3:0], 4'b00 };
				end
			end
		end

		if (OPT_ODDR)
		begin
			always @(*)
				m_clk = !m_cs_n;
		end else begin

			always @(posedge i_clk)
			if (i_reset)
				m_clk <= 1'b1;
			else if (m_cs_n)
				m_clk <= 1'b1;
			else if ((!m_clk)&&(ckpos))
				m_clk <= 1'b1;
			else if (m_midcount)
				m_clk <= 1'b1;
			else if (new_word && m_this_word[M_WAITBIT])
				m_clk <= 1'b1;
			else if (ckneg)
				m_clk <= 1'b0;
		end

`ifdef	FORMAL
		(* anyconst *) reg [M_LGADDR:0]	f_const_addr;

		always @(*)
		begin
			assert((m_cmd_word[f_const_addr][M_WAITBIT])
				||(m_cmd_word[f_const_addr][9:8] != 2'b01));
			if (m_cmd_word[f_const_addr][M_WAITBIT])
				assert(m_cmd_word[f_const_addr][M_WAITBIT-3:0] > 0);
		end
		always @(*)
		begin
			if (m_cmd_index != f_const_addr)
				assume((m_cmd_word[m_cmd_index][M_WAITBIT])||(m_cmd_word[m_cmd_index][9:8] != 2'b01));
			if (m_cmd_word[m_cmd_index][M_WAITBIT])
				assume(m_cmd_word[m_cmd_index][M_WAITBIT-3:0]>0);
		end

		always @(*)
		begin
			assert((m_this_word[M_WAITBIT])
				||(m_this_word[9:8] != 2'b01));
			if (m_this_word[M_WAITBIT])
				assert(m_this_word[M_WAITBIT-3:0] > 0);
		end

		// Setting the last two command words to IDLE with maximum
		// counts is required by our implementation
		always @(*)
			assert(m_cmd_word[5'h1e] == 11'h7ff);
		always @(*)
			assert(m_cmd_word[5'h1f] == 11'h7ff);

		wire	[M_LGADDR-1:0]	last_index;
		assign	last_index = m_cmd_index - 1;

		always @(posedge i_clk)
		if ((f_past_valid)&&(m_cmd_index != M_FIRSTIDX))
			assert(m_this_word == m_cmd_word[last_index]);

		always @(posedge i_clk)
			assert(m_midcount == (m_counter != 0));

		always @(posedge i_clk)
		begin
			cover(!maintenance);
			cover(m_cmd_index == 5'h0a);
			cover(m_cmd_index == 5'h0b);
			cover(m_cmd_index == 5'h0c);
			cover(m_cmd_index == 5'h0d);
			cover(m_cmd_index == 5'h0e);
			cover(m_cmd_index == 5'h0f);
			cover(m_cmd_index == 5'h10);
			cover(m_cmd_index == 5'h11);
			cover(m_cmd_index == 5'h12);
			cover(m_cmd_index == 5'h13);
			cover(m_cmd_index == 5'h14);
			cover(m_cmd_index == 5'h15);
			cover(m_cmd_index == 5'h16);
			cover(m_cmd_index == 5'h17);
			cover(m_cmd_index == 5'h18);
			cover(m_cmd_index == 5'h19);
			cover(m_cmd_index == 5'h1a);
			cover(m_cmd_index == 5'h1b);
			cover(m_cmd_index == 5'h1c);
			cover(m_cmd_index == 5'h1d);
			cover(m_cmd_index == 5'h1e);
			cover(m_cmd_index == 5'h1f);
		end

		reg	[M_WAITBIT:0]	f_last_word;
		reg	[8:0]		f_mspi;
		reg	[2:0]		f_mqspi;

		initial	f_last_word = -1;
		always @(posedge i_clk)
		if (i_reset)
			f_last_word = -1;
		else if (new_word)
			f_last_word <= m_this_word;


		initial	f_mspi = 0;
		always @(posedge i_clk)
		if (i_reset)
			f_mspi <= 0;
		else if (ckstb) begin
			f_mspi <= f_mspi << 1;
			if (maintenance && !m_final &&  new_word
				&&(!m_this_word[M_WAITBIT])
				&&(m_this_word[9:8] == NORMAL_SPI))
			begin
				if (m_cs_n && !OPT_ODDR)
					f_mspi[0] <= 1'b1;
				else
					f_mspi[1] <= 1'b1;
			end
		end

		initial	f_mqspi = 0;
		always @(posedge i_clk)
		if (i_reset)
			f_mqspi <= 0;
		else if (ckstb) begin
			f_mqspi <= f_mqspi << 1;
			if (maintenance && !m_final && new_word
				&&(!m_this_word[M_WAITBIT])
				&&(m_this_word[9]))
			begin
				if (m_cs_n && !OPT_ODDR)
					f_mqspi[0] <= 1'b1;
				else
					f_mqspi[1] <= 1'b1;
			end
		end

		always @(*)
		if (OPT_ODDR)
			assert(!f_mspi[0] && !f_mqspi[0]);


		always @(*)
		if ((|f_mspi) || (|f_mqspi))
		begin

			assert(maintenance);
			assert(!m_cs_n);
			assert(m_mod == f_last_word[9:8]);
			assert(m_midcount == 1'b0);

		end else if (maintenance && o_qspi_cs_n)
		begin
			assert(f_last_word[M_WAITBIT]);
			assert(m_counter <= f_last_word[M_WAITBIT-1:0]);
			assert(m_midcount == (m_counter != 0));
			assert(m_cs_n);
		end

		always @(*)
			assert((f_mspi == 0)||(f_mqspi == 0));

		always @(*)
		if (|f_mspi)
			assert(m_mod == NORMAL_SPI);

		always @(*)
		case(f_mspi[8:1])
		8'h00: begin end
		8'h01: assert(m_dat[0] == f_last_word[7]);
		8'h02: assert(m_dat[0] == f_last_word[6]);
		8'h04: assert(m_dat[0] == f_last_word[5]);
		8'h08: assert(m_dat[0] == f_last_word[4]);
		8'h10: assert(m_dat[0] == f_last_word[3]);
		8'h20: assert(m_dat[0] == f_last_word[2]);
		8'h40: assert(m_dat[0] == f_last_word[1]);
		8'h80: assert(m_dat[0] == f_last_word[0]);
		default: begin assert(0); end
		endcase

		always @(*)
		if (|f_mqspi)
			assert(m_mod == QUAD_WRITE || m_mod == QUAD_READ);

		always @(*)
		case(f_mqspi[2:1])
		2'b00: begin end
		2'b01: assert(m_dat[3:0] == f_last_word[7:4]);
		2'b10: assert(m_dat[3:0] == f_last_word[3:0]);
		default: begin assert(f_mqspi != 2'b11); end
		endcase
`endif
	end else begin : NO_STARTUP_OPT

		always @(*)
		begin
			maintenance = 0;
			m_mod       = 2'b00;
			m_cs_n      = 1'b1;
			m_clk       = 1'b0;
			m_dat       = 4'h0;
		end

		// verilator lint_off UNUSED
		wire	[8:0] unused_maintenance;
		assign	unused_maintenance = { maintenance,
					m_mod, m_cs_n, m_clk, m_dat };
		// verilator lint_on  UNUSED
	end endgenerate


	reg	[32+(OPT_ADDR32 ? 8:0)+4*(OPT_ODDR ? 0:1)-1:0]	data_pipe;
	reg	pre_ack = 1'b0;
	reg	actual_sck;

	//
	//
	// Data / access portion
	//
	//
	initial	data_pipe = 0;
	always @(posedge i_clk)
	begin
		if (!o_wb_stall)
		begin
			// Set the high bits to zero initially
			data_pipe <= 0;

			data_pipe[8+LGFLASHSZ-1:0] <= {
					i_wb_addr, 1'b0, 4'ha, 4'h0 };

			if (i_cfg_stb)
				// High speed configuration I/O
				data_pipe[24+(OPT_ADDR32 ? 8:0) +: 8] <= i_wb_data[7:0];

			if ((i_cfg_stb)&&(!i_wb_data[QSPEED_BIT]))
			begin // Low speed configuration I/O
				data_pipe[28+(OPT_ADDR32 ? 8:0)]<= i_wb_data[7];
				data_pipe[24+(OPT_ADDR32 ? 8:0)]<= i_wb_data[6];
			end

			if (i_cfg_stb)
			begin // These can be set independent of speed
				data_pipe[20+(OPT_ADDR32 ? 8:0)]<= i_wb_data[5];
				data_pipe[16+(OPT_ADDR32 ? 8:0)]<= i_wb_data[4];
				data_pipe[12+(OPT_ADDR32 ? 8:0)]<= i_wb_data[3];
				data_pipe[ 8+(OPT_ADDR32 ? 8:0)]<= i_wb_data[2];
				data_pipe[ 4+(OPT_ADDR32 ? 8:0)]<= i_wb_data[1];
				data_pipe[ 0+(OPT_ADDR32 ? 8:0)]<= i_wb_data[0];
			end
		end else if (ckstb)
			data_pipe <= { data_pipe[(32+(OPT_ADDR32 ? 8:0)+4*((OPT_ODDR ? 0:1)-1))-1:0], 4'h0 };

		if (maintenance)
			data_pipe[28+(OPT_ADDR32 ? 8:0)+4*(OPT_ODDR ? 0:1) +: 4] <= m_dat;
	end

	assign	o_qspi_dat = data_pipe[28+(OPT_ADDR32 ? 8:0)+4*(OPT_ODDR ? 0:1) +: 4];

	// Since we can't abort any transaction once started, without
	// risking losing XIP mode or any other mode we might be in, we'll
	// keep track of whether this operation should be ack'd upon
	// completion
	always @(posedge i_clk)
	if ((i_reset)||(!i_wb_cyc))
		pre_ack <= 1'b0;
	else if ((bus_request)||(cfg_write))
		pre_ack <= 1'b1;

	generate if (OPT_PIPE)
	begin : OPT_PIPE_BLOCK
		reg	r_pipe_req;
		wire	w_pipe_condition;

		reg	[(AW-1):0]	next_addr;
		always  @(posedge i_clk)
		if (!o_wb_stall)
			next_addr <= i_wb_addr + 2'd2;

		assign	w_pipe_condition = (i_wb_stb)&&(!i_wb_we)&&(pre_ack)
				&&(!maintenance)
				&&(!cfg_mode)
				&&(!o_qspi_cs_n)
				&&(|clk_ctr[2:0])
				&&(next_addr == i_wb_addr);

		initial	r_pipe_req = 1'b0;
		always @(posedge i_clk)
		if ((clk_ctr == 1)&&(ckstb))
			r_pipe_req <= 1'b0;
		else
			r_pipe_req <= w_pipe_condition;

		assign	pipe_req = r_pipe_req;
	end else begin
		assign	pipe_req = 1'b0;
	end endgenerate


	initial	clk_ctr = 0;
	always @(posedge i_clk)
	if ((i_reset)||(maintenance))
		clk_ctr <= 0;
	else if ((bus_request)&&(!pipe_req))
		// Notice that this is only for
		// regular bus reads, and so the check for
		// !pipe_req
		clk_ctr <= 5'd14 + NDUMMY + (OPT_ADDR32 ? 2:0)+(OPT_ODDR ? 0:1);
	else if (bus_request) // && pipe_req
		// Otherwise, if this is a piped read, we'll
		// reset the counter back to eight.
		clk_ctr <= 5'd8;
	else if (cfg_ls_write)
		clk_ctr <= 5'd8 + ((OPT_ODDR) ? 0:1);
	else if (cfg_write)
		clk_ctr <= 5'd2 + ((OPT_ODDR) ? 0:1);
	else if ((ckstb)&&(|clk_ctr))
		clk_ctr <= clk_ctr - 1'b1;

	initial	o_qspi_sck = (!OPT_ODDR);
	always @(posedge i_clk)
	if (i_reset)
		o_qspi_sck <= (!OPT_ODDR);
	else if (maintenance)
		o_qspi_sck <= m_clk;
	else if ((!OPT_ODDR)&&(bus_request)&&(pipe_req))
		o_qspi_sck <= 1'b0;
	else if ((bus_request)||(cfg_write))
		o_qspi_sck <= 1'b1;
	else if (OPT_ODDR)
	begin
		if ((cfg_mode)&&(clk_ctr <= 1))
			// Config mode has no pipe instructions
			o_qspi_sck <= 1'b0;
		else if (clk_ctr[4:0] > 5'd1)
			o_qspi_sck <= 1'b1;
		else
			o_qspi_sck <= 1'b0;
	end else if (((ckpos)&&(!o_qspi_sck))||(o_qspi_cs_n))
	begin
		o_qspi_sck <= 1'b1;
	end else if ((ckneg)&&(o_qspi_sck)) begin

		if ((cfg_mode)&&(clk_ctr <= 1))
			// Config mode has no pipe instructions
			o_qspi_sck <= 1'b1;
		else if (clk_ctr[4:0] > 5'd1)
			o_qspi_sck <= 1'b0;
		else
			o_qspi_sck <= 1'b1;
	end

	initial	o_qspi_cs_n = 1'b1;
	always @(posedge i_clk)
	if (i_reset)
		o_qspi_cs_n <= 1'b1;
	else if (maintenance)
		o_qspi_cs_n <= m_cs_n;
	else if ((cfg_stb)&&(i_wb_we))
		o_qspi_cs_n <= (!i_wb_data[CFG_MODE])||(i_wb_data[USER_CS_n]);
	else if ((OPT_CFG)&&(cfg_cs))
		o_qspi_cs_n <= 1'b0;
	else if ((bus_request)||(cfg_write))
		o_qspi_cs_n <= 1'b0;
	else if (ckstb)
		o_qspi_cs_n <= (clk_ctr <= 1);

	// Control the mode of the external pins
	// 	NORMAL_SPI: i_miso is an input,  o_mosi is an output
	// 	QUAD_READ:  i_miso is an input,  o_mosi is an input
	// 	QUAD_WRITE: i_miso is an output, o_mosi is an output
	initial	o_qspi_mod =  NORMAL_SPI;
	always @(posedge i_clk)
	if (i_reset)
		o_qspi_mod <= NORMAL_SPI;
	else if (maintenance)
		o_qspi_mod <= m_mod;
	else if ((bus_request)&&(!pipe_req))
		o_qspi_mod <= QUAD_WRITE;
	else if ((bus_request)||(cfg_hs_read))
		o_qspi_mod <= QUAD_READ;
	else if (cfg_hs_write)
		o_qspi_mod <= QUAD_WRITE;
	else if ((cfg_ls_write)||((cfg_mode)&&(!cfg_speed)))
		o_qspi_mod <= NORMAL_SPI;
	else if ((ckstb)&&(clk_ctr <= 5'd9)&&((!cfg_mode)||(!cfg_dir)))
		o_qspi_mod <= QUAD_READ;

	initial	o_wb_stall = 1'b1;
	always @(posedge i_clk)
	if (i_reset)
		o_wb_stall <= 1'b1;
	else if (maintenance)
		o_wb_stall <= 1'b1;
	else if ((RDDELAY > 0)&&((i_cfg_stb)||(i_wb_stb))&&(!o_wb_stall))
		o_wb_stall <= 1'b1;
	else if ((RDDELAY == 0)&&((cfg_write)||(bus_request)))
		o_wb_stall <= 1'b1;
	else if (ckstb || clk_ctr == 0)
	begin
		if (ckpre && (i_wb_stb)&&(pipe_req)&&(clk_ctr == 5'd2))
			o_wb_stall <= 1'b0;
		else if ((clk_ctr > 1)||(xtra_stall))
			o_wb_stall <= 1'b1;
		else
			o_wb_stall <= 1'b0;
	end else if (ckpre && (i_wb_stb)&&(pipe_req)&&(clk_ctr == 5'd1))
		o_wb_stall <= 1'b0;

	initial	dly_ack = 1'b0;
	always @(posedge i_clk)
	if (i_reset)
		dly_ack <= 1'b0;
	else if ((ckstb)&&(clk_ctr == 1))
		dly_ack <= (i_wb_cyc)&&(pre_ack);
	else if ((i_wb_stb)&&(!o_wb_stall)&&(!bus_request))
		dly_ack <= 1'b1;
	else if (cfg_noop)
		dly_ack <= 1'b1;
	else
		dly_ack <= 1'b0;

	generate if (OPT_ODDR)
	begin : SCK_ACTUAL

		always @(*)
			actual_sck = o_qspi_sck;

	end else if (OPT_CLKDIV == 1)
	begin : SCK_ONE

		initial	actual_sck = 1'b0;
		always @(posedge i_clk)
		if (i_reset)
			actual_sck <= 1'b0;
		else
			actual_sck <= (!o_qspi_sck)&&(clk_ctr > 0);

	end else begin : SCK_ANY

		initial	actual_sck = 1'b0;
		always @(posedge i_clk)
		if (i_reset)
			actual_sck <= 1'b0;
		else
			actual_sck <= (o_qspi_sck)&&(ckpre)&&(clk_ctr > 0);

	end endgenerate


`ifdef	FORMAL
	reg	[F_LGDEPTH-1:0]	f_extra;
`endif

	generate if (RDDELAY == 0)
	begin : RDDELAY_NONE

		always @(*)
		begin
			read_sck = actual_sck;
			o_wb_ack = dly_ack;
			xtra_stall = 1'b0;
		end

`ifdef	FORMAL
		always @(*)
			f_extra = 0;
`endif

	end else
	begin : RDDELAY_NONZERO

		reg	[RDDELAY-1:0]	sck_pipe, ack_pipe, stall_pipe;
		reg	not_done;

		initial	sck_pipe = 0;
		initial	ack_pipe = 0;
		initial	stall_pipe = -1;
		if (RDDELAY > 1)
		begin
			always @(posedge i_clk)
			if (i_reset)
				sck_pipe <= 0;
			else
				sck_pipe <= { sck_pipe[RDDELAY-2:0], actual_sck };

			always @(posedge i_clk)
			if (i_reset || !i_wb_cyc)
				ack_pipe <= 0;
			else
				ack_pipe <= { ack_pipe[RDDELAY-2:0], dly_ack };

			always @(posedge i_clk)
			if (i_reset)
				stall_pipe <= -1;
			else
				stall_pipe <= { stall_pipe[RDDELAY-2:0], not_done };


		end else // if (RDDELAY > 0)
		begin
			always @(posedge i_clk)
			if (i_reset)
				sck_pipe <= 0;
			else
				sck_pipe <= actual_sck;

			always @(posedge i_clk)
			if (i_reset || !i_wb_cyc)
				ack_pipe <= 0;
			else
				ack_pipe <= dly_ack;

			always @(posedge i_clk)
			if (i_reset)
				stall_pipe <= -1;
			else
				stall_pipe <= not_done;
		end

		always @(*)
		begin
			not_done = (i_wb_stb || i_cfg_stb) && !o_wb_stall;
			if (clk_ctr > 1)
				not_done = 1'b1;
			if ((clk_ctr == 1)&&(!ckstb))
				not_done = 1'b1;
		end

		always @(*)
			o_wb_ack = ack_pipe[RDDELAY-1];

		always @(*)
			read_sck = sck_pipe[RDDELAY-1];

		always @(*)
			xtra_stall = |stall_pipe;

`ifdef	FORMAL
		integer	k;
		always @(*)
		if (!i_wb_cyc)
			f_extra = 0;
		else begin
			f_extra = 0;
			for(k=0; k<RDDELAY; k=k+1)
				f_extra = f_extra + (ack_pipe[k] ? 1 : 0);
		end
`endif // FORMAL

	end endgenerate

	always @(posedge i_clk)
	begin
		if (read_sck)
		begin
			if (OPT_ENDIANSWAP)
			begin
				if (!o_qspi_mod[1])
				begin
					o_wb_data <= { o_wb_data[30:24], i_qspi_dat[1],
						o_wb_data[22:16], o_wb_data[31],
						o_wb_data[14:8], o_wb_data[23],
						o_wb_data[6:0], o_wb_data[15] };
				end else begin
					o_wb_data <= { o_wb_data[27:24], i_qspi_dat,
						o_wb_data[19:16], o_wb_data[31:28],
						o_wb_data[11:8], o_wb_data[23:20],
						o_wb_data[3:0], o_wb_data[15:12]};
				end

				if (cfg_mode)
				begin
					// No endian-swapping in config mode
					if (!o_qspi_mod[1])
					o_wb_data[7:0]<= { o_wb_data[6:0], i_qspi_dat[1] };
					else
					o_wb_data[7:0]<= { o_wb_data[3:0], i_qspi_dat };
				end

			end else if (!o_qspi_mod[1])
				// No endian-swapping
				o_wb_data <= { o_wb_data[30:0], i_qspi_dat[1] };
			else
				o_wb_data <= { o_wb_data[27:0], i_qspi_dat };
		end // read_sck

		if ((OPT_CFG)&&(cfg_mode))
			o_wb_data[16:8] <= { 4'b0, cfg_mode, cfg_speed, 1'b0,
				cfg_dir, cfg_cs };
	end


	//
	//
	//  User override access
	//
	//
	initial	cfg_mode = 1'b0;
	always @(posedge i_clk)
	if ((i_reset)||(!OPT_CFG))
		cfg_mode <= 1'b0;
	else if ((i_cfg_stb)&&(!o_wb_stall)&&(i_wb_we))
		cfg_mode <= i_wb_data[CFG_MODE];

	initial	cfg_cs = 1'b0;
	always @(posedge i_clk)
	if ((i_reset)||(!OPT_CFG))
		cfg_cs <= 1'b0;
	else if ((i_cfg_stb)&&(!o_wb_stall)&&(i_wb_we))
		cfg_cs    <= (!i_wb_data[USER_CS_n])&&(i_wb_data[CFG_MODE]);

	initial	cfg_speed = 1'b0;
	initial	cfg_dir   = 1'b0;
	always @(posedge i_clk)
	if (!OPT_CFG)
	begin
		cfg_speed <= 1'b0;
		cfg_dir   <= 1'b0;
	end else if ((i_cfg_stb)&&(!o_wb_stall)&&(i_wb_we))
	begin
		cfg_speed <= i_wb_data[QSPEED_BIT];
		cfg_dir   <= i_wb_data[DIR_BIT];
	end

	/*
	reg	r_last_cfg;

	initial	r_last_cfg = 1'b0;
	always @(posedge i_clk)
		r_last_cfg <= cfg_mode;
	assign	o_dbg_trigger = (!cfg_mode)&&(r_last_cfg);
	assign	o_debug = { o_dbg_trigger,
			i_wb_cyc, i_cfg_stb, i_wb_stb, o_wb_ack, o_wb_stall,//6
			o_qspi_cs_n, o_qspi_sck, o_qspi_dat, o_qspi_mod,// 8
			i_qspi_dat, cfg_mode, cfg_cs, cfg_speed, cfg_dir,// 8
			actual_sck, i_wb_we,
			(((i_wb_stb)||(i_cfg_stb))
				&&(i_wb_we)&&(!o_wb_stall)&&(!o_wb_ack))
				? i_wb_data[7:0] : o_wb_data[7:0]
			};
	*/

	// verilator lint_off UNUSED
	wire	[19:0]	unused;
	assign	unused = { i_wb_data[31:12] };
	// verilator lint_on  UNUSED

`ifdef	FORMAL
	localparam	F_MEMDONE   = NDUMMY+6+8+(OPT_ADDR32 ? 2:0)+(OPT_ODDR ? 0:1);
	localparam	F_MEMACK    = F_MEMDONE + RDDELAY;
	localparam	F_PIPEDONE  = 8;
	localparam	F_PIPEACK   = F_PIPEDONE + RDDELAY;
	localparam	F_CFGLSDONE = 8+(OPT_ODDR ? 0:1);
	localparam	F_CFGLSACK  = F_CFGLSDONE + RDDELAY;
	localparam	F_CFGHSDONE = 2+(OPT_ODDR ? 0:1);
	localparam	F_CFGHSACK  = RDDELAY+F_CFGHSDONE;
	localparam	F_ACKCOUNT = (15+NDUMMY+RDDELAY)
				*(OPT_ODDR ? 1 : (OPT_CLKDIV+1));
	genvar	k;

	wire	[(F_LGDEPTH-1):0]	f_nreqs, f_nacks,
					f_outstanding;
	reg	[(AW-1):0]	f_req_addr;
	reg	[(OPT_ADDR32 ? 29:21):0]	fv_addr;
	reg	[31:0]	fv_data;
	reg	[F_MEMACK:0] f_memread;
	reg	[32:0]	f_past_data;
	reg	[F_PIPEACK:0]	f_piperead;
	reg	[F_CFGHSACK:0]	f_cfghsread;
	reg	[F_CFGHSACK:0]	f_cfghswrite;
	reg	[F_CFGLSACK:0]	f_cfglswrite;

//
//
// Generic setup
//
//
`ifdef	QFLEXPRESS
`define	ASSUME	assume
`else
`define	ASSUME	assert
`endif

	// Keep track of a flag telling us whether or not $past()
	// will return valid results
	initial	f_past_valid = 1'b0;
	always @(posedge i_clk)
		f_past_valid = 1'b1;

	always @(*)
	if (!f_past_valid)
       		`ASSUME(i_reset);

	/////////////////////////////////////////////////
	//
	//
	// Assumptions about our inputs
	//
	//
	/////////////////////////////////////////////////

	always @(*)
		`ASSUME((!i_wb_stb)||(!i_cfg_stb));

	always @(posedge i_clk)
	if ((f_past_valid)&&(!$past(i_reset))
			&&($past(i_wb_stb))&&($past(o_wb_stall)))
		`ASSUME(i_wb_stb);

	always @(posedge i_clk)
	if ((f_past_valid)&&(!$past(i_reset))
			&&($past(i_cfg_stb))&&($past(o_wb_stall)))
		`ASSUME(i_cfg_stb);

	fwb_slave #(.AW(AW), .DW(DW),.F_LGDEPTH(F_LGDEPTH),
			.F_MAX_STALL(((OPT_CLKDIV<3) ? (F_ACKCOUNT+1):0)
				+(OPT_ADDR32 ? 2:0)),
			.F_MAX_ACK_DELAY(((OPT_CLKDIV<3) ? F_ACKCOUNT : 0)
				+(OPT_ADDR32 ? 2:0)),
			.F_OPT_RMW_BUS_OPTION(0),
			.F_OPT_CLK2FFLOGIC(1'b0),
			.F_OPT_DISCONTINUOUS(1))
		f_wbm(i_clk, i_reset,
			i_wb_cyc, (i_wb_stb)||(i_cfg_stb), i_wb_we, i_wb_addr,
				i_wb_data, 4'hf,
			o_wb_ack, o_wb_stall, o_wb_data, 1'b0,
			f_nreqs, f_nacks, f_outstanding);

	always @(*)
		assert(f_outstanding <= 2 + f_extra);

	always @(posedge i_clk)
	if ((f_past_valid)&&(!$past(i_wb_stb))||($past(o_wb_stall)))
		assert(f_outstanding <= 1 + f_extra);

	////////////////////////////////////////////////////////////////////////
	//
	// Assumptions about i_qspi_dat
	//
	// 1. On output, i_qspi_dat equals the input
	// 2. Otherwise when implementing multi-cycle clocks, i_qspi_dat only
	//	changes on a negative edge
	//
	(* anyseq *) reg [3:0] dly_idat;
	always @(posedge i_clk)
	if (o_qspi_mod == NORMAL_SPI)
	begin
		assume(dly_idat[3:2] == 2'b11);
		assume(dly_idat[0] == o_qspi_dat[0]);
		if ((!OPT_ODDR)&&((o_qspi_sck)||(!$past(o_qspi_sck))))
			assume($stable(dly_idat[1]));
	end else if (o_qspi_mod == QUAD_WRITE)
		assume(dly_idat == o_qspi_dat);
	else if ((!OPT_ODDR)&&((o_qspi_sck)||(!$past(o_qspi_sck))))
		assume($stable(dly_idat));

	generate if (RDDELAY > 0)
	begin
		always @(posedge i_clk)
			assume(i_qspi_dat == $past(dly_idat,RDDELAY));
	end else begin
		always @(posedge i_clk)
			assume(i_qspi_dat == dly_idat);
	end endgenerate

	//
	////////////////////////////////////////////////////////////////////////
	//
	// Maintenance mode assertions
	//

	always @(*)
	if (maintenance)
	begin
		assume((!i_wb_stb)&&(!i_cfg_stb));

		assert(f_outstanding == 0);

		assert(o_wb_stall);
		//
		assert(clk_ctr == 0);
		assert(cfg_mode == 1'b0);
	end

	always @(*)
	if (maintenance)
	begin
		assert(clk_ctr == 0);
		assert(!o_wb_ack);
	end

	////////////////////////////////////////////////////////////////////////
	//
	//
	//
	always @(posedge i_clk)
	if (dly_ack)
		assert(clk_ctr[2:0] == 0);

	// Zero cycle requests
	always @(posedge i_clk)
	if ((f_past_valid)&&(!$past(i_reset))&&(($past(cfg_noop))
			||($past(i_wb_stb && i_wb_we && !o_wb_stall))))
		assert((dly_ack)&&((!i_wb_cyc)
			||(f_outstanding == 1 + f_extra)));

	always @(posedge i_clk)
	if ((f_outstanding > 0)&&(clk_ctr > 0))
		assert(pre_ack);

	always @(posedge i_clk)
	if ((i_wb_cyc)&&(dly_ack))
		assert(f_outstanding >= 1 + f_extra);

	always @(posedge i_clk)
	if ((f_past_valid)&&(clk_ctr == 0)&&(!dly_ack)
			&&((!$past(i_wb_stb|i_cfg_stb))||($past(o_wb_stall))))
		assert(f_outstanding == f_extra);

	always @(*)
	if ((i_wb_cyc)&&(pre_ack)&&(!o_qspi_cs_n))
		assert((f_outstanding >= 1 + f_extra)||((OPT_CFG)&&(cfg_mode)));

	always @(*)
	if ((cfg_mode)&&(!dly_ack)&&(clk_ctr == 0))
		assert(f_outstanding == f_extra);

	always @(*)
	if (cfg_mode)
		assert(f_outstanding <= 1 + f_extra);

	/////////////////
	//
	// Idle channel
	//
	/////////////////
	always @(*)
	if (!maintenance)
	begin
		if (o_qspi_cs_n)
		begin
			assert(clk_ctr == 0);
			assert(o_qspi_sck  == !OPT_ODDR);
		end else if (clk_ctr == 0)
			assert(o_qspi_sck == !OPT_ODDR);
	end

	always @(*)
		assert(o_qspi_mod != 2'b01);

	always @(*)
	if (clk_ctr > (5'h8 * (1+OPT_CLKDIV)))
	begin
		assert(!cfg_mode);
		assert(!cfg_cs);
	end


	always @(posedge i_clk)
	if ((OPT_CLKDIV==1)&&(!o_qspi_cs_n)&&(!$past(o_qspi_cs_n))
			&&(!$past(o_qspi_cs_n,2))&&(!cfg_mode))
		assert(o_qspi_sck != $past(o_qspi_sck));

	/////////////////
	//
	//  Read requests
	//
	/////////////////
	always @(posedge i_clk)
	if ((f_past_valid)&&(!$past(i_reset))&&($past(bus_request)))
	begin
		assert(!o_qspi_cs_n);
		if ((OPT_ODDR)||(!$past(pipe_req)))
			assert(o_qspi_sck == 1'b1);
		else
			assert(o_qspi_sck == 1'b0);
		//
		if (!$past(o_qspi_cs_n))
		begin
			assert(clk_ctr == 5'd8);
			assert(o_qspi_mod == QUAD_READ);
		end else begin
			assert(clk_ctr == 5'd14 + NDUMMY
				+ (OPT_ADDR32 ? 2:0) + (OPT_ODDR ? 0:1));
			assert(o_qspi_mod == QUAD_WRITE);
		end
	end

	always @(*)
		assert(clk_ctr <= 5'd18 + NDUMMY + (OPT_ODDR ? 0:1));

	always @(*)
	if ((OPT_ODDR)&&(!o_qspi_cs_n))
		assert((o_qspi_sck)||(actual_sck)||(cfg_mode)||(maintenance));

	always @(*)
	if ((RDDELAY == 0)&&((dly_ack)&&(clk_ctr == 0)))
		assert(!o_wb_stall);

	always @(*)
	if (!maintenance)
	begin
		if (cfg_mode)
		begin
			if (!cfg_cs)
				assert(o_qspi_cs_n);
			else if (!cfg_speed)
				assert(o_qspi_mod == NORMAL_SPI);
			else if ((cfg_dir)&&(clk_ctr > 0))
				assert(o_qspi_mod == QUAD_WRITE);
		end else if (clk_ctr > 5'd8)
			assert(o_qspi_mod == QUAD_WRITE);
		else if (clk_ctr > 0)
			assert(o_qspi_mod == QUAD_READ);
	end

	always @(posedge i_clk)
	if (((!OPT_PIPE)&&(clk_ctr != 0))||(clk_ctr > 5'd1))
		assert(o_wb_stall);

	always @(posedge i_clk)
	if ((OPT_CLKDIV>0)&&($past(o_qspi_cs_n)))
		assert(o_qspi_sck);

	/////////////////
	//
	//  User mode
	//
	/////////////////
	always @(*)
	if ((maintenance)||(!OPT_CFG))
		assert(!cfg_mode);
	always @(*)
	if ((OPT_CFG)&&(cfg_mode))
		assert(o_qspi_cs_n == !cfg_cs);
	else
		assert(!cfg_cs);

	//
	//
	//
	//
	always @(posedge i_clk)
	if (bus_request)
	begin
		// Make sure all of the bits are set
		fv_addr <= 0;
		// Now set as many bits as we have address bits
		fv_addr[AW-1:0] <= i_wb_addr;
	end

	always @(posedge i_clk)
	if ((i_wb_stb || i_cfg_stb) && !o_wb_stall && i_wb_we)
		fv_data <= i_wb_data;

	// Memory reads

	initial	f_memread = 0;
	generate if (RDDELAY == 0)
	begin

		always @(posedge i_clk)
		if (i_reset)
			f_memread <= 0;
		else begin
			if (ckstb)
				f_memread <= { f_memread[F_MEMACK-1:0],1'b0};
			else if (!OPT_ODDR)
				f_memread[F_MEMACK] <= 1'b0;
			if ((bus_request)&&(o_qspi_cs_n))
				f_memread[0] <= 1'b1;
		end
	end else begin

		always @(posedge i_clk)
		if (i_reset)
			f_memread <= 0;
		else begin
			if (ckstb)
				f_memread <= { f_memread[F_MEMACK-1:0],1'b0};
			else if (!OPT_ODDR)
				f_memread[F_MEMACK:F_MEMDONE]
					<= { f_memread[F_MEMACK-1:F_MEMDONE],1'b0};
			if ((bus_request)&&(o_qspi_cs_n))
				f_memread[0] <= 1'b1;
		end
	end endgenerate

	always @(posedge i_clk)
	if ((OPT_ODDR)&&(|f_memread[F_MEMDONE-1:0]))
		assert(o_qspi_sck);

	always @(posedge i_clk)
	if (|f_memread[6+(OPT_ADDR32 ? 2:0)+(OPT_ODDR ? 0:1):0])
		assert(o_qspi_mod == QUAD_WRITE);
	else if (|f_memread[(OPT_ODDR ? 0:1)+(OPT_ADDR32 ? 2:0) +7 +: NDUMMY])
	// begin assert(1); end
	begin end
	else if (|f_memread)
		assert(o_qspi_mod == QUAD_READ);

	generate if (RDDELAY > 0)
	begin
		always @(posedge i_clk)
		if ($past(ckpos,RDDELAY))
		begin
			if ($past(o_qspi_mod,RDDELAY) == NORMAL_SPI)
				f_past_data <= { f_past_data[31:0], i_qspi_dat[1] };
			else if ($past(o_qspi_mod,RDDELAY) == QUAD_READ)
				f_past_data <= { f_past_data[28:0], i_qspi_dat[3:0] };
		end
	end else begin
		always @(posedge i_clk)
		if (ckpos)
		begin
			if (o_qspi_mod == NORMAL_SPI)
				f_past_data <= { f_past_data[31:0], i_qspi_dat[1] };
			else if (o_qspi_mod == QUAD_READ)
				f_past_data <= { f_past_data[28:0], i_qspi_dat[3:0] };
		end
	end endgenerate


	always @(posedge i_clk)
	if (|f_memread[(OPT_ODDR ? 0:1) +: 7 + (OPT_ADDR32 ? 2:0)])
	begin
		if (OPT_ADDR32)
		begin
			// Eight extra bits of address
			if (f_memread[(OPT_ODDR ? 0:1)])
				assert(o_qspi_dat== fv_addr[29:26]);
			if (f_memread[1 + (OPT_ODDR ? 0:1)])
				assert(o_qspi_dat== fv_addr[25:22]);
		end
		// 6 nibbles of address, one nibble of mode
		if (f_memread[(OPT_ODDR ? 0:1)+(OPT_ADDR32 ? 2:0)])
			assert(o_qspi_dat== fv_addr[21:18]);
		if (f_memread[1+(OPT_ODDR ? 0:1)+(OPT_ADDR32 ? 2:0)])
			assert(o_qspi_dat== fv_addr[17:14]);
		if (f_memread[2+(OPT_ODDR ? 0:1)+(OPT_ADDR32 ? 2:0)])
			assert(o_qspi_dat== fv_addr[13:10]);
		if (f_memread[3+(OPT_ODDR ? 0:1)+(OPT_ADDR32 ? 2:0)])
			assert(o_qspi_dat== fv_addr[ 9: 6]);
		if (f_memread[4+(OPT_ODDR ? 0:1)+(OPT_ADDR32 ? 2:0)])
			assert(o_qspi_dat== fv_addr[ 5: 2]);
		if (f_memread[5+(OPT_ODDR ? 0:1)+(OPT_ADDR32 ? 2:0)])
			assert(o_qspi_dat=={ fv_addr[1:0],2'b00 });
		if (f_memread[6+(OPT_ODDR ? 0:1)+(OPT_ADDR32 ? 2:0)])
			assert(o_qspi_dat == 4'ha);
	end

	always @(posedge i_clk)
	if (OPT_ODDR)
	begin
		if (f_memread[F_MEMACK] && OPT_ENDIANSWAP)
		begin
			assert(o_wb_data[ 7: 4] == $past(i_qspi_dat,8));
			assert(o_wb_data[ 3: 0] == $past(i_qspi_dat,7));
			assert(o_wb_data[15:12] == $past(i_qspi_dat,6));
			assert(o_wb_data[11: 8] == $past(i_qspi_dat,5));
			assert(o_wb_data[23:20] == $past(i_qspi_dat,4));
			assert(o_wb_data[19:16] == $past(i_qspi_dat,3));
			assert(o_wb_data[31:28] == $past(i_qspi_dat,2));
			assert(o_wb_data[27:24] == $past(i_qspi_dat,1));
		end else if (f_memread[F_MEMACK])
		begin
			assert(o_wb_data[31:28] == $past(i_qspi_dat,8));
			assert(o_wb_data[27:24] == $past(i_qspi_dat,7));
			assert(o_wb_data[23:20] == $past(i_qspi_dat,6));
			assert(o_wb_data[19:16] == $past(i_qspi_dat,5));
			assert(o_wb_data[15:12] == $past(i_qspi_dat,4));
			assert(o_wb_data[11: 8] == $past(i_qspi_dat,3));
			assert(o_wb_data[ 7: 4] == $past(i_qspi_dat,2));
			assert(o_wb_data[ 3: 0] == $past(i_qspi_dat,1));
		end else if (|f_memread)
		begin
			if (!OPT_PIPE)
				assert(o_wb_stall);
			else if (!f_memread[F_MEMDONE-1])
				assert(o_wb_stall);
			assert(!o_wb_ack);
		end
	end else if (f_memread[F_MEMACK] && OPT_ENDIANSWAP)
		assert((!o_wb_ack)||(
			   o_wb_data[ 7: 0] == f_past_data[31:24]
			&& o_wb_data[15: 8] == f_past_data[23:16]
			&& o_wb_data[23:16] == f_past_data[15: 8]
			&& o_wb_data[31:24] == f_past_data[ 7: 0]));
	else if (f_memread[F_MEMACK]) // 25
		assert((!o_wb_ack)||(o_wb_data == f_past_data[31:0]));
	else if (|f_memread)
	begin
		if ((!OPT_PIPE)||(!ckstb))
			assert(o_wb_stall);
		else if (!f_memread[F_MEMDONE-1])
			assert(o_wb_stall);
		assert(!o_wb_ack);
	end

	always @(posedge i_clk)
	if (f_memread[F_MEMDONE])
		assert((clk_ctr == 0)||((OPT_PIPE)&&(clk_ctr == F_PIPEDONE)));
	// else if (|f_memread[F_MEMDONE-1:0])
	//	assert(f_memread[F_MEMDONE-clk_ctr]);

	generate for(k=0; k<F_MEMACK-1; k=k+1)
	begin : ONEHOT_MEMREAD
		always @(*)
		if (f_memread[k])
			assert((f_memread ^ (1<<k)) == 0);
	end endgenerate


	generate if (RDDELAY == 0)
	begin

		initial	f_piperead = 0;
		always @(posedge i_clk)
		if ((i_reset)||(!OPT_PIPE))
			f_piperead <= 0;
		else if (ckstb) begin
			f_piperead <= { f_piperead[F_PIPEACK-1:0],1'b0};
			f_piperead[0] <= (bus_request)&&(!o_qspi_cs_n);
		end else if (!OPT_ODDR)
			f_piperead[F_PIPEACK] <= 1'b0;

	end else begin

		initial	f_piperead = 0;
		always @(posedge i_clk)
		if ((i_reset)||(!OPT_PIPE))
			f_piperead <= 0;
		else if (ckstb) begin
			f_piperead <= { f_piperead[F_PIPEACK-1:0],1'b0};
			f_piperead[0] <= (bus_request)&&(!o_qspi_cs_n);
		end else if (!OPT_ODDR)
			f_piperead[F_PIPEACK:F_PIPEDONE] <= { f_piperead[F_PIPEACK-1:F_PIPEDONE], 1'b0 };

	end endgenerate

	always @(posedge  i_clk)
	if (OPT_ODDR)
	begin
		if (f_piperead[F_PIPEACK] && OPT_ENDIANSWAP)
		begin
			assert(o_wb_data[ 7: 4] == $past(i_qspi_dat,8));
			assert(o_wb_data[ 3: 0] == $past(i_qspi_dat,7));
			assert(o_wb_data[15:12] == $past(i_qspi_dat,6));
			assert(o_wb_data[11: 8] == $past(i_qspi_dat,5));
			assert(o_wb_data[23:20] == $past(i_qspi_dat,4));
			assert(o_wb_data[19:16] == $past(i_qspi_dat,3));
			assert(o_wb_data[31:28] == $past(i_qspi_dat,2));
			assert(o_wb_data[27:24] == $past(i_qspi_dat,1));
		end else if (f_piperead[F_PIPEACK])
		begin
			assert(o_wb_data[31:28] == $past(i_qspi_dat,8));
			assert(o_wb_data[27:24] == $past(i_qspi_dat,7));
			assert(o_wb_data[23:20] == $past(i_qspi_dat,6));
			assert(o_wb_data[19:16] == $past(i_qspi_dat,5));
			assert(o_wb_data[15:12] == $past(i_qspi_dat,4));
			assert(o_wb_data[11: 8] == $past(i_qspi_dat,3));
			assert(o_wb_data[ 7: 4] == $past(i_qspi_dat,2));
			assert(o_wb_data[ 3: 0] == $past(i_qspi_dat));
		end else if ((|f_piperead)&&(!f_piperead[RDDELAY]))
			assert(!o_wb_ack);
	end else if (f_piperead[F_PIPEACK] && OPT_ENDIANSWAP)
	begin
		assert((!o_wb_ack)||(
			   o_wb_data[ 7: 0] == f_past_data[31:24]
			&& o_wb_data[15: 8] == f_past_data[23:16]
			&& o_wb_data[23:16] == f_past_data[15: 8]
			&& o_wb_data[31:24] == f_past_data[ 7: 0]));
	end else if (f_piperead[F_PIPEACK]) // 25
		assert((!o_wb_ack)||(o_wb_data == f_past_data[31:0]));
	else if (|f_piperead)
	begin
		if ((!OPT_PIPE)||(!ckstb))
			assert(o_wb_stall);
		else if (!f_piperead[F_PIPEDONE-1])
			assert(o_wb_stall);
		if (!f_memread[F_MEMACK])
			assert(!o_wb_ack);
	end

	always @(posedge i_clk)
	if (f_piperead[F_PIPEDONE])
		assert(clk_ctr == 0 || clk_ctr == F_PIPEDONE);
	else if (|f_piperead[F_PIPEDONE-1:0])
		assert(f_piperead[F_PIPEDONE-clk_ctr]);

	always @(*)
	if (i_cfg_stb && !o_wb_stall)
	begin
		assert(|{ cfg_noop, cfg_hs_write, cfg_hs_read, cfg_ls_write });

		if (cfg_noop)
			assert({ cfg_hs_write, cfg_hs_read, cfg_ls_write }==0);
		else if (cfg_hs_write)
			assert({ cfg_hs_read, cfg_ls_write }==0);
		else if (cfg_hs_read)
			assert({ cfg_ls_write }==0);
	end
	////////////////////////////////////////////////////////////////////////
	//
	// Lowspeed config write
	//
	initial	f_cfglswrite = 0;
	always @(posedge i_clk)
	if (i_reset)
		f_cfglswrite <= 0;
	else begin
		if (ckstb)
			f_cfglswrite <= { f_cfglswrite[F_CFGLSACK-1:0], 1'b0 };
		else if (!OPT_ODDR)
			f_cfglswrite[F_CFGLSACK:F_CFGLSDONE]
				<= { f_cfglswrite[F_CFGLSACK:F_CFGLSDONE],1'b0 };
		if (cfg_ls_write)
			f_cfglswrite[0] <= 1'b1;
	end

	always @(*)
	if (OPT_ODDR)
	begin
		if (|f_cfglswrite[7:0])
			assert(o_qspi_sck);
		else if (|f_cfglswrite)
			assert(!o_qspi_sck);
	end

	always @(posedge i_clk)
	if (|f_cfglswrite[7:0])
	begin
		assert(!dly_ack);
		if (f_cfglswrite[F_CFGLSDONE-8])
		begin
			assert(o_qspi_dat[0] == fv_data[7]);
			assert(clk_ctr == 8);
		end
		if (f_cfglswrite[F_CFGLSDONE-7])
		begin
			assert(o_qspi_dat[0] == fv_data[6]);
			assert(clk_ctr == 7);
		end
		if (f_cfglswrite[F_CFGLSDONE-6])
		begin
			assert(o_qspi_dat[0] == fv_data[5]);
			assert(clk_ctr == 6);
		end
		if (f_cfglswrite[F_CFGLSDONE-5])
		begin
			assert(o_qspi_dat[0] == fv_data[4]);
			assert(clk_ctr == 5);
		end
		if (f_cfglswrite[F_CFGLSDONE-4])
		begin
			assert(o_qspi_dat[0] == fv_data[3]);
			assert(clk_ctr == 4);
		end
		if (f_cfglswrite[F_CFGLSDONE-3])
		begin
			assert(o_qspi_dat[0] == fv_data[2]);
			assert(clk_ctr == 3);
		end
		if (f_cfglswrite[F_CFGLSDONE-2])
		begin
			assert(o_qspi_dat[0] == fv_data[1]);
			assert(clk_ctr == 2);
		end
		if (f_cfglswrite[F_CFGLSDONE-1])
		begin
			assert(o_qspi_dat[0] == fv_data[0]);
			assert(clk_ctr == 1);
		end
	end

	always @(posedge i_clk)
	if (|f_cfglswrite)
	begin
		assert(!o_qspi_cs_n);
		assert(o_qspi_mod == NORMAL_SPI);
	end

	always @(posedge i_clk)
	if (|f_cfglswrite[F_CFGLSACK:F_CFGLSDONE])
		assert(o_qspi_sck == !OPT_ODDR);

	always @(posedge i_clk)
	if ((OPT_ODDR)&&(f_cfglswrite[F_CFGLSACK]))
	begin
		assert((o_wb_ack)||(!$past(pre_ack))||(!$past(i_wb_cyc)));
		assert(o_wb_data[7] == $past(i_qspi_dat[1],8));
		assert(o_wb_data[6] == $past(i_qspi_dat[1],7));
		assert(o_wb_data[5] == $past(i_qspi_dat[1],6));
		assert(o_wb_data[4] == $past(i_qspi_dat[1],5));
		assert(o_wb_data[3] == $past(i_qspi_dat[1],4));
		assert(o_wb_data[2] == $past(i_qspi_dat[1],3));
		assert(o_wb_data[1] == $past(i_qspi_dat[1],2));
		assert(o_wb_data[0] == $past(i_qspi_dat[1],1));
		assert(!o_wb_stall);
	end else if (f_cfglswrite[F_CFGLSACK])
		assert(o_wb_data[7:0] == f_past_data[7:0]);
	else if (|f_cfglswrite[F_CFGLSACK-1:0])
		assert(o_wb_stall);

	////////////////////////////////////////////////////////////////////////
	//
	// High speed config write
	//
	generate if (RDDELAY == 0)
	begin
		initial	f_cfghswrite = 0;
		always @(posedge i_clk)
		if (i_reset)
			f_cfghswrite <= 0;
		else begin
			if (ckstb)
				f_cfghswrite <= { f_cfghswrite[F_CFGHSACK-1:0], 1'b0 };
			else if (!OPT_ODDR)
				f_cfghswrite[F_CFGHSACK] <= 1'b0;
			if (cfg_hs_write)
				f_cfghswrite[0] <= 1'b1;
		end

	end else begin

		initial	f_cfghswrite = 0;
		always @(posedge i_clk)
		if (i_reset)
			f_cfghswrite <= 0;
		else begin
			if (ckstb)
				f_cfghswrite <= { f_cfghswrite[F_CFGHSACK-1:0], 1'b0 };
			else if (!OPT_ODDR)
				f_cfghswrite[F_CFGHSACK:F_CFGHSDONE]
			  	<= { f_cfghswrite[F_CFGHSACK:F_CFGHSDONE], 1'b0 };
			if (cfg_hs_write)
				f_cfghswrite[0] <= 1'b1;
		end
	end endgenerate

	always @(*)
	if (OPT_ODDR)
	begin
		if (|f_cfghswrite[F_CFGHSDONE-1:0])
			assert(o_qspi_sck);
		else if (|f_cfghswrite)
			assert(!o_qspi_sck);
	end

	always @(posedge i_clk)
	if ((OPT_ODDR) && (f_cfghswrite[0]))
		assert(o_qspi_dat == $past(i_wb_data[7:4]));
	else if ((!OPT_ODDR) && (f_cfghswrite[1]))
		assert(o_qspi_dat == fv_data[7:4]);

	always @(posedge i_clk)
	if ((OPT_ODDR)&&f_cfghswrite[1])
		assert(o_qspi_dat == $past(i_wb_data[3:0],2));
	else if ((!OPT_ODDR) && (f_cfghswrite[2]))
		assert(o_qspi_dat == fv_data[3:0]);

	always @(posedge i_clk)
	if (OPT_ODDR)
	begin
		if (|f_cfghswrite[F_CFGHSDONE-1:0])
			assert(o_qspi_sck);
		else if (|f_cfghswrite)
			assert(!o_qspi_sck);
	end

	generate if (RDDELAY > 0)
	begin

		always @(posedge i_clk)
		if (|f_cfghswrite[F_CFGHSACK:F_CFGHSDONE])
			assert(!actual_sck);

	end endgenerate

	always @(posedge i_clk)
	if (|f_cfghswrite)
	begin
		assert(!o_qspi_cs_n);
		assert(o_qspi_mod == QUAD_WRITE);
	end

	always @(posedge i_clk)
	if (|f_cfghswrite[1:0])
	begin
		assert(!dly_ack);
		assert(o_wb_stall);
	end

	always @(posedge i_clk)
	if (f_cfghswrite[F_CFGHSACK])
	begin
		assert((o_wb_ack)||(!$past(pre_ack))||(!$past(i_wb_cyc)));
		assert(o_qspi_mod == QUAD_WRITE);
		assert(!o_wb_stall);
	end else if (|f_cfghswrite)
	begin
		assert(o_wb_stall);
		assert(!o_wb_ack);
	end

	////////////////////////////////////////////////////////////////////////
	//
	// High speed config read
	//
	initial	f_cfghsread = 0;
	always @(posedge i_clk)
	if (i_reset)
		f_cfghsread <= 0;
	else begin
		if (ckstb)
			f_cfghsread <= { f_cfghsread[F_CFGHSACK-1:0], 1'b0 };
		else if (!OPT_ODDR)
			f_cfghsread[F_CFGHSACK:F_CFGHSDONE]
				<={f_cfghsread[F_CFGHSACK:F_CFGHSDONE], 1'b0};
		if (cfg_hs_read)
			f_cfghsread[0] <= 1'b1;
	end

	always @(*)
	if (OPT_ODDR)
	begin
		if (|f_cfghsread[1:0])
			assert(o_qspi_sck);
		else if (|f_cfghsread)
			assert(!o_qspi_sck);
	end

	always @(posedge i_clk)
	if (|f_cfghsread[F_CFGHSACK:F_CFGHSDONE])
		assert(o_qspi_sck == !OPT_ODDR);

	always @(*)
	if ((!maintenance)&&(o_qspi_cs_n))
		assert(!actual_sck);

	always @(posedge i_clk)
	if (|f_cfghsread[F_CFGHSDONE-1:F_CFGHSDONE-2])
	begin
		assert(!dly_ack);
		assert(!o_qspi_cs_n);
		assert(o_qspi_mod == QUAD_READ);
		assert(o_wb_stall);
	end

	generate if (RDDELAY > 0)
	begin

		always @(posedge i_clk)
		if (|f_cfghswrite[F_CFGHSACK:F_CFGHSDONE])
			assert(!actual_sck);

	end endgenerate


	always @(posedge i_clk)
	if (f_cfghsread[F_CFGHSACK])
	begin
		if (OPT_ODDR)
		begin
		assert(o_wb_data[7:4] == $past(i_qspi_dat[3:0],2));
		assert(o_wb_data[3:0] == $past(i_qspi_dat[3:0],1));
		end
		assert(o_wb_data[7:0] == f_past_data[7:0]);
		assert((o_wb_ack)||(!$past(pre_ack))||(!$past(i_wb_cyc)));
		assert(o_qspi_mod == QUAD_READ);
		assert(!o_wb_stall);
	end else if (|f_cfghsread)
	begin
		assert(o_wb_stall);
		assert(!o_wb_ack);
	end

	////////////////////////////////////////////////////////////////////////
	//
	// Crossover checks
	//
	wire	f_qspi_not_done, f_qspi_not_ackd, f_qspi_active, f_qspi_ack;
	assign	f_qspi_not_done =
			(|f_memread[F_MEMDONE-1:0])
			||(|f_piperead[F_PIPEDONE-1:0])
			||(|f_cfglswrite[F_CFGLSDONE-1:0])
			||(|f_cfghswrite[F_CFGHSDONE-1:0])
			||(|f_cfghsread[F_CFGHSDONE-1:0]);
	assign	f_qspi_active = (!maintenance)&&(
			(|f_memread[F_MEMACK-1:0])
			||(|f_piperead[F_PIPEACK-1:0])
			||(|f_cfglswrite[F_CFGLSACK-1:0])
			||(|f_cfghswrite[F_CFGHSACK-1:0])
			||(|f_cfghsread[F_CFGHSACK-1:0]));
	assign	f_qspi_not_ackd = (!maintenance)&&(!f_qspi_not_done)&&(
			(|f_memread[F_MEMACK-1:0])
			||(|f_piperead[F_PIPEACK-1:0])
			||(|f_cfglswrite[F_CFGLSACK-1:0])
			||(|f_cfghswrite[F_CFGHSACK-1:0])
			||(|f_cfghsread[F_CFGHSACK-1:0]));
	assign	f_qspi_ack = (!maintenance)&&
			(|f_memread[F_MEMACK:0])
			||(|f_piperead[F_PIPEACK:0])
			||(|f_cfglswrite[F_CFGLSACK:0])
			||(|f_cfghswrite[F_CFGHSACK:0])
			||(|f_cfghsread[F_CFGHSACK:0]);

	always @(*)
	begin
		if ((|f_memread[F_MEMDONE:0])||(|f_piperead[F_PIPEDONE:0]))
		begin
			assert(f_cfglswrite  == 0);
			assert(f_cfghswrite == 0);
			assert(f_cfghsread  == 0);
		end

		if (|f_cfglswrite[F_CFGLSDONE:0])
		begin
			assert(f_cfghswrite[F_CFGHSDONE:0] == 0);
			assert(f_cfghsread[F_CFGHSDONE:0] == 0);
		end

		if (|f_cfghswrite[F_CFGHSDONE:0])
			assert(f_cfghsread[F_CFGHSDONE:0] == 0);

		if (maintenance)
		begin
			assert(f_memread    == 0);
			assert(f_piperead   == 0);
			assert(f_cfglswrite == 0);
			assert(f_cfghswrite == 0);
			assert(f_cfghsread  == 0);
		end

		assert(clk_ctr <= F_MEMDONE);
	end

	always @(posedge i_clk)
	if ((f_past_valid)&&(!f_qspi_ack)&&(!$past(i_reset))
		&&(!$past(maintenance)))
	begin
		assert($stable(o_wb_data[7:0]));
		if (!cfg_mode && !$past(cfg_mode)
				&& !$past(i_cfg_stb && !o_wb_stall)
				&&($past(f_past_valid))
				&& !$past(i_cfg_stb && !o_wb_stall,2))
			assert($stable(o_wb_data));
	end

	always @(*)
	if (!maintenance && actual_sck)
	begin
		assert(f_qspi_not_done);
		/*
		assert((|f_memread[F_MEMDONE:0])
			||(|f_piperead[F_PIPEDONE:0])
			||(|f_cfglswrite[F_CFGLSDONE:0])
			||(|f_cfghswrite[F_CFGHSDONE:0])
			||(|f_cfghsread[F_CFGHSDONE:0]));
			*/
	end

	always @(*)
	if (!maintenance && !o_qspi_cs_n && !cfg_mode)
	begin
		assert((|f_memread[F_MEMDONE:0])
			||(|f_piperead[F_PIPEDONE:0]));
	end else if (!maintenance && cfg_mode)
	begin
		if ((o_qspi_sck == OPT_ODDR)||(clk_ctr > 0)||(actual_sck))
		begin
			assert( (|f_cfglswrite[F_CFGLSDONE-1:0])
				||(|f_cfghswrite[F_CFGHSDONE-1:0])
				||(|f_cfghsread[F_CFGHSDONE-1:0]));
		end
	end

	always @(posedge i_clk)
	if (o_wb_ack && !$past((i_wb_stb || i_cfg_stb)&&!o_wb_stall, 1+RDDELAY))
	begin
		assert(f_memread[F_MEMACK]
			|| f_piperead[F_PIPEACK]
			|| f_cfglswrite[F_CFGLSACK]
			|| f_cfghswrite[F_CFGHSACK]
			|| f_cfghsread[F_CFGHSACK]);
	end


	////////////////////////////////////////////////////////////////////////
	//
	// Cover Properties
	//
	////////////////////////////////////////////////////////////////////////
	//
	// Due to the way the chip starts up, requiring 32k+ maintenance clocks,
	// these cover statements are not likely to be hit

	generate if (!OPT_STARTUP)
	begin
		// Why is this generate only if !OPT_STARTUP?  For performance
		// reasons.  The startup sequence can take a great many clocks.
		// cover() would never be able to work through all of those
		// clocks to find the following examples, and so it would fail.
		// By only checking these cover() statements if !OPT_STARTUP,
		// we give them an opportunity to succeed
		always @(posedge i_clk)
			cover(o_wb_ack && f_memread[ F_MEMACK]);

		if (OPT_CFG)
		begin
			always @(posedge i_clk)
			begin
			cover(cfg_ls_write);
			cover(cfg_hs_write);
			cover(cfg_hs_read);

			cover(|f_cfglswrite);
			cover(|f_cfghsread);
			cover(|f_cfghswrite);

			cover(o_wb_ack && |f_cfglswrite);
			cover(o_wb_ack && |f_cfghsread);
			cover(o_wb_ack && |f_cfghswrite);

			cover(f_cfglswrite[0]);
			cover(f_cfghsread[ 0]);
			cover(f_cfghswrite[0]);

			cover(f_cfglswrite[F_CFGLSACK-2]);
			cover(f_cfghsread[ F_CFGHSACK-2]);
			cover(f_cfghswrite[F_CFGHSACK-2]);

			cover(f_cfglswrite[F_CFGLSACK-1]);
			cover(f_cfghsread[ F_CFGHSACK-1]);
			cover(f_cfghswrite[F_CFGHSACK-1]);

			cover(f_cfglswrite[F_CFGLSACK]);
			cover(f_cfghsread[ F_CFGHSACK]);
			cover(f_cfghswrite[F_CFGHSACK]);

			cover(o_wb_ack && f_cfglswrite[F_CFGLSACK]);
			cover(o_wb_ack && f_cfghsread[ F_CFGHSACK]);
			cover(o_wb_ack && f_cfghswrite[F_CFGHSACK]);
			end
		end

		if (OPT_PIPE)
		begin
			always @(posedge i_clk)
				cover(o_wb_ack && f_piperead[F_PIPEACK]);
		end
		//
		//
		//
		always @(posedge i_clk)
			cover((o_wb_ack)&&(!cfg_mode));
		always @(posedge i_clk)
			cover((o_wb_ack)&&(!cfg_mode)&&(!$past(o_qspi_cs_n)));
		always @(posedge i_clk)
			// Cover a piped transaction
			cover((o_wb_ack)&&(!cfg_mode)&&(!o_qspi_cs_n));	//!
		always @(posedge i_clk)
			cover((o_wb_ack)&&(cfg_mode)&&(cfg_speed));
		always @(posedge i_clk)
			cover((o_wb_ack)&&(cfg_mode)&&(!cfg_speed)&&(cfg_dir));
		always @(posedge i_clk)
			cover((o_wb_ack)&&(cfg_mode)&&(!cfg_speed)&&(!cfg_dir));
	end else begin

		always @(posedge i_clk)
			cover(!maintenance);

	end endgenerate

`endif
endmodule
