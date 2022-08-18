EXE_NAME = app
LD_SCRIPT = app.ld
BUILD_DIR = build/app
SRC_FILES = \
	app.S \
	app.c \
	cfg.c \
	cic.c \
	dd.c \
	debug.c \
	flash.c \
	flashram.c \
	fpga.c \
	gvr.c \
	hw.c \
	isv.c \
	lcmxo2.c \
	rtc.c \
	task.c \
	update.c \
	usb.c
PAD_TO = 0x08008000

include common.mk

$(BUILD_DIR)/app.S.o: ../build/loader/loader.bin
