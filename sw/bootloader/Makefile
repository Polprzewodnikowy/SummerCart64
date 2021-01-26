ROOTDIR = $(N64_INST)
GCCN64PREFIX = $(ROOTDIR)/bin/mips64-elf-
CHKSUM64PATH = $(ROOTDIR)/bin/chksum64
MKDFSPATH = $(ROOTDIR)/bin/mkdfs
HEADERPATH = $(ROOTDIR)/mips64-elf/lib
N64TOOL = $(ROOTDIR)/bin/n64tool
HEADERNAME = header
LINK_FLAGS = -L$(ROOTDIR)/mips64-elf/lib -ldragon -lc -lm -ldragonsys -Tn64.ld
CFLAGS = -std=gnu99 -march=vr4300 -mtune=vr4300 -Os -Wall -I./src -I./src/boot -I./src/fatfs -I./src/sc64 -I$(ROOTDIR)/mips64-elf/include
ASFLAGS = -mtune=vr4300 -march=vr4300
CC = $(GCCN64PREFIX)gcc
AS = $(GCCN64PREFIX)as
LD = $(GCCN64PREFIX)ld
OBJCOPY = $(GCCN64PREFIX)objcopy
OBJDUMP = $(GCCN64PREFIX)objdump

SRC_DIRS = src src/boot src/fatfs src/sc64
SRC_FILES = $(wildcard $(patsubst %, %/*.c, . $(SRC_DIRS)))
OBJ_FILES = $(addprefix build/, $(notdir $(SRC_FILES:.c=.o)))
VPATH = $(SRC_DIRS)

ROM_SIZE = 90k

ifeq ($(N64_BYTE_SWAP),true)
ROM_EXTENSION = .v64
N64_FLAGS = -b -l $(ROM_SIZE) -h $(HEADERPATH)/$(HEADERNAME) -o build/$(PROG_NAME)$(ROM_EXTENSION)
else
ROM_EXTENSION = .z64
N64_FLAGS = -l $(ROM_SIZE) -h $(HEADERPATH)/$(HEADERNAME) -o build/$(PROG_NAME)$(ROM_EXTENSION)
endif

PROG_NAME = SummerLoader64

all: make_output_dir build/$(PROG_NAME)$(ROM_EXTENSION)

$(OBJ_FILES): Makefile

build/$(PROG_NAME)$(ROM_EXTENSION): build/$(PROG_NAME).elf
	$(OBJCOPY) build/$(PROG_NAME).elf build/$(PROG_NAME).bin -O binary
	$(OBJDUMP) -S build/$(PROG_NAME).elf > build/$(PROG_NAME).lst
	rm -f build/$(PROG_NAME)$(ROM_EXTENSION)
	$(N64TOOL) $(N64_FLAGS) -t $(PROG_NAME) build/$(PROG_NAME).bin
	$(CHKSUM64PATH) build/$(PROG_NAME)$(ROM_EXTENSION)
	$(OBJCOPY) build/$(PROG_NAME)$(ROM_EXTENSION) build/$(PROG_NAME).hex -I binary -O ihex

build/$(PROG_NAME).elf: $(OBJ_FILES)
	$(LD) -o build/$(PROG_NAME).elf $(OBJ_FILES) $(LINK_FLAGS)

build/%.o: %.c
	$(COMPILE.c) $(OUTPUT_OPTION) $<

make_output_dir:
	$(shell mkdir ./build 2>/dev/null)

clean:
	rm -rf ./build

.PHONY: all clean make_output_dir
