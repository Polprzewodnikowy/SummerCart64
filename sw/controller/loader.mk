EXE_NAME = loader
LD_SCRIPT = loader.ld
BUILD_DIR = build/loader
SRC_FILES = \
	loader.S \
	flash.c \
	fpga.c \
	hw.c \
	lcmxo2.c \
	loader.c \
	update.c
PAD_TO = 0x08001000

include common.mk
