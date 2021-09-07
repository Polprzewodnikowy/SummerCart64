#include "driver.h"
#include "sys.h"


// DMA

bool dma_start (uint32_t address, uint32_t length, e_dma_id_t id, e_dma_dir_t dir) {
    if (DMA->SCR & DMA_SCR_BUSY) {
        return false;
    }

    DMA->MADDR = address;
    DMA->ID_LEN = ((id & 0x03) << 30) | (length & 0x3FFFFFFF);
    DMA->SCR = ((dir == DMA_DIR_TO_SDRAM) ? DMA_SCR_DIR : 0) | DMA_SCR_START;

    return true;
}

void dma_abort (void) {
    DMA->SCR = DMA_SCR_STOP;
}

bool dma_busy (void) {
    return DMA->SCR & DMA_SCR_BUSY;
}


// FLASHRAM

e_flashram_op_t flashram_get_pending_operation (void) {
    uint32_t scr = FLASHRAM->SCR;
    
    if (!(scr & FLASHRAM_OPERATION_PENDING)) {
        return FLASHRAM_OP_NONE;
    }

    if (scr & FLASHRAM_WRITE_OR_ERASE) {
        if (scr & FLASHRAM_SECTOR_OR_ALL) {
            return FLASHRAM_OP_ERASE_ALL;
        } else {
            return FLASHRAM_OP_ERASE_SECTOR;
        }
    } else {
        return FLASHRAM_OP_WRITE_PAGE;
    }
}

uint32_t flashram_get_operation_length (e_flashram_op_t op) {
    switch (op) {
        case FLASHRAM_OP_ERASE_ALL: return FLASHRAM_SIZE;
        case FLASHRAM_OP_ERASE_SECTOR: return FLASHRAM_SECTOR_SIZE;
        case FLASHRAM_OP_WRITE_PAGE: return FLASHRAM_PAGE_SIZE;
        default: return 0;
    }
}

void flashram_set_operation_done (void) {
    FLASHRAM->SCR = FLASHRAM_OPERATION_DONE;
}

uint32_t flashram_get_page (void) {
    return (FLASHRAM->SCR >> FLASHRAM_PAGE_BIT);
}

volatile uint32_t * flashram_get_page_buffer (void) {
    return FLASHRAM->BUFFER;
}


// USB

bool usb_rx_byte (uint8_t *data) {
    if (!(USB->SCR & USB_SCR_RXNE)) {
        return false;
    }

    *data = USB->DR;

    return true;
}

bool usb_rx_word (uint32_t *data) {
    static uint8_t current_byte = 0;
    static uint32_t buffer = 0;
    uint8_t tmp;

    while (usb_rx_byte(&tmp)) {
        buffer = (buffer << 8) | tmp;
        current_byte += 1;
        if (current_byte == 4) {
            current_byte = 0;
            *data = buffer;
            buffer = 0;

            return true;
        }
    }

    return false;
}

bool usb_tx_byte (uint8_t data) {
    if (!(USB->SCR & USB_SCR_TXE)) {
        return false;
    }

    USB->DR = data;

    return true;
}

bool usb_tx_word (uint32_t data) {
    static uint8_t current_byte = 0;
    while (usb_tx_byte(data >> ((3 - current_byte) * 8))) {
        current_byte += 1;
        if (current_byte == 4) {
            current_byte = 0;

            return true;
        }
    }

    return false;
}

void usb_flush_rx (void) {
    USB->SCR = USB_SCR_FLUSH_RX;
}

void usb_flush_tx (void) {
    USB->SCR = USB_SCR_FLUSH_TX;
}


// CFG

uint32_t cfg_get_status (void) {
    return CFG->SCR;
}

void cfg_set_cpu_ready (bool enabled) {
    if (enabled) {
        CFG->SCR |= CFG_SCR_CPU_READY;
    } else {
        CFG->SCR &= ~CFG_SCR_CPU_READY;
    }    
}

void cfg_set_sdram_switch (bool enabled) {
    if (enabled) {
        CFG->SCR |= CFG_SCR_SDRAM_SWITCH;
    } else {
        CFG->SCR &= ~CFG_SCR_SDRAM_SWITCH;
    }
}

void cfg_set_sdram_writable (bool enabled) {
    if (enabled) {
        CFG->SCR |= CFG_SCR_SDRAM_WRITABLE;
    } else {
        CFG->SCR &= ~CFG_SCR_SDRAM_WRITABLE;
    }
}

void cfg_set_dd_enable (bool enabled) {
    if (enabled) {
        CFG->SCR |= CFG_SCR_DD_EN;
    } else {
        CFG->SCR &= ~CFG_SCR_DD_EN;
    }
}

void cfg_set_usb_waiting (bool enabled) {
    if (enabled) {
        CFG->SCR |= CFG_SCR_USB_WAITING;
    } else {
        CFG->SCR &= ~CFG_SCR_USB_WAITING;
    }
}

void cfg_set_save_type (e_cfg_save_type_t save_type) {
    uint32_t save_offset = 0;

    CFG->SCR &= ~(CFG_SCR_FLASHRAM_EN | CFG_SCR_SRAM_BANKED | CFG_SCR_SRAM_EN);

    switch (save_type) {
        case CFG_SAVE_TYPE_NONE:
            break;
        case CFG_SAVE_TYPE_EEPROM_4K:
            save_offset = SDRAM_SIZE - CFG_SAVE_SIZE_EEPROM_4K;
            break;
        case CFG_SAVE_TYPE_EEPROM_16K:
            save_offset = SDRAM_SIZE - CFG_SAVE_SIZE_EEPROM_16K;
            break;
        case CFG_SAVE_TYPE_SRAM:
            save_offset = SDRAM_SIZE - CFG_SAVE_SIZE_SRAM;
            CFG->SCR |= CFG_SCR_SRAM_EN;
            break;
        case CFG_SAVE_TYPE_FLASHRAM:
            save_offset = SDRAM_SIZE - CFG_SAVE_SIZE_FLASHRAM;
            CFG->SCR |= CFG_SCR_FLASHRAM_EN;
            break;
        case CFG_SAVE_TYPE_SRAM_BANKED:
            save_offset = SDRAM_SIZE - CFG_SAVE_SIZE_SRAM_BANKED;
            CFG->SCR |= CFG_SCR_SRAM_BANKED | CFG_SCR_SRAM_EN;
            break;
        case CFG_SAVE_TYPE_FLASHRAM_PKST2:
            save_offset = CFG_SAVE_OFFSET_PKST2;
            CFG->SCR |= CFG_SCR_FLASHRAM_EN;
            break;
        default:
            break;
    }

    CFG->SAVE_OFFSET = save_offset;
}

void cfg_set_save_offset (uint32_t offset) {
    CFG->SAVE_OFFSET = offset;
}

uint32_t cfg_get_save_offset (void) {
    return CFG->SAVE_OFFSET;
}

void cfg_set_dd_offset (uint32_t offset) {
    CFG->DD_OFFSET = offset;
}

uint32_t cfg_get_dd_offset (void) {
    return CFG->DD_OFFSET;
}

bool cfg_get_command (uint8_t *cmd, uint32_t *args) {
    if (!(CFG->SCR & CFG_SCR_CPU_BUSY)) {
        return false;
    }

    *cmd = CFG->CMD;
    for (size_t i = 0; i < 2; i++) {
        args[i] = CFG->DATA[i];
    }

    return true;
}

void cfg_set_response (uint32_t *args, bool error) {
    for (size_t i = 0; i < 2; i++) {
        CFG->DATA[i] = args[i];
    }
    if (error) {
        CFG->SCR |= CFG_SCR_CMD_ERROR;
    } else {
        CFG->SCR &= ~CFG_SCR_CMD_ERROR;
    }
    CFG->SCR &= ~(CFG_SCR_CPU_BUSY);
}


// SI

bool si_rx_ready (void) {
    return SI->SCR & SI_SCR_RX_READY;
}

bool si_rx_stop_bit (void) {
    return SI->SCR & SI_SCR_RX_STOP_BIT;
}

bool si_tx_busy (void) {
    return SI->SCR & SI_SCR_TX_BUSY;
}

void si_rx_reset (void) {
    SI->SCR = SI_SCR_RX_RESET;
}

void si_reset (void) {
    SI->SCR = SI_SCR_TX_RESET | SI_SCR_RX_RESET;
}

void si_rx (uint8_t *data) {
    uint32_t rx_length = (SI->SCR & SI_SCR_RX_LENGTH_MASK) >> SI_SCR_RX_LENGTH_BIT;
    for (size_t i = 0; i < rx_length; i++) {
        data[i] = ((uint8_t *) SI->DATA)[(10 - rx_length) + i];
    }
}

void si_tx (uint8_t *data, size_t length) {
    for (size_t i = 0; i < ((length + 3) / 4); i++) {
        SI->DATA[i] = ((uint32_t *) data)[i];
    }
    SI->DATA[length / 4] |= (0x80 << ((length % 4) * 8));
    SI->SCR = (((length * 8) + 1) << SI_SCR_TX_LENGTH_BIT) | SI_SCR_TX_START;
}


// I2C

bool i2c_busy (void) {
    return I2C->SCR & I2C_SCR_BUSY;
}

bool i2c_ack (void) {
    return I2C->SCR & I2C_SCR_ACK;
}

void i2c_start (void) {
    I2C->SCR = I2C_SCR_START;
}

void i2c_stop (void) {
    I2C->SCR = I2C_SCR_STOP;
}

void i2c_begin_trx (uint8_t data, bool mack) {
    I2C->SCR = mack ? I2C_SCR_MACK : 0;
    I2C->DR = data;
}

uint8_t i2c_get_data (void) {
    return I2C->DR;
}


// RTC

static const uint8_t rtc_bit_mask[7] = {
    0b01111111,
    0b01111111,
    0b00111111,
    0b00000111,
    0b00111111,
    0b00011111,
    0b11111111
};

void rtc_sanitize_time_data (uint8_t *time_data) {
    for (int i = 0; i < 7; i++) {
        time_data[i] &= rtc_bit_mask[i];
    }
}

void rtc_convert_to_n64 (uint8_t *rtc_data, uint8_t *n64_data) {
    rtc_sanitize_time_data(rtc_data);
    n64_data[0] = rtc_data[RTC_RTCSEC];
    n64_data[1] = rtc_data[RTC_RTCMIN];
    n64_data[2] = rtc_data[RTC_RTCHOUR] | 0x80;
    n64_data[4] = rtc_data[RTC_RTCWKDAY] - 1;
    n64_data[3] = rtc_data[RTC_RTCDATE];
    n64_data[5] = rtc_data[RTC_RTCMTH];
    n64_data[6] = rtc_data[RTC_RTCYEAR];
}

void rtc_convert_from_n64 (uint8_t *n64_data, uint8_t *rtc_data) {
    rtc_data[RTC_RTCSEC] = n64_data[0];
    rtc_data[RTC_RTCMIN] = n64_data[1];
    rtc_data[RTC_RTCHOUR] = n64_data[2];
    rtc_data[RTC_RTCWKDAY] = n64_data[4] + 1;
    rtc_data[RTC_RTCDATE] = n64_data[3];
    rtc_data[RTC_RTCMTH] = n64_data[5];
    rtc_data[RTC_RTCYEAR] = n64_data[6];
    rtc_sanitize_time_data(rtc_data);
}


// Misc

static const char hex_char_map[16] = {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};

void print (const char *text) {
    while (*text != '\0') {
        while (!(UART->SCR & UART_SCR_TXE));
        UART->DR = *text++;
    }
}

void print_02hex (uint8_t number) {
    char buffer[3];
    buffer[0] = hex_char_map[(number >> 4) & 0x0F];
    buffer[1] = hex_char_map[number & 0x0F];
    buffer[2] = '\0';
    print(buffer);
}

void print_08hex (uint32_t number) {
    print_02hex((number >> 24) & 0xFF);
    print_02hex((number >> 16) & 0xFF);
    print_02hex((number >> 8) & 0xFF);
    print_02hex((number >> 0) & 0xFF);
}

uint32_t swapb (uint32_t data) {
    return (
        (data << 24) |
        ((data << 8) & 0x00FF0000) |
        ((data >> 8) & 0x0000FF00) |
        (data >> 24)
    );
}
