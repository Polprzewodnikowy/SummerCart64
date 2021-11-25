TOOLCHAIN = mips64-elf-
CC = $(TOOLCHAIN)gcc
CXX = $(TOOLCHAIN)g++
OBJCOPY = $(TOOLCHAIN)objcopy
OBJDUMP = $(TOOLCHAIN)objdump
SIZE = $(TOOLCHAIN)size

FLAGS = -march=vr4300 -mtune=vr4300 -falign-functions=32 $(USER_FLAGS)
CFLAGS = -Os -Wall -ffunction-sections -fdata-sections -ffreestanding -MMD -MP
LDFLAGS = -lc -nostartfiles -Wl,--gc-sections

SRC_DIR = src
BUILD_DIR = build

SRC_FILES = startup.S main.c sc64.c boot.c crc32.c

SRCS = $(addprefix $(SRC_DIR)/, $(SRC_FILES))
OBJS = $(addprefix $(BUILD_DIR)/, $(notdir $(patsubst %,%.o,$(SRCS))))
DEPS = $(OBJS:.o=.d)

VPATH = $(SRC_DIR)

$(@info $(shell mkdir -p ./$(BUILD_DIR) &> /dev/null))

$(BUILD_DIR)/%.S.o: %.S
	$(CC) -x assembler-with-cpp $(FLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.c.o: %.c
	$(CC) $(FLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/n64boot.elf: $(OBJS) N64.ld
	$(CXX) $(FLAGS) $(LDFLAGS) -TN64.ld $(OBJS) -o $@
	@$(OBJDUMP) -D $@ > $(BUILD_DIR)/n64boot.lst

$(BUILD_DIR)/n64boot.bin: $(BUILD_DIR)/n64boot.elf
	@$(OBJCOPY) -O binary $< $@
	@truncate --size=90k $@
	@chksum64 $@ > /dev/null

$(BUILD_DIR)/n64boot.hex: $(BUILD_DIR)/n64boot.bin
	@$(OBJCOPY) -I binary -O ihex $< $@

print_size: $(BUILD_DIR)/n64boot.elf
	@echo 'Size of modules:'
	@$(SIZE) -B -d -t --common $(OBJS)
	@echo 'Size of n64boot:'
	@$(SIZE) -B -d $<

all: $(BUILD_DIR)/n64boot.hex print_size

clean:
	@rm -rf ./$(BUILD_DIR)/*

.PHONY: all clean print_size

-include $(DEPS)
