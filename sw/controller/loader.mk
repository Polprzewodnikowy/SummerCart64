EXE_NAME = loader
BUILD_DIR = build/loader

LD_SCRIPT = loader.ld
PAD_TO = 0x08001000

SRC_FILES = \
	loader.S \
	flash.c \
	fpga.c \
	hw.c \
	lcmxo2.c \
	loader.c \
	update.c

include common.mk
