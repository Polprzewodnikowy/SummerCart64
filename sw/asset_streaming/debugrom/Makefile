# SC64 Asset Streaming - Debug Test ROM
#
# Builds an N64 ROM that provides an interactive cache debugger.
# Requires the same mips64-elf-gcc toolchain as the SC64 bootloader.
#
# Targets:
#   make              - Build the debug ROM
#   make archive      - Generate test asset archive for SD card
#   make clean        - Clean build artifacts
#
# Usage:
#   1. make archive                         # Generate test assets
#   2. Write build/debug_assets.bin to SD at sector 0x10000
#   3. make                                 # Build the ROM
#   4. Load build/debug_rom.z64 via SC64

N64_INST ?= /opt/libdragon
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

# Directories
BOOTLOADER_SRC = ../../bootloader/src
ASSET_SRC = ../src
TOOLS = ../tools
BUILD = build

# Compiler flags
FLAGS = -march=vr4300 -mtune=vr4300 -mfix4300
CFLAGS = -Os -Wall -ffunction-sections -fdata-sections -ffreestanding -MMD -MP
INCLUDES = -I$(BOOTLOADER_SRC) -I$(ASSET_SRC)
LDFLAGS = -lc -nostartfiles -Wl,--gc-sections

# Sources from bootloader (infrastructure)
BOOT_SRCS = \
	startup.S \
	exception.c \
	exception.S \
	display.c \
	font.c \
	init.c \
	interrupts.c \
	interrupts.S \
	io.c \
	joybus.c \
	sc64.c \
	syscalls.c

# Sources from asset streaming library
ASSET_SRCS = \
	asset_manager.c \
	asset_cache.c \
	lz4_decode.c \
	asset_debug.c

# Debug ROM sources
ROM_SRCS = \
	main.c

# Build object lists
BOOT_OBJS = $(addprefix $(BUILD)/boot_,$(notdir $(patsubst %,%.o,$(BOOT_SRCS))))
ASSET_OBJS = $(addprefix $(BUILD)/asset_,$(notdir $(patsubst %,%.o,$(ASSET_SRCS))))
ROM_OBJS = $(addprefix $(BUILD)/rom_,$(notdir $(patsubst %,%.o,$(ROM_SRCS))))
ALL_OBJS = $(BOOT_OBJS) $(ASSET_OBJS) $(ROM_OBJS)
DEPS = $(ALL_OBJS:.o=.d)

$(shell mkdir -p $(BUILD))

# Linker script (reuse bootloader's)
LDSCRIPT = ../../bootloader/N64.ld

# ---- Build rules ----

all: $(BUILD)/debug_rom.z64

# Bootloader infrastructure objects
$(BUILD)/boot_%.S.o: $(BOOTLOADER_SRC)/%.S
	$(CC) -x assembler-with-cpp $(FLAGS) -Wa,-I$(dir $<) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD)/boot_%.c.o: $(BOOTLOADER_SRC)/%.c
	$(CC) $(FLAGS) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Asset streaming library objects (with ASSET_DEBUG enabled)
$(BUILD)/asset_%.c.o: $(ASSET_SRC)/%.c
	$(CC) $(FLAGS) $(CFLAGS) $(INCLUDES) -DASSET_DEBUG -c $< -o $@

# Debug ROM objects
$(BUILD)/rom_%.c.o: %.c
	$(CC) $(FLAGS) $(CFLAGS) $(INCLUDES) -DASSET_DEBUG -c $< -o $@

# Link
$(BUILD)/debug_rom.elf: $(ALL_OBJS) $(LDSCRIPT)
	$(CXX) $(FLAGS) $(LDFLAGS) -T$(LDSCRIPT) $(ALL_OBJS) -o $@
	@echo "Size of debug ROM:"
	@$(SIZE) -B -d $@

# Convert to N64 ROM format
$(BUILD)/debug_rom.z64: $(BUILD)/debug_rom.elf
	@cp $< $<.stripped
	@$(STRIP) -s $<.stripped
	@$(N64_TOOL) --title "SC64 Asset Debug" --output $@ --align 256 $<.stripped
	@echo "ROM: $@ ($(shell stat -c%s $@ 2>/dev/null || stat -f%z $@ 2>/dev/null) bytes)"

# ---- Test asset archive ----

archive: $(BUILD)/debug_assets.bin

$(BUILD)/debug_assets.bin: gen_test_assets.py $(TOOLS)/asset_pack.py
	$(PYTHON) gen_test_assets.py $(BUILD)
	@echo ""
	@echo "Write $(BUILD)/debug_assets.bin to SD card at sector 0x10000:"
	@echo "  dd if=$(BUILD)/debug_assets.bin of=/dev/sdX bs=512 seek=65536"

# ---- Clean ----

clean:
	rm -rf $(BUILD)

.PHONY: all archive clean

-include $(DEPS)
