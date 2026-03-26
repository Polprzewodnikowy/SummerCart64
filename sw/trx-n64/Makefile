# TRX-N64: Tomb Raider 1 — N64 + SC64 Port
#
# Build system for the N64 target ROM.
#
# Targets:
#   make              - Build the ROM
#   make pack         - Package TR1 game assets for SD card
#   make clean        - Clean build artifacts
#
# Requirements:
#   - mips64-elf-gcc toolchain (same as SC64 bootloader)
#   - Python 3 with lz4 module (for asset packing)
#   - Original TR1 game data (.PHD level files) for packing
#
# Usage:
#   1. Place TR1 .PHD files in data/levels/
#   2. make pack              # Convert to SC64 asset archive
#   3. Write build/tr1.bin to SD card at sector 0x10000
#   4. make                   # Build ROM
#   5. Load build/trx-n64.z64 via SC64

N64_INST ?= /opt/libdragon
N64_BINDIR = $(N64_INST)/bin

TOOLCHAIN = $(N64_BINDIR)/mips64-elf-
CC = $(TOOLCHAIN)gcc
CXX = $(TOOLCHAIN)g++
OBJCOPY = $(TOOLCHAIN)objcopy
STRIP = $(TOOLCHAIN)strip
SIZE = $(TOOLCHAIN)size
N64_TOOL = $(N64_BINDIR)/n64tool
PYTHON = python3

# Directories
BOOTLOADER_SRC = ../bootloader/src
ASSET_SRC = ../asset_streaming/src
ASSET_TOOLS = ../asset_streaming/tools
TRX_SRC = ../trx/src/trx
BUILD = build

# Compiler flags
FLAGS = -march=vr4300 -mtune=vr4300 -mfix4300
CFLAGS = -Os -Wall -Wno-unused-function -ffunction-sections -fdata-sections \
         -ffreestanding -MMD -MP \
         -DN64_TARGET -DASSET_DEBUG
INCLUDES = -I$(BOOTLOADER_SRC) -I$(ASSET_SRC) -Iinclude \
           -I$(TRX_SRC)/..
LDFLAGS = -lc -lm -nostartfiles -Wl,--gc-sections

# ---- Source files ----

# Bootloader infrastructure (display, IO, controller, SC64)
BOOT_SRCS = \
	startup.S exception.c exception.S \
	display.c font.c init.c \
	interrupts.c interrupts.S \
	io.c joybus.c sc64.c syscalls.c

# Asset streaming library
ASSET_SRCS = \
	asset_manager.c asset_cache.c \
	lz4_decode.c asset_debug.c

# N64 port layer
PORT_SRCS = \
	src/main.c \
	src/platform/n64_platform.c \
	src/render/n64_render.c \
	src/render/n64_transform.c \
	src/render/n64_room_draw.c \
	src/level/n64_level_loader.c \
	src/level/tr_level_parse.c

# TRX game logic (portable C files)
# These are added incrementally as we integrate each subsystem.
# Start with core types and math, then rooms, items, etc.
TRX_GAME_SRCS = \
	game/random.c \
	game/matrix.c \
	game/viewport.c \
	game/game_buf.c

TRX_CORE_SRCS = \
	core/memory.c \
	core/vector.c

# ---- Object file generation ----

BOOT_OBJS = $(addprefix $(BUILD)/boot_,$(addsuffix .o,$(BOOT_SRCS)))
ASSET_OBJS = $(addprefix $(BUILD)/asset_,$(addsuffix .o,$(ASSET_SRCS)))
PORT_OBJS = $(addprefix $(BUILD)/port_,$(addsuffix .o,$(notdir $(PORT_SRCS))))
TRX_GAME_OBJS = $(addprefix $(BUILD)/trx_,$(subst /,_,$(addsuffix .o,$(TRX_GAME_SRCS))))
TRX_CORE_OBJS = $(addprefix $(BUILD)/trx_,$(subst /,_,$(addsuffix .o,$(TRX_CORE_SRCS))))

ALL_OBJS = $(BOOT_OBJS) $(ASSET_OBJS) $(PORT_OBJS) $(TRX_GAME_OBJS) $(TRX_CORE_OBJS)
DEPS = $(ALL_OBJS:.o=.d)

$(shell mkdir -p $(BUILD))

# Linker script (extended from bootloader — larger RAM for game)
LDSCRIPT = N64-trx.ld

# ---- Build rules ----

all: $(BUILD)/trx-n64.z64

# Bootloader infrastructure
$(BUILD)/boot_%.S.o: $(BOOTLOADER_SRC)/%.S
	$(CC) -x assembler-with-cpp $(FLAGS) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD)/boot_%.c.o: $(BOOTLOADER_SRC)/%.c
	$(CC) $(FLAGS) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Asset streaming
$(BUILD)/asset_%.c.o: $(ASSET_SRC)/%.c
	$(CC) $(FLAGS) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Port layer
$(BUILD)/port_%.c.o: src/%.c
	$(CC) $(FLAGS) $(CFLAGS) $(INCLUDES) -c $< -o $@
$(BUILD)/port_n64_platform.c.o: src/platform/n64_platform.c
	$(CC) $(FLAGS) $(CFLAGS) $(INCLUDES) -c $< -o $@
$(BUILD)/port_n64_render.c.o: src/render/n64_render.c
	$(CC) $(FLAGS) $(CFLAGS) $(INCLUDES) -c $< -o $@
$(BUILD)/port_n64_level_loader.c.o: src/level/n64_level_loader.c
	$(CC) $(FLAGS) $(CFLAGS) $(INCLUDES) -c $< -o $@
$(BUILD)/port_n64_transform.c.o: src/render/n64_transform.c
	$(CC) $(FLAGS) $(CFLAGS) $(INCLUDES) -c $< -o $@
$(BUILD)/port_n64_room_draw.c.o: src/render/n64_room_draw.c
	$(CC) $(FLAGS) $(CFLAGS) $(INCLUDES) -c $< -o $@
$(BUILD)/port_tr_level_parse.c.o: src/level/tr_level_parse.c
	$(CC) $(FLAGS) $(CFLAGS) $(INCLUDES) -c $< -o $@

# TRX game logic
$(BUILD)/trx_game_%.c.o: $(TRX_SRC)/game/%.c
	$(CC) $(FLAGS) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD)/trx_core_%.c.o: $(TRX_SRC)/core/%.c
	$(CC) $(FLAGS) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Link
$(BUILD)/trx-n64.elf: $(ALL_OBJS) $(LDSCRIPT)
	$(CXX) $(FLAGS) $(LDFLAGS) -T$(LDSCRIPT) $(ALL_OBJS) -o $@
	@echo ""
	@$(SIZE) -B -d $@

# Convert to N64 ROM
$(BUILD)/trx-n64.z64: $(BUILD)/trx-n64.elf
	@cp $< $<.stripped
	@$(STRIP) -s $<.stripped
	@$(N64_TOOL) --title "TOMB RAIDER N64" --output $@ --align 256 $<.stripped
	@echo ""
	@echo "ROM: $@ ($$(stat -c%s $@ 2>/dev/null || stat -f%z $@ 2>/dev/null) bytes)"

# ---- Asset packing ----

# Convert TR1 game data to SC64 asset archive
pack: tools/tr1_pack.py $(ASSET_TOOLS)/asset_pack.py
	$(PYTHON) tools/tr1_pack.py data/ $(BUILD)/
	@echo ""
	@echo "Write $(BUILD)/tr1_assets.bin to SD card:"
	@echo "  dd if=$(BUILD)/tr1_assets.bin of=/dev/sdX bs=512 seek=65536"

# ---- Clean ----

clean:
	rm -rf $(BUILD)

.PHONY: all pack clean

-include $(DEPS)
