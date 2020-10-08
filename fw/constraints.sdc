create_clock -name i_clk -period 20 [get_ports i_clk]

create_clock -name i_ftdi_clk -period 33.333 [get_ports i_ftdi_clk]

derive_pll_clocks -create_base_clocks

derive_clock_uncertainty
