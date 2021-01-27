#ifndef SC64_SD_H__
#define SC64_SD_H__


#include "platform.h"


#define RESPONSE_START_BIT              (1 << 7)

#define R1_PARAMETER_ERROR              (1 << 6)
#define R1_ADDRESS_ERROR                (1 << 5)
#define R1_ERASE_SEQUENCE_ERROR         (1 << 4)
#define R1_COM_CRC_ERROR                (1 << 3)
#define R1_ILLEGAL_COMMAND              (1 << 2)
#define R1_ERASE_RESET                  (1 << 1)
#define R1_IN_IDLE_STATE                (1 << 0)

#define R3_27_36_V_BIT                  (15)
#define R3_27_36_V_MASK                 (0x1FF << R3_27_36_V_BIT)
#define R3_CCS                          (1 << 30)

#define R7_COMMAND_VERSION_BIT          (28)
#define R7_COMMAND_VERSION_MASK         (0xF << R7_COMMAND_VERSION_BIT)
#define R7_VOLTAGE_ACCEPTED_BIT         (8)
#define R7_VOLTAGE_ACCEPTED_MASK        (0xF << R7_VOLTAGE_ACCEPTED_BIT)
#define R7_CHECK_PATTERN_BIT            (0)
#define R7_CHECK_PATTERN_MASK           (0xFF << R7_CHECK_PATTERN_BIT)

#define R7_SUPPLY_VOLTAGE_27_36_V       (1 << 8)
#define R7_CHECK_PATTERN_AA             (0xAA << 0)

#define SD_NOT_BUSY_RESPONSE            (0xFF)

#define CMD_START_BIT                   (1 << 6)
#define ACMD_MARK                       (1 << 7)

#define CMD0                            (CMD_START_BIT | 0)
#define CMD6                            (CMD_START_BIT | 6)
#define CMD8                            (CMD_START_BIT | 8)
#define CMD12                           (CMD_START_BIT | 12)
#define CMD17                           (CMD_START_BIT | 17)
#define CMD18                           (CMD_START_BIT | 18)
#define CMD55                           (CMD_START_BIT | 55)
#define CMD58                           (CMD_START_BIT | 58)
#define ACMD41                          (ACMD_MARK | CMD_START_BIT | 41)

#define CMD8_ARG_SUPPLY_VOLTAGE_27_36_V (1 << 8)
#define CMD8_ARG_CHECK_PATTERN_AA       (0xAA << 0)

#define ACMD41_ARG_HCS                  (1 << 30)

#define DATA_TOKEN_BLOCK_READ           (0xFE)

#define SD_BLOCK_SIZE                   (512)


uint8_t sc64_sd_init(void);
void sc64_sd_deinit(void);
uint8_t sc64_sd_get_status(void);
uint8_t sc64_sd_cmd_send(uint8_t cmd, uint32_t arg, uint8_t *response);
uint8_t sc64_sd_block_read(uint8_t *buffer, size_t length);


#endif
