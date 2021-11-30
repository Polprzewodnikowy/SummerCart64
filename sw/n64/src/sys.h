#ifndef SYS_H__
#define SYS_H__


#include <inttypes.h>
#include <stddef.h>


typedef volatile uint8_t io8_t;
typedef volatile uint32_t io32_t;


#define	ALIGN(value, align)         (((value) + ((typeof(value))(align) - 1)) & ~((typeof(value))(align) - 1))

#define CACHED(address)             ((io32_t *) ((((io32_t) (address)) & 0x1FFFFFFFUL) | 0x80000000UL))
#define UNCACHED(address)           ((io32_t *) ((((io32_t) (address)) & 0x1FFFFFFFUL) | 0xA0000000UL))

#define CPU_FREQUENCY               (93750000UL)


typedef struct {
    uint32_t tv_type;
    uint32_t rom_type;
    uint32_t rom_base;
    uint32_t reset_type;
    uint32_t cic_id;
    uint32_t version;
    uint32_t mem_size;
    uint8_t app_nmi_buffer[64];
    uint32_t __reserved_1[37];
    uint32_t mem_size_6105;
} boot_info_t;

#define BOOT_INFO_BASE              (0x00000300UL)
#define BOOT_INFO                   ((boot_info_t *) BOOT_INFO_BASE)


typedef struct {
    io32_t MADDR;
    io32_t PADDR;
    io32_t RDMA;
    io32_t WDMA;
    io32_t SR;
    struct {
        io32_t LAT;
        io32_t PWD;
        io32_t PGS;
        io32_t RLS;
    } DOM[2];
} pi_regs_t;

#define PI_BASE                     (0x04600000UL)
#define PI                          ((pi_regs_t *) PI_BASE)

#define PI_SR_DMA_BUSY              (1 << 0)
#define PI_SR_IO_BUSY               (1 << 1)
#define PI_SR_DMA_ERROR             (1 << 2)
#define PI_SR_RESET_CONTROLLER      (1 << 0)
#define PI_SR_CLEAR_INTERRUPT       (1 << 1)


typedef struct {
    io32_t MADDR;
    io32_t RDMA;
    io32_t __reserved_1;
    io32_t __reserved_2;
    io32_t WDMA;
    io32_t __reserved_3;
    io32_t SR;
} si_regs_t;

#define SI_BASE                     (0x04800000UL)
#define SI                          ((si_regs_t *) SI_BASE)

#define SI_SR_DMA_BUSY              (1 << 0)
#define SI_SR_IO_BUSY               (1 << 1)
#define SI_SR_DMA_ERROR             (1 << 3)
#define SI_SR_INTERRUPT             (1 << 12)
#define SI_SR_CLEAR_INTERRUPT       (0)


typedef struct {
    uint32_t pi_config;
    uint32_t clock_rate;
    uint32_t load_addr;
    struct {
        uint16_t __reserved_1;
        uint8_t major;
        char minor;
    } sdk_version;
    uint32_t crc[2];
    uint32_t __reserved_1[2];
    char name[20];
    uint8_t ___reserved_2[7];
    char id[4];
    uint8_t version;
    uint32_t ipl3[1008];
} rom_header_t;

#define ROM_HEADER_DDIPL_BASE       (0x06000000UL)
#define ROM_HEADER_DDIPL            ((rom_header_t *) ROM_HEADER_DDIPL_BASE)
#define ROM_HEADER_CART_BASE        (0x10000000UL)
#define ROM_HEADER_CART             ((rom_header_t *) ROM_HEADER_CART_BASE)


#define PIFRAM_BASE                 (0x1FC007C0UL)
#define PIFRAM                      ((io8_t *) PIFRAM_BASE)



void wait_ms(uint32_t ms);
uint32_t io_read(io32_t *address);
void io_write(io32_t *address, uint32_t value);
uint32_t pi_busy(void);
uint32_t pi_io_read(io32_t *address);
void pi_io_write(io32_t *address, uint32_t value);
uint32_t si_busy(void);
uint32_t si_io_read(io32_t *address);
void si_io_write(io32_t *address, uint32_t value);


#endif
