TOOLCHAIN = riscv64-unknown-elf-
CC = $(TOOLCHAIN)gcc
OBJCOPY = $(TOOLCHAIN)objcopy
SIZE = $(TOOLCHAIN)size

FLAGS = -mabi=ilp32 -march=rv32i
CFLAGS = -Os -Wall -ffunction-sections -fdata-sections -ffreestanding -MMD -MP
LDFLAGS = -nostartfiles -Wl,--gc-sections

SRC_DIR = src
BUILD_DIR = build
BOOTLOADER_DIR = ../../fw/rtl/cpu

SRCS = $(wildcard $(patsubst %, %/*.c, . $(SRC_DIR)))
OBJS = $(addprefix $(BUILD_DIR)/, $(notdir $(SRCS:.c=.o)))
DEPS = $(OBJS:.o=.d)

VPATH = $(SRC_DIR)

all: make_output_dir $(BOOTLOADER_DIR)/cpu_bootloader.sv $(BUILD_DIR)/controller.rom

$(BUILD_DIR)/%.o: %.c
	$(CC) $(FLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/uc.elf: $(OBJS) SC64.ld
	$(CC) $(FLAGS) $(LDFLAGS) -TSC64.ld $(OBJS) -o $@

$(BUILD_DIR)/controller.rom: $(BUILD_DIR)/uc.elf
	$(OBJCOPY) -R .bootloader $(BUILD_DIR)/uc.elf $(BUILD_DIR)/controller.elf
	$(OBJCOPY) -O binary --set-section-flags .bss=alloc,contents $(BUILD_DIR)/controller.elf $(BUILD_DIR)/controller.bin
	python3 tools/bin2rom.py $@ < $(BUILD_DIR)/controller.bin
	@echo 'Size of controller:'
	@$(SIZE) -B build/controller.elf

$(BOOTLOADER_DIR)/cpu_bootloader.sv: $(BUILD_DIR)/uc.elf
	$(OBJCOPY) -j .bootloader $(BUILD_DIR)/uc.elf $(BUILD_DIR)/bootloader.elf
	$(OBJCOPY) -O binary $(BUILD_DIR)/bootloader.elf $(BUILD_DIR)/bootloader.bin
	python3 tools/bin2sv.py tools/cpu_bootloader_template.sv $@ < $(BUILD_DIR)/bootloader.bin
	@echo 'Size of bootloader:'
	@$(SIZE) -B build/bootloader.elf

make_output_dir:
	@$(shell mkdir ./$(BUILD_DIR) 2> /dev/null)

clean:
	rm -rf build

.PHONY: all make_output_dir clean

-include $(DEPS)
