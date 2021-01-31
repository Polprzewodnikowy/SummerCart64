# Clocks

derive_pll_clocks -create_base_clocks

set sys_clk {sys_pll|altpll_component|auto_generated|pll1|clk[0]}
set sdram_pll_clk {sys_pll|altpll_component|auto_generated|pll1|clk[1]}

create_generated_clock -name flash_se_neg_reg -divide_by 2 -source [get_pins -compatibility_mode {*altera_onchip_flash:*onchip_flash_0|altera_onchip_flash_avmm_data_controller:avmm_data_controller|flash_se_neg_reg|clk}] [get_pins -compatibility_mode {*altera_onchip_flash:*onchip_flash_0|altera_onchip_flash_avmm_data_controller:avmm_data_controller|flash_se_neg_reg|q}]
create_generated_clock -name sdram_clk -source [get_pins $sdram_pll_clk] -master_clock $sdram_pll_clk [get_ports o_sdram_clk]
create_generated_clock -name ftdi_clk -divide_by 2 -source [get_pins $sys_clk] -master_clock $sys_clk [get_registers {usb_pc_inst|usb_ftdi_fsi_inst|o_ftdi_clk}]
create_generated_clock -name sd_clk -divide_by 2 -source [get_pins $sys_clk] -master_clock $sys_clk [get_registers {sd_interface_inst|o_sd_clk}]

derive_clock_uncertainty


# SDRAM timings

set_input_delay -clock sdram_clk -max 5.4 [get_ports {io_sdram_dq[*]}]
set_input_delay -clock sdram_clk -min 2.5 [get_ports {io_sdram_dq[*]}]

set_output_delay -clock sdram_clk -max 1.5 [get_ports {o_sdram_cs o_sdram_ras o_sdram_cas o_sdram_we o_sdram_a[*] o_sdram_ba[*] io_sdram_dq[*]}]
set_output_delay -clock sdram_clk -min -0.8 [get_ports {o_sdram_cs o_sdram_ras o_sdram_cas o_sdram_we o_sdram_a[*] o_sdram_ba[*] io_sdram_dq[*]}]

set_multicycle_path -from [get_clocks {sdram_clk}] -to [get_clocks $sys_clk] -setup -end 2


# False paths

set_false_path -from [get_clocks $sys_clk] -to [get_ports {o_ftdi_si}]
set_false_path -from [get_ports {i_ftdi_so}] -to [get_clocks $sys_clk]
set_false_path -from [get_ports {i_ftdi_cts}] -to [get_clocks $sys_clk]

set_false_path -from [get_ports {i_n64_reset}] -to [get_clocks $sys_clk]
set_false_path -from [get_ports {i_n64_nmi}] -to [get_clocks $sys_clk]

set_false_path -from [get_ports {i_n64_pi_alel}] -to [get_clocks $sys_clk]
set_false_path -from [get_ports {i_n64_pi_aleh}] -to [get_clocks $sys_clk]
set_false_path -from [get_ports {i_n64_pi_read}] -to [get_clocks $sys_clk]
set_false_path -from [get_ports {i_n64_pi_write}] -to [get_clocks $sys_clk]
set_false_path -from [get_ports {io_n64_pi_ad[*]}] -to [get_clocks $sys_clk]
set_false_path -from [get_clocks $sys_clk] -to [get_ports {io_n64_pi_ad[*]}]

set_false_path -from [get_ports {i_n64_si_clk}] -to [get_clocks $sys_clk]
set_false_path -from [get_ports {io_n64_si_dq}] -to [get_clocks $sys_clk]
set_false_path -from [get_clocks $sys_clk] -to [get_ports {io_n64_si_dq}]

set_false_path -from [get_clocks $sys_clk] -to [get_ports {io_sd_cmd}]
set_false_path -from [get_clocks $sys_clk] -to [get_ports {io_sd_dat[*]}]
set_false_path -from [get_ports {io_sd_cmd}] -to [get_clocks $sys_clk]
set_false_path -from [get_ports {io_sd_dat[*]}] -to [get_clocks $sys_clk]

set_false_path -from [get_clocks $sys_clk] -to [get_ports {o_led}]

set_false_path -from [get_ports {io_pmod[*]}] -to [get_clocks $sys_clk]
set_false_path -from [get_clocks $sys_clk] -to [get_ports {io_pmod[*]}]
