TOOLCHAIN = riscv32-unknown-elf-
CC = $(TOOLCHAIN)gcc
OBJCOPY = $(TOOLCHAIN)objcopy
OBJDUMP = $(TOOLCHAIN)objdump
SIZE = $(TOOLCHAIN)size

FLAGS = -mabi=ilp32 -march=rv32i $(USER_FLAGS)
CFLAGS = -Os -Wall -ffunction-sections -fdata-sections -ffreestanding -MMD -MP
LDFLAGS = -nostartfiles -Wl,--gc-sections

SRC_DIR = src
BUILD_DIR = build

SRC_FILES = startup.S process.c usb.c cfg.c dma.c joybus.c rtc.c i2c.c flashram.c uart.c flash.c

SRCS = $(addprefix $(SRC_DIR)/, $(SRC_FILES))
OBJS = $(addprefix $(BUILD_DIR)/, $(notdir $(patsubst %,%.o,$(SRCS))))
DEPS = $(OBJS:.o=.d)

VPATH = $(SRC_DIR)

$(@info $(shell mkdir -p ./$(BUILD_DIR) &> /dev/null))

all: $(BUILD_DIR)/governor.hex

$(BUILD_DIR)/%.c.o: %.c
	$(CC) $(FLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.S.o: %.S
	$(CC) -x assembler $(FLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/governor.hex: $(OBJS) SC64.ld
	$(CC) $(FLAGS) $(LDFLAGS) -TSC64.ld $(OBJS) -o $(BUILD_DIR)/governor.elf
	$(OBJDUMP) -D $(BUILD_DIR)/governor.elf > $(BUILD_DIR)/governor.map
	$(OBJCOPY) -O binary $(BUILD_DIR)/governor.elf $(BUILD_DIR)/governor.bin
	$(OBJCOPY) -I binary -O ihex $(BUILD_DIR)/governor.bin $@
	@echo 'Size of modules:'
	@$(SIZE) -B -t --common $(OBJS)
	@echo 'Size of governor:'
	@$(SIZE) -B $(BUILD_DIR)/governor.elf

clean:
	rm -rf ./$(BUILD_DIR)/*

.PHONY: all clean

-include $(DEPS)
