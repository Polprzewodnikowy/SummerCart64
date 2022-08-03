EXE_NAME = loader
LD_SCRIPT = loader.ld
BUILD_DIR = build_loader
SRC_FILES = \
	loader_startup.S \
	boot.c \
	debug.c \
	fpga.c \
	hw.c \
	lcmxo2.c \
	loader_main.c

include common.mk
