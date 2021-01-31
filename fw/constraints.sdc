# Clocks

derive_pll_clocks -create_base_clocks

set sys_clk {sys_pll|altpll_component|auto_generated|pll1|clk[0]}
set sdram_pll_clk {sys_pll|altpll_component|auto_generated|pll1|clk[1]}

create_generated_clock -name sdram_clk -source [get_pins $sdram_pll_clk] -master_clock $sdram_pll_clk [get_ports {o_sdram_clk}]
create_generated_clock -name flash_se_neg_reg -divide_by 2 \
    -source [get_pins -compatibility_mode {*altera_onchip_flash:*onchip_flash_0|altera_onchip_flash_avmm_data_controller:avmm_data_controller|flash_se_neg_reg|clk}] \
    [get_pins -compatibility_mode {*altera_onchip_flash:*onchip_flash_0|altera_onchip_flash_avmm_data_controller:avmm_data_controller|flash_se_neg_reg|q}]

derive_clock_uncertainty


# SDRAM timings

set sdram_outputs {o_sdram_cs o_sdram_ras o_sdram_cas o_sdram_we o_sdram_a[*] o_sdram_ba[*] io_sdram_dq[*]}
set sdram_inputs {io_sdram_dq[*]}

set_output_delay -clock [get_clocks {sdram_clk}] -max 1.5 [get_ports $sdram_outputs]
set_output_delay -clock [get_clocks {sdram_clk}] -min -0.8 [get_ports $sdram_outputs]

set_input_delay -clock [get_clocks {sdram_clk}] -max 5.4 [get_ports $sdram_inputs]
set_input_delay -clock [get_clocks {sdram_clk}] -min 2.5 [get_ports $sdram_inputs]

set_multicycle_path -setup -end 2 -from [get_clocks {sdram_clk}] -to [get_clocks $sys_clk]


# FTDI timings

set_false_path -to [get_ports {o_ftdi_clk o_ftdi_si}]
set_false_path -from [get_ports {i_ftdi_so i_ftdi_cts}]


# SD card timings

set_false_path -to [get_ports {o_sd_clk io_sd_cmd io_sd_dat[*]}]
set_false_path -from [get_ports {io_sd_cmd io_sd_dat[*]}]


# N64, PI and SI timings

set_false_path -from [get_ports {i_n64_reset i_n64_nmi}]

set_false_path -to [get_ports {io_n64_pi_ad[*]}]
set_false_path -from [get_ports {i_n64_pi_* io_n64_pi_ad[*]}]

set_false_path -to [get_ports {io_n64_si_dq}]
set_false_path -from [get_ports {i_n64_si_clk io_n64_si_dq}]


# LED timings

set_false_path -to [get_ports {o_led}]


# PMOD timings

set_false_path -to [get_ports {io_pmod[*]}]
set_false_path -from [get_ports {io_pmod[*]}]
