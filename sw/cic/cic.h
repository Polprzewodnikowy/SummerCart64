#ifndef CIC_H__
#define CIC_H__


#include <stdint.h>


typedef struct {
    volatile struct {
        uint8_t REGION: 1;
        uint8_t TYPE: 1;
        uint8_t DISABLED: 1;
        uint8_t __reserved: 5;
        uint8_t SEED;
        uint8_t CHECKSUM[6];
    } CONFIG;
    volatile uint32_t IO;
    volatile uint32_t DEBUG;
} ext_regs_t;

#define EXT ((ext_regs_t *) (0xC0000000UL))

#define CIC_DQ                      (1 << 0) // IO
#define CIC_CLK                     (1 << 1) // I
#define CIC_RESET                   (1 << 2) // I
#define CIC_TIMER_ELAPSED           (1 << 3) // I
#define CIC_TIMER_START             (1 << 4) // O
#define CIC_TIMER_CLEAR             (1 << 5) // O
#define CIC_INVALID_REGION          (1 << 6) // O


#define CIC_INIT()                  { EXT->IO = (CIC_TIMER_CLEAR | CIC_DQ); }
#define CIC_IS_RUNNING()            (EXT->IO & CIC_RESET)
#define CIC_CLK_WAIT_LOW()          { while ((EXT->IO & (CIC_TIMER_ELAPSED | CIC_RESET | CIC_CLK)) == (CIC_RESET | CIC_CLK)); }
#define CIC_CLK_WAIT_HIGH()         { while ((EXT->IO & (CIC_TIMER_ELAPSED | CIC_RESET | CIC_CLK)) == CIC_RESET); }
#define CIC_CLK_IS_LOW()            ((EXT->IO & (CIC_RESET | CIC_CLK)) == CIC_RESET)
#define CIC_DQ_GET()                (EXT->IO & CIC_DQ)
#define CIC_DQ_SET(v)               { EXT->IO = ((v) ? CIC_DQ : 0); }
#define CIC_NOTIFY_INVALID_REGION() { EXT->IO = (CIC_INVALID_REGION | CIC_DQ); }
#define CIC_TIMEOUT_START()         { EXT->IO = (CIC_TIMER_START | CIC_DQ); }
#define CIC_TIMEOUT_STOP()          { EXT->IO = (CIC_TIMER_CLEAR | CIC_DQ); }
#define CIC_TIMEOUT_ELAPSED()       (EXT->IO & CIC_TIMER_ELAPSED)
#define CIC_SET_STEP(v)             { EXT->DEBUG = (v); }


typedef enum {
    CIC_COMMAND_COMPARE = 0,
    CIC_COMMAND_X105 = 2,
    CIC_COMMAND_SOFT_RESET = 3,
} cic_command_t;

typedef enum {
    CIC_STEP_UNAVAILABLE = 0,
    CIC_STEP_POWER_OFF = 1,
    CIC_STEP_LOAD_CONFIG = 2,
    CIC_STEP_ID = 3,
    CIC_STEP_SEED = 4,
    CIC_STEP_CHECKSUM = 5,
    CIC_STEP_INIT_RAM = 6,
    CIC_STEP_COMMAND = 7,
    CIC_STEP_COMPARE = 8,
    CIC_STEP_X105 = 9,
    CIC_STEP_RESET_BUTTON = 10,
    CIC_STEP_DIE_DISABLED = 11,
    CIC_STEP_DIE_64DD = 12,
    CIC_STEP_DIE_INVALID_REGION = 13,
    CIC_STEP_DIE_COMMAND = 14,
} cic_step_t;


#endif
