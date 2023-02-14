EXE_NAME = app
BUILD_DIR = build/app

LD_SCRIPT = app.ld

SRC_FILES = \
	app.S \
	app.c \
	button.c \
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
	led.c \
	rtc.c \
	sd.c \
	task.c \
	timer.c \
	update.c \
	usb.c \
	version.c \
	writeback.c

include common.mk

$(BUILD_DIR)/app.S.o: ../build/loader/loader.bin
