# Clocks

create_clock -name {i_clk} -period 20.000 [get_ports {i_clk}]

create_generated_clock -name flash_se_neg_reg -divide_by 2 -source [get_pins -compatibility_mode {*altera_onchip_flash:*onchip_flash_0|altera_onchip_flash_avmm_data_controller:avmm_data_controller|flash_se_neg_reg|clk}] [get_pins -compatibility_mode {*altera_onchip_flash:*onchip_flash_0|altera_onchip_flash_avmm_data_controller:avmm_data_controller|flash_se_neg_reg|q}]

set w_sys_clk {sys_pll|altpll_component|auto_generated|pll1|clk[0]}
set w_sdram_clk {sys_pll|altpll_component|auto_generated|pll1|clk[1]}

derive_pll_clocks
derive_clock_uncertainty


# SDRAM timings

set_false_path -from [get_clocks $w_sdram_clk] -to [get_ports {o_sdram_clk}]

set_input_delay -clock $w_sdram_clk -max 5.9 {io_sdram_dq[*]}
set_input_delay -clock $w_sdram_clk -min 0.5 {io_sdram_dq[*]}

set_output_delay -clock $w_sdram_clk -max 1.5 {o_sdram_cs o_sdram_ras o_sdram_cas o_sdram_we o_sdram_a[*] o_sdram_ba[*] io_sdram_dq[*]}
set_output_delay -clock $w_sdram_clk -min -0.8 {o_sdram_cs o_sdram_ras o_sdram_cas o_sdram_we o_sdram_a[*] o_sdram_ba[*] io_sdram_dq[*]}

set_multicycle_path -from [get_clocks $w_sys_clk] -to [get_clocks $w_sdram_clk] -setup -end 2


# Asynchronous logic

set_false_path -from [get_clocks {*}] -to [get_ports {o_ftdi_clk}]
set_false_path -from [get_clocks {*}] -to [get_ports {o_ftdi_si}]
set_false_path -from [get_ports {i_ftdi_so}] -to [get_clocks {*}]
set_false_path -from [get_ports {i_ftdi_cts}] -to [get_clocks {*}]

set_false_path -from [get_ports {i_n64_reset}] -to [get_clocks {*}]
set_false_path -from [get_ports {i_n64_nmi}] -to [get_clocks {*}]

set_false_path -from [get_ports {i_n64_pi_alel}] -to [get_clocks {*}]
set_false_path -from [get_ports {i_n64_pi_aleh}] -to [get_clocks {*}]
set_false_path -from [get_ports {i_n64_pi_read}] -to [get_clocks {*}]
set_false_path -from [get_ports {i_n64_pi_write}] -to [get_clocks {*}]
set_false_path -from [get_ports {io_n64_pi_ad[*]}] -to [get_clocks {*}]
set_false_path -from [get_clocks {*}] -to [get_ports {io_n64_pi_ad[*]}]

set_false_path -from [get_ports {i_n64_si_clk}] -to [get_clocks {*}]
set_false_path -from [get_ports {io_n64_si_dq}] -to [get_clocks {*}]
set_false_path -from [get_clocks {*}] -to [get_ports {io_n64_si_dq}]

set_false_path -from [get_clocks {*}] -to [get_ports {o_led}]

set_false_path -from [get_ports {io_pmod[*]}] -to [get_clocks {*}]
set_false_path -from [get_clocks {*}] -to [get_ports {io_pmod[*]}]
