interface n64_scb ();

    logic n64_reset;
    logic n64_nmi;

    logic bootloader_enabled;
    logic rom_write_enabled;
    logic rom_shadow_enabled;
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
    logic [9:0] flashram_sector;
    logic flashram_sector_or_all;
    logic flashram_write_or_erase;
    logic flashram_read_mode;
    logic flashram_write;
    logic [5:0] flashram_address;
    logic [15:0] flashram_wdata;

    logic eeprom_write;
    logic [10:0] eeprom_address;
    logic [7:0] eeprom_rdata;
    logic [7:0] eeprom_wdata;

    logic rtc_pending;
    logic rtc_done;
    logic rtc_wdata_valid;
    logic [41:0] rtc_rdata;
    logic [41:0] rtc_wdata;

    logic cfg_unlock;
    logic cfg_pending;
    logic cfg_done;
    logic cfg_error;
    logic cfg_irq;
    logic [7:0] cfg_cmd;
    logic [31:0] cfg_rdata [0:1];
    logic [31:0] cfg_wdata [0:1];
    logic [31:0] cfg_version;

    modport controller (
        input n64_reset,
        input n64_nmi,

        output bootloader_enabled,
        output rom_write_enabled,
        output rom_shadow_enabled,
        output sram_enabled,
        output sram_banked,
        output flashram_enabled,
        output dd_enabled,
        output ddipl_enabled,
        output eeprom_enabled,
        output eeprom_16k_mode,

        input flashram_pending,
        output flashram_done,
        input flashram_sector,
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
        output cfg_irq,
        input cfg_cmd,
        input cfg_rdata,
        output cfg_wdata,
        output cfg_version
    );

    modport pi (
        output n64_reset,
        output n64_nmi,

        input bootloader_enabled,
        input rom_write_enabled,
        input rom_shadow_enabled,
        input sram_enabled,
        input sram_banked,
        input flashram_enabled,
        input dd_enabled,
        input ddipl_enabled,

        input flashram_read_mode,

        input cfg_unlock
    );

    modport flashram (
        output flashram_pending,
        input flashram_done,
        output flashram_sector,
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
        input cfg_irq,
        output cfg_cmd,
        output cfg_rdata,
        input cfg_wdata,
        input cfg_version
    );

endinterface
