# Clocks

derive_pll_clocks -create_base_clocks

set sys_clk {system_inst|intel_pll_inst|altpll_component|auto_generated|pll1|clk[0]}
set sdram_pll_clk {system_inst|intel_pll_inst|altpll_component|auto_generated|pll1|clk[1]}
# set sd_reg_clk {sd_interface_inst|sd_clk_inst|o_sd_clk|q}

create_generated_clock -name sdram_clk -source [get_pins $sdram_pll_clk] [get_ports {o_sdram_clk}]
create_clock -name usb_clk -period 40.0 [get_ports {o_usb_clk}]
# create_generated_clock -name sd_reg_clk -source [get_pins {sd_interface_inst|sd_clk_inst|o_sd_clk|clk}] -divide_by 2 [get_pins $sd_reg_clk]
# create_generated_clock -name sd_clk -source [get_pins $sd_reg_clk] [get_ports {o_sd_clk}]
create_generated_clock -name flash_se_neg_reg \
    -source [get_pins -compatibility_mode {*altera_onchip_flash:*onchip_flash_0|altera_onchip_flash_avmm_data_controller:avmm_data_controller|flash_se_neg_reg|clk}] \
    -divide_by 2 \
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
set_multicycle_path -hold -end 1 -from [get_clocks {sdram_clk}] -to [get_clocks $sys_clk]


# SD card timings

# set_output_delay -clock [get_clocks {sd_clk}] -max 6.0 [get_ports {io_sd_cmd io_sd_dat[*]}]
# set_output_delay -clock [get_clocks {sd_clk}] -min -2.0 [get_ports {io_sd_cmd io_sd_dat[*]}]

# set_input_delay -clock [get_clocks {sd_clk}] -max 15.0 [get_ports {io_sd_cmd io_sd_dat[*]}]
# set_input_delay -clock [get_clocks {sd_clk}] -min 6.5 [get_ports {io_sd_cmd io_sd_dat[*]}]

# set_multicycle_path -hold -start 1 -from [get_clocks $sys_clk] -to [get_clocks {sd_clk}]

# set_multicycle_path -setup -end 3 -from [get_clocks {sd_clk}] -to [get_clocks $sys_clk]
# set_multicycle_path -hold -end 1 -from [get_clocks {sd_clk}] -to [get_clocks $sys_clk]


# FT1248 timings

set_output_delay -clock [get_clocks {usb_clk}] -max 5.0 [get_ports {io_usb_miosi[*] o_usb_cs}]
set_output_delay -clock [get_clocks {usb_clk}] -min -5.0 [get_ports {io_usb_miosi[*] o_usb_cs}]

set_input_delay -clock [get_clocks {usb_clk}] -max 5.0 [get_ports {io_usb_miosi[*] i_usb_miso}]
set_input_delay -clock [get_clocks {usb_clk}] -min 5.0 [get_ports {io_usb_miosi[*] i_usb_miso}]

set_multicycle_path -setup -start 2 -from [get_clocks $sys_clk] -to [get_clocks {usb_clk}]
set_multicycle_path -hold -start 3 -from [get_clocks $sys_clk] -to [get_clocks {usb_clk}]

set_multicycle_path -setup -end 2 -from [get_clocks {usb_clk}] -to [get_clocks $sys_clk]
set_multicycle_path -hold -end 3 -from [get_clocks {usb_clk}] -to [get_clocks $sys_clk]


# N64, PI and SI timings

set_false_path -to [get_ports {o_n64_irq}]
set_false_path -from [get_ports {i_n64_reset i_n64_nmi}]

set_false_path -to [get_ports {io_n64_pi_ad[*]}]
set_false_path -from [get_ports {i_n64_pi_* io_n64_pi_ad[*]}]

set_false_path -to [get_ports {io_n64_si_dq}]
set_false_path -from [get_ports {i_n64_si_clk io_n64_si_dq}]


# LED timings

set_false_path -to [get_ports {o_led}]


# UART timings

set_false_path -to [get_ports {o_uart_txd}]
set_false_path -from [get_ports {i_uart_rxd}]


# I2C timings

set_false_path -to [get_ports {o_rtc_scl io_rtc_sda}]
set_false_path -from [get_ports {io_rtc_sda}]
