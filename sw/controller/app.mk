EXE_NAME = app
LD_SCRIPT = app.ld
BUILD_DIR = build_app
SRC_FILES = \
	app_startup.S \
	app_main.c \
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

include common.mk

$(BUILD_DIR)/app_startup.S.o: ../build_loader/loader.bin
