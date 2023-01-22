EXE_NAME = loader
BUILD_DIR = build/loader

LD_SCRIPT = loader.ld

SRC_FILES = \
	loader.S \
	flash.c \
	fpga.c \
	hw.c \
	lcmxo2.c \
	loader.c \
	update.c

include common.mk
