TOOLCHAIN = arm-none-eabi-
CC = $(TOOLCHAIN)gcc
CXX = $(TOOLCHAIN)g++
OBJCOPY = $(TOOLCHAIN)objcopy
OBJDUMP = $(TOOLCHAIN)objdump
SIZE = $(TOOLCHAIN)size

FLAGS = -mcpu=cortex-m0plus -mthumb -DSTM32G030xx $(USER_FLAGS) -g -ggdb3
CFLAGS = -Og -Wall -ffunction-sections -fdata-sections -ffreestanding -MMD -MP -I./lib
LDFLAGS = -nostartfiles -Wl,--gc-sections

SRC_DIR = src
BUILD_DIR = build

SRC_FILES = \
	startup.S \
	cfg.c \
	cic.c \
	flash.c \
	flashram.c \
	fpga.c \
	gvr.c \
	hw.c \
	main.c \
	rtc.c \
	task.c \
	usb.c

SRCS = $(addprefix $(SRC_DIR)/, $(SRC_FILES))
OBJS = $(addprefix $(BUILD_DIR)/, $(notdir $(patsubst %,%.o,$(SRCS))))
DEPS = $(OBJS:.o=.d)

VPATH = $(SRC_DIR)

$(@info $(shell mkdir -p ./$(BUILD_DIR) &> /dev/null))

$(BUILD_DIR)/%.S.o: %.S
	$(CC) -x assembler-with-cpp $(FLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.c.o: %.c
	$(CC) $(FLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/controller.elf: $(OBJS) STM32G030F6Px_FLASH.ld
	$(CXX) $(FLAGS) $(LDFLAGS) -TSTM32G030F6Px_FLASH.ld $(OBJS) -o $@
	@$(OBJDUMP) -S -D $@ > $(BUILD_DIR)/controller.lst

$(BUILD_DIR)/controller.bin: $(BUILD_DIR)/controller.elf
	@$(OBJCOPY) -O binary $< $@

$(BUILD_DIR)/controller.hex: $(BUILD_DIR)/controller.bin
	@$(OBJCOPY) -I binary -O ihex $< $@

print_size: $(BUILD_DIR)/controller.elf
	@echo 'Size of modules:'
	@$(SIZE) -B -d -t --common $(OBJS)
	@echo 'Size of controller:'
	@$(SIZE) -B -d $<

all: $(BUILD_DIR)/controller.hex print_size

clean:
	@rm -rf ./$(BUILD_DIR)/*

.PHONY: all clean print_size

-include $(DEPS)
