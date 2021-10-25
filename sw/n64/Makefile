BUILD_DIR = build
SOURCE_DIR = src
PROGRAM_NAME = SummerLoader64

include $(N64_INST)/include/n64.mk

src = main.c sc64.c boot.c crc32.c

all: $(BUILD_DIR)/$(PROGRAM_NAME).hex

$(BUILD_DIR)/$(PROGRAM_NAME).elf: $(src:%.c=$(BUILD_DIR)/%.o)

$(PROGRAM_NAME).z64: N64_ROM_TITLE="$(PROGRAM_NAME)"

$(BUILD_DIR)/$(PROGRAM_NAME).hex: $(PROGRAM_NAME).z64
	sed 's/\x00*$$//' $(PROGRAM_NAME).z64 > $(BUILD_DIR)/$(PROGRAM_NAME)_stripped.z64
	@if [ $$(stat -L -c %s $(BUILD_DIR)/$(PROGRAM_NAME)_stripped.z64) -gt 92160 ]; then\
		echo "\n    Error: stripped file size is larger than 90kB thus cannot fit inside FPGA flash.\n"; exit 1;\
	fi
	truncate --size=90k $(BUILD_DIR)/$(PROGRAM_NAME)_stripped.z64
	$(N64_OBJCOPY) -I binary -O ihex $(BUILD_DIR)/$(PROGRAM_NAME)_stripped.z64 $(BUILD_DIR)/$(PROGRAM_NAME).hex

clean:
	rm -rf ./$(BUILD_DIR) ./$(PROGRAM_NAME).z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
