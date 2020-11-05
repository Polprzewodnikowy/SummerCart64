#ifndef SC64_REGS_H__
#define SC64_REGS_H__

// SummerCart64 config register

#define SC64_CONFIG_REG             (0x1E000000)

#define SC64_CONFIG_SDRAM_ENABLE    (1 << 1)

// Boot CIC and TV type override register

#define SC64_BOOT_REG               (0x1E000004)

#define SC64_BOOT_CIC_TYPE_MSK      (0x0F)
#define SC64_BOOT_CIC_TYPE_BIT      (0)
#define SC64_BOOT_CIC_TYPE(x)       (((x) & SC64_BOOT_CIC_TYPE_MSK) >> SC64_BOOT_CIC_TYPE_BIT)
#define SC64_BOOT_TV_TYPE_MSK       (0x30)
#define SC64_BOOT_TV_TYPE_BIT       (4)
#define SC64_BOOT_TV_TYPE(x)        (((x) & SC64_BOOT_TV_TYPE_MSK) >> SC64_BOOT_TV_TYPE_BIT)

#endif
