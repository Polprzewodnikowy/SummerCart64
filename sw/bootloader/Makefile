N64_BINDIR = $(N64_INST)/bin

TOOLCHAIN = $(N64_BINDIR)/mips64-elf-
CC = $(TOOLCHAIN)gcc
CXX = $(TOOLCHAIN)g++
OBJCOPY = $(TOOLCHAIN)objcopy
OBJDUMP = $(TOOLCHAIN)objdump
STRIP = $(TOOLCHAIN)strip
SIZE = $(TOOLCHAIN)size
N64_ELFCOMPRESS = $(N64_BINDIR)/n64elfcompress
N64_TOOL = $(N64_BINDIR)/n64tool
PYTHON = python3

FLAGS = -march=vr4300 -mtune=vr4300 -mfix4300 $(USER_FLAGS)
CFLAGS = -Os -Wall -ffunction-sections -fdata-sections -ffreestanding -MMD -MP
ASFLAGS = -Wa,-I$(N64_INST)/mips64-elf/lib
LDFLAGS = -lc -nostartfiles -Wl,--gc-sections
VERSION ?=

SRC_DIR = src
ASSET_DIR = assets
BUILD_DIR = build

SRC_FILES = \
	startup.S \
	boot.c \
	cic.c \
	display.c \
	error.c \
	exception.c \
	exception.S \
	font.c \
	init.c \
	interrupts.c \
	interrupts.S \
	io.c \
	joybus.c \
	main.c \
	menu.c \
	reboot.S \
	sc64.c \
	syscalls.c \
	test.c \
	version.c \
	fatfs/diskio.c \
	fatfs/ff.c \
	fatfs/ffsystem.c \
	fatfs/ffunicode.c

ASSET_FILES = \
	sc64_logo_640_240_dimmed.png

SRCS = $(SRC_FILES) $(ASSET_FILES)
SRC_OBJS = $(patsubst %,%.o,$(SRC_FILES))
ASSET_OBJS = $(patsubst %,%.asset.o,$(basename $(ASSET_FILES)))
OBJS = $(addprefix $(BUILD_DIR)/,$(notdir $(SRC_OBJS) $(ASSET_OBJS)))
DEPS = $(OBJS:.o=.d)
VPATH = $(SRC_DIR) $(SRC_DIR)/fatfs $(ASSET_DIR)

$(@info $(shell mkdir -p ./$(BUILD_DIR) &> /dev/null))

$(BUILD_DIR)/%.S.o: %.S
	$(CC) -x assembler-with-cpp $(FLAGS) $(ASFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.c.o: %.c
	$(CC) $(FLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/version.c.o: version.c .FORCE
	$(CC) $(FLAGS) $(CFLAGS) $(VERSION) -c $< -o $@

$(BUILD_DIR)/%.asset: $(ASSET_DIR)/%.png tools/asset_converter.py
	$(PYTHON) tools/asset_converter.py $< $@ --compress

$(BUILD_DIR)/%.asset.o: $(BUILD_DIR)/%.asset $(ASSET_DIR)/assets.S
	@sed -e "s,@sym@,$*,g" -e "s,@file@,$<," < $(ASSET_DIR)/assets.S | $(CC) -x assembler-with-cpp $(FLAGS) $(ASFLAGS) $(CFLAGS) -c - -o $@

$(BUILD_DIR)/bootloader.elf: $(OBJS) N64.ld
	$(CXX) $(FLAGS) $(LDFLAGS) -TN64.ld $(OBJS) -o $@
	@echo 'Size of modules:'
	@$(SIZE) -B -d -t --common $(OBJS)
	@echo 'Size of bootloader:'
	@$(SIZE) -B -d $@
	@$(OBJDUMP) -S $@ > $(BUILD_DIR)/bootloader.lst

$(BUILD_DIR)/bootloader.bin: $(BUILD_DIR)/bootloader.elf
	@cp $< $<.stripped
	@$(STRIP) -s $<.stripped
	@cp $<.stripped $<.compressed
	@$(N64_ELFCOMPRESS) -c 2 -o $(dir $<) $<.compressed
	@$(N64_TOOL) --title "SC64 bootloader" --output $@ --align 256 $<.compressed

all: $(BUILD_DIR)/bootloader.bin

clean:
	@rm -rf ./$(BUILD_DIR)/*

.FORCE:

.PHONY: .FORCE all clean print_size

-include $(DEPS)
