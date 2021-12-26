TOOLCHAIN = riscv32-unknown-elf-
CC = $(TOOLCHAIN)gcc
CXX = $(TOOLCHAIN)g++
OBJCOPY = $(TOOLCHAIN)objcopy
OBJDUMP = $(TOOLCHAIN)objdump
SIZE = $(TOOLCHAIN)size

FLAGS = -mabi=ilp32 -march=rv32i $(USER_FLAGS)
CFLAGS = -Os -Wall -ffunction-sections -fdata-sections -ffreestanding -MMD -MP
LDFLAGS = -nostartfiles -Wl,--gc-sections

SRC_DIR = src
BUILD_DIR = build

SRC_FILES = \
	startup.S \
	process.c \
	usb.c \
	cfg.c \
	dma.c \
	joybus.c \
	rtc.c \
	i2c.c \
	flashram.c \
	uart.c \
	flash.c \
	dd.c \
	isv.c

SRCS = $(addprefix $(SRC_DIR)/, $(SRC_FILES))
OBJS = $(addprefix $(BUILD_DIR)/, $(notdir $(patsubst %,%.o,$(SRCS))))
DEPS = $(OBJS:.o=.d)

VPATH = $(SRC_DIR)

$(@info $(shell mkdir -p ./$(BUILD_DIR) &> /dev/null))

$(BUILD_DIR)/%.S.o: %.S
	$(CC) -x assembler-with-cpp $(FLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.c.o: %.c
	$(CC) $(FLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/governor.elf: $(OBJS) SC64.ld
	$(CXX) $(FLAGS) $(LDFLAGS) -TSC64.ld $(OBJS) -o $@
	@$(OBJDUMP) -D $@ > $(BUILD_DIR)/governor.lst

$(BUILD_DIR)/governor.bin: $(BUILD_DIR)/governor.elf
	@$(OBJCOPY) -O binary $< $@

$(BUILD_DIR)/governor.hex: $(BUILD_DIR)/governor.bin
	@$(OBJCOPY) -I binary -O ihex $< $@

print_size: $(BUILD_DIR)/governor.elf
	@echo 'Size of modules:'
	@$(SIZE) -B -d -t --common $(OBJS)
	@echo 'Size of governor:'
	@$(SIZE) -B -d $<

all: $(BUILD_DIR)/governor.hex print_size

clean:
	@rm -rf ./$(BUILD_DIR)/*

.PHONY: all clean print_size

-include $(DEPS)
