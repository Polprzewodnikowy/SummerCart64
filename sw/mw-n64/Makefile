# MW-N64: Morrowind — N64 + SC64 Port
#
# Build: N64_INST=/opt/libdragon make

N64_INST ?= /opt/libdragon
N64_BINDIR = $(N64_INST)/bin
TOOLCHAIN = $(N64_BINDIR)/mips64-elf-
CC = $(TOOLCHAIN)gcc
CXX = $(TOOLCHAIN)g++
OBJCOPY = $(TOOLCHAIN)objcopy
SIZE = $(TOOLCHAIN)size
PYTHON = python3

BOOTLOADER_SRC = ../bootloader/src
ASSET_SRC = ../asset_streaming/src
BUILD = build

FLAGS = -march=vr4300 -mtune=vr4300 -mfix4300
CFLAGS = -Os -Wall -Wno-unused-function -ffunction-sections -fdata-sections \
         -ffreestanding -MMD -MP -DN64_TARGET
INCLUDES = -I$(BOOTLOADER_SRC) -I$(ASSET_SRC) -Iinclude -Isrc
LDFLAGS = -lc -lm -nostartfiles -Wl,--gc-sections

# Bootloader infrastructure
BOOT_SRCS = startup.S exception.c exception.S display.c font.c init.c \
	interrupts.c interrupts.S io.c joybus.c sc64.c syscalls.c

# Asset streaming
ASSET_SRCS = asset_manager.c asset_cache.c lz4_decode.c asset_debug.c

# Morrowind port
PORT_SRCS = \
	src/main.c \
	src/esm/mw_esm.c \
	src/world/mw_cell_manager.c \
	src/render/mw_terrain.c \
	src/game/mw_save.c \
	src/game/mw_script.c \
	src/game/mw_character.c \
	src/game/mw_dialogue.c \
	src/world/mw_weather.c \
	src/stubs.c

# Object files
BOOT_OBJS = $(addprefix $(BUILD)/boot_,$(addsuffix .o,$(BOOT_SRCS)))
ASSET_OBJS = $(addprefix $(BUILD)/asset_,$(addsuffix .o,$(ASSET_SRCS)))
PORT_OBJS = $(addprefix $(BUILD)/port_,$(addsuffix .o,$(notdir $(PORT_SRCS))))
ALL_OBJS = $(BOOT_OBJS) $(ASSET_OBJS) $(PORT_OBJS)
DEPS = $(ALL_OBJS:.o=.d)

$(shell mkdir -p $(BUILD))

LDSCRIPT = N64-mw.ld

all: $(BUILD)/mw-n64.z64

$(BUILD)/boot_%.S.o: $(BOOTLOADER_SRC)/%.S
	$(CC) -x assembler-with-cpp $(FLAGS) $(CFLAGS) $(INCLUDES) -c $< -o $@
$(BUILD)/boot_%.c.o: $(BOOTLOADER_SRC)/%.c
	$(CC) $(FLAGS) $(CFLAGS) $(INCLUDES) -c $< -o $@
$(BUILD)/asset_%.c.o: $(ASSET_SRC)/%.c
	$(CC) $(FLAGS) $(CFLAGS) $(INCLUDES) -c $< -o $@
$(BUILD)/port_main.c.o: src/main.c
	$(CC) $(FLAGS) $(CFLAGS) $(INCLUDES) -c $< -o $@
$(BUILD)/port_mw_esm.c.o: src/esm/mw_esm.c
	$(CC) $(FLAGS) $(CFLAGS) $(INCLUDES) -c $< -o $@
$(BUILD)/port_mw_cell_manager.c.o: src/world/mw_cell_manager.c
	$(CC) $(FLAGS) $(CFLAGS) $(INCLUDES) -c $< -o $@
$(BUILD)/port_mw_terrain.c.o: src/render/mw_terrain.c
	$(CC) $(FLAGS) $(CFLAGS) $(INCLUDES) -c $< -o $@
$(BUILD)/port_mw_save.c.o: src/game/mw_save.c
	$(CC) $(FLAGS) $(CFLAGS) $(INCLUDES) -c $< -o $@
$(BUILD)/port_mw_script.c.o: src/game/mw_script.c
	$(CC) $(FLAGS) $(CFLAGS) $(INCLUDES) -c $< -o $@
$(BUILD)/port_mw_character.c.o: src/game/mw_character.c
	$(CC) $(FLAGS) $(CFLAGS) $(INCLUDES) -c $< -o $@
$(BUILD)/port_mw_dialogue.c.o: src/game/mw_dialogue.c
	$(CC) $(FLAGS) $(CFLAGS) $(INCLUDES) -c $< -o $@
$(BUILD)/port_mw_weather.c.o: src/world/mw_weather.c
	$(CC) $(FLAGS) $(CFLAGS) $(INCLUDES) -c $< -o $@
$(BUILD)/port_stubs.c.o: src/stubs.c
	$(CC) $(FLAGS) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD)/mw-n64.elf: $(ALL_OBJS) $(LDSCRIPT)
	$(CXX) $(FLAGS) $(LDFLAGS) -T$(LDSCRIPT) $(ALL_OBJS) -o $@
	@$(SIZE) -B -d $@

$(BUILD)/mw-n64.bin: $(BUILD)/mw-n64.elf
	@$(OBJCOPY) -O binary $< $@
	@echo "ROM: $@ ($$(stat -c%s $@) bytes)"

$(BUILD)/mw-n64.z64: $(BUILD)/mw-n64.bin
	@$(PYTHON) ../trx-n64/tools/make_z64.py $< $@
	@echo "Z64: $@ ($$(stat -c%s $@) bytes)"

pack: tools/mw_pack.py
	$(PYTHON) tools/mw_pack.py data/ $(BUILD)/

clean:
	rm -rf $(BUILD)

.PHONY: all pack clean
-include $(DEPS)
