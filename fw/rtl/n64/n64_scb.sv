interface n64_scb ();

    logic n64_reset;
    logic n64_nmi;

    logic bootloader_enabled;
    logic rom_write_enabled;
    logic rom_shadow_enabled;
    logic rom_extended_enabled;
    logic sram_enabled;
    logic sram_banked;
    logic flashram_enabled;
    logic dd_enabled;
    logic ddipl_enabled;
    logic eeprom_enabled;
    logic eeprom_16k_mode;

    logic dd_write;
    logic [6:0] dd_address;
    logic [15:0] dd_rdata;
    logic [15:0] dd_wdata;

    logic flashram_pending;
    logic flashram_done;
    logic [9:0] flashram_page;
    logic flashram_sector_or_all;
    logic flashram_write_or_erase;
    logic flashram_read_mode;
    logic flashram_write;
    logic [5:0] flashram_address;
    logic [15:0] flashram_wdata;

    logic sram_done;

    logic eeprom_write;
    logic [10:0] eeprom_address;
    logic [7:0] eeprom_rdata;
    logic [7:0] eeprom_wdata;

    logic rtc_pending;
    logic rtc_done;
    logic rtc_wdata_valid;
    logic [42:0] rtc_rdata;
    logic [42:0] rtc_wdata;

    logic cfg_unlock;
    logic cfg_pending;
    logic cfg_done;
    logic cfg_error;
    logic [7:0] cfg_cmd;
    logic [31:0] cfg_rdata [0:1];
    logic [31:0] cfg_wdata [0:1];
    logic [31:0] cfg_identifier;

    logic btn_irq;
    logic usb_irq;
    logic aux_irq;

    logic aux_pending;
    logic [31:0] aux_rdata;
    logic [31:0] aux_wdata;

    logic [15:0] save_count;

    logic cic_invalid_region;
    logic cic_disabled;
    logic cic_64dd_mode;
    logic cic_region;
    logic [7:0] cic_seed;
    logic [47:0] cic_checksum;
    logic [3:0] cic_debug_step;

    logic pi_sdram_active;
    logic pi_flash_active;
    logic [31:0] pi_debug_address;
    logic [16:0] pi_debug_rw_count;
    logic pi_debug_direction;
    logic [3:0] pi_debug_fifo_flags;

    modport controller (
        input n64_reset,
        input n64_nmi,

        output bootloader_enabled,
        output rom_write_enabled,
        output rom_shadow_enabled,
        output rom_extended_enabled,
        output sram_enabled,
        output sram_banked,
        output flashram_enabled,
        output dd_enabled,
        output ddipl_enabled,
        output eeprom_enabled,
        output eeprom_16k_mode,

        input flashram_pending,
        output flashram_done,
        input flashram_page,
        input flashram_sector_or_all,
        input flashram_write_or_erase,

        input rtc_pending,
        output rtc_done,
        output rtc_wdata_valid,
        input rtc_rdata,
        output rtc_wdata,

        input cfg_pending,
        output cfg_done,
        output cfg_error,
        input cfg_cmd,
        input cfg_rdata,
        output cfg_wdata,
        output cfg_identifier,

        output btn_irq,
        output usb_irq,
        output aux_irq,

        input aux_pending,
        input aux_rdata,
        output aux_wdata,

        input save_count,

        input cic_invalid_region,
        output cic_disabled,
        output cic_64dd_mode,
        output cic_region,
        output cic_seed,
        output cic_checksum,
        input cic_debug_step,

        input pi_debug_address,
        input pi_debug_rw_count,
        input pi_debug_direction,
        input pi_debug_fifo_flags
    );

    modport pi (
        output n64_reset,
        output n64_nmi,

        input bootloader_enabled,
        input rom_write_enabled,
        input rom_shadow_enabled,
        input rom_extended_enabled,
        input sram_enabled,
        input sram_banked,
        input flashram_enabled,
        input dd_enabled,
        input ddipl_enabled,

        output sram_done,

        input flashram_read_mode,

        input cfg_unlock,

        output pi_sdram_active,
        output pi_flash_active,

        output pi_debug_address,
        output pi_debug_rw_count,
        output pi_debug_direction,
        output pi_debug_fifo_flags
    );

    modport flashram (
        output flashram_pending,
        input flashram_done,
        output flashram_page,
        output flashram_sector_or_all,
        output flashram_write_or_erase,

        output flashram_read_mode,

        output flashram_write,
        output flashram_address,
        output flashram_wdata
    );

    modport si (
        input eeprom_enabled,
        input eeprom_16k_mode,

        output eeprom_write,
        output eeprom_address,
        input eeprom_rdata,
        output eeprom_wdata,

        output rtc_pending,
        input rtc_done,
        input rtc_wdata_valid,
        output rtc_rdata,
        input rtc_wdata
    );

    modport dd (
        input n64_reset,
        input n64_nmi,

        output dd_write,
        output dd_address,
        input dd_rdata,
        output dd_wdata
    );

    modport bram (
        input flashram_write,
        input flashram_address,
        input flashram_wdata,

        input eeprom_write,
        input eeprom_address,
        output eeprom_rdata,
        input eeprom_wdata,

        input dd_write,
        input dd_address,
        output dd_rdata,
        input dd_wdata
    );

    modport cfg (
        input n64_reset,
        input n64_nmi,

        output cfg_unlock,
        output cfg_pending,
        input cfg_done,
        input cfg_error,
        output cfg_cmd,
        output cfg_rdata,
        input cfg_wdata,
        input cfg_identifier,

        input btn_irq,
        input usb_irq,
        input aux_irq,

        output aux_pending,
        output aux_rdata,
        input aux_wdata
    );

    modport save_counter (
        input eeprom_write,
        input sram_done,
        input flashram_done,

        output save_count
    );

    modport cic (
        output cic_invalid_region,
        input cic_disabled,
        input cic_64dd_mode,
        input cic_region,
        input cic_seed,
        input cic_checksum,
        output cic_debug_step
    );

    modport arbiter (
        input pi_sdram_active,
        input pi_flash_active
    );

endinterface
