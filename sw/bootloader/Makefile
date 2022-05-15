TOOLCHAIN = mips64-elf-
CC = $(TOOLCHAIN)gcc
CXX = $(TOOLCHAIN)g++
OBJCOPY = $(TOOLCHAIN)objcopy
OBJDUMP = $(TOOLCHAIN)objdump
SIZE = $(TOOLCHAIN)size
PYTHON = python3

FLAGS = -march=vr4300 -mtune=vr4300 $(USER_FLAGS)
CFLAGS = -Os -Wall -ffunction-sections -fdata-sections -ffreestanding -G 0 -MMD -MP
ASFLAGS = -Wa,-I$(N64_INST)/mips64-elf/lib
LDFLAGS = -lc -nostartfiles -Wl,--gc-sections

SRC_DIR = src
ASSET_DIR = assets
BUILD_DIR = build

SRC_FILES = \
	startup.S \
	boot.c \
	crc32.c \
	error.c \
	exception.c \
	exception.S \
	font.c \
	init.c \
	interrupt.c \
	io.c \
	ipl2.S \
	main.c \
	sc64.c \
	storage.c \
	syscalls.c \
	version.c \
	fatfs/diskio.c \
	fatfs/ff.c \
	fatfs/ffsystem.c \
	fatfs/ffunicode.c

ASSET_FILES = \
	background.png

SRCS = $(SRC_FILES) $(ASSET_FILES)
OBJS = $(addprefix $(BUILD_DIR)/, $(notdir $(patsubst %,%.o,$(SRCS))))
DEPS = $(OBJS:.o=.d)
$(info $(SRCS))
$(info $(OBJS))
VPATH = $(SRC_DIR) $(SRC_DIR)/fatfs $(ASSET_DIR)

$(@info $(shell mkdir -p ./$(BUILD_DIR) &> /dev/null))

$(BUILD_DIR)/%.S.o: %.S
	$(CC) -x assembler-with-cpp $(FLAGS) $(ASFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.c.o: %.c
	$(CC) $(FLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.png.o: %.png
	$(PYTHON) tools/asset_converter.py $< $@
	$(OBJCOPY) -I binary -O elf32-bigmips -B mips:4000 $@ $@

$(BUILD_DIR)/bootloader.elf: $(OBJS) N64.ld
	$(CXX) $(FLAGS) $(LDFLAGS) -TN64.ld $(OBJS) -o $@
	@$(OBJDUMP) -S $@ > $(BUILD_DIR)/bootloader.lst

$(BUILD_DIR)/bootloader.bin: $(BUILD_DIR)/bootloader.elf
	@$(OBJCOPY) -O binary $< $@
	@chksum64 $@ > /dev/null
	@truncate --size=1028k $@

$(BUILD_DIR)/bootloader.hex: $(BUILD_DIR)/bootloader.bin
	@$(OBJCOPY) -I binary -O ihex $< $@

print_size: $(BUILD_DIR)/bootloader.elf
	@echo 'Size of modules:'
	@$(SIZE) -B -d -t --common $(OBJS)
	@echo 'Size of bootloader:'
	@$(SIZE) -B -d $<

all: $(BUILD_DIR)/bootloader.hex print_size

clean:
	@rm -rf ./$(BUILD_DIR)/*

$(BUILD_DIR)/version.c.o: .FORCE

.FORCE:

.PHONY: .FORCE all clean print_size

-include $(DEPS)
