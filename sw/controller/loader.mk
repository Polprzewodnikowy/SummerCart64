EXE_NAME = loader
LD_SCRIPT = loader.ld
BUILD_DIR = build/loader
SRC_FILES = \
	loader.S \
	fpga.c \
	hw.c \
	lcmxo2.c \
	loader.c \
	update.c

include common.mk
