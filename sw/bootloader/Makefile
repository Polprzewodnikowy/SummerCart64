ROOTDIR = $(N64_INST)

GCCN64PREFIX = $(ROOTDIR)/bin/mips64-elf-
CC = $(GCCN64PREFIX)gcc
AS = $(GCCN64PREFIX)as
LD = $(GCCN64PREFIX)ld
OBJCOPY = $(GCCN64PREFIX)objcopy
OBJDUMP = $(GCCN64PREFIX)objdump

CHKSUM64 = $(ROOTDIR)/bin/chksum64
MKSPRITE = $(ROOTDIR)/bin/mksprite
N64TOOL = $(ROOTDIR)/bin/n64tool

HEADER_PATH = $(ROOTDIR)/mips64-elf/lib
HEADER_NAME = header

PROG_NAME = SummerLoader64

ROM_SIZE = 90k

SOURCE_DIR = src
BUILD_DIR = build

SRC_DIRS = $(SOURCE_DIR) $(sort $(dir $(wildcard $(SOURCE_DIR)/*/.)))
INC_DIRS = $(addprefix -I, . $(SRC_DIRS))
SRC_FILES = $(wildcard $(patsubst %, %/*.c, . $(SRC_DIRS)))
IMG_FILES = $(wildcard $(patsubst %, %/*.png, . $(SRC_DIRS)))
OBJ_FILES = $(addprefix $(BUILD_DIR)/, $(notdir $(IMG_FILES:.png=.o) $(SRC_FILES:.c=.o)))

VPATH = $(SRC_DIRS)

COMMONFLAGS = -march=vr4300 -mtune=vr4300
ASFLAGS = $(COMMONFLAGS)
CFLAGS = $(COMMONFLAGS) -std=gnu99 -Os -Wall -I$(ROOTDIR)/mips64-elf/include $(INC_DIRS)
LINK_FLAGS = -L$(ROOTDIR)/mips64-elf/lib -ldragon -lc -lm -ldragonsys -Tn64.ld
N64_FLAGS = -l $(ROM_SIZE) -h $(HEADER_PATH)/$(HEADER_NAME) -o $(BUILD_DIR)/$(PROG_NAME).z64

all: make_output_dir $(BUILD_DIR)/$(PROG_NAME).z64

$(OBJ_FILES): Makefile

$(BUILD_DIR)/$(PROG_NAME).z64: $(BUILD_DIR)/$(PROG_NAME).elf
	$(OBJCOPY) $(BUILD_DIR)/$(PROG_NAME).elf $(BUILD_DIR)/$(PROG_NAME).bin -O binary
	$(OBJDUMP) -S $(BUILD_DIR)/$(PROG_NAME).elf > $(BUILD_DIR)/$(PROG_NAME).lst
	$(N64TOOL) $(N64_FLAGS) -t $(PROG_NAME) $(BUILD_DIR)/$(PROG_NAME).bin
	$(CHKSUM64) $(BUILD_DIR)/$(PROG_NAME).z64
	$(OBJCOPY) $(BUILD_DIR)/$(PROG_NAME).z64 $(BUILD_DIR)/$(PROG_NAME).hex -I binary -O ihex

$(BUILD_DIR)/$(PROG_NAME).elf: $(OBJ_FILES)
	$(LD) -o $(BUILD_DIR)/$(PROG_NAME).elf $(OBJ_FILES) $(LINK_FLAGS)

$(BUILD_DIR)/%.o: %.c
	$(COMPILE.c) $(OUTPUT_OPTION) $<

$(BUILD_DIR)/%.sprite: $(IMG_FILES)
	$(MKSPRITE) 32 $< $@

$(BUILD_DIR)/%.o: $(BUILD_DIR)/%.sprite
	$(OBJCOPY) -I binary -O elf32-bigmips -B mips:4000 --rename-section .data=.rodata $< $@

make_output_dir:
	$(shell mkdir ./$(BUILD_DIR) 2> /dev/null)

clean:
	$(shell rm -rf ./$(BUILD_DIR) 2> /dev/null)

.PHONY: all clean make_output_dir
