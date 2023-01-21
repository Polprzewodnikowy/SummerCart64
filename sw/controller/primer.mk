EXE_NAME = primer
BUILD_DIR = build/primer

LD_SCRIPT = primer.ld
PAD_TO = 0x20002000

SRC_FILES = \
	primer.S \
	fpga.c \
	hw.c \
	lcmxo2.c \
	primer.c

include common.mk

FLAGS += -DLCMXO2_I2C
