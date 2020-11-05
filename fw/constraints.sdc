create_clock -name i_clk -period 20 [get_ports i_clk]

derive_pll_clocks -create_base_clocks

derive_clock_uncertainty
