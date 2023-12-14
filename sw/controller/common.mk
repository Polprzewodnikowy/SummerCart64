TOOLCHAIN = arm-none-eabi-
CC = $(TOOLCHAIN)gcc
CXX = $(TOOLCHAIN)g++
OBJCOPY = $(TOOLCHAIN)objcopy
OBJDUMP = $(TOOLCHAIN)objdump
SIZE = $(TOOLCHAIN)size

FLAGS = -mcpu=cortex-m0plus -mthumb -DSTM32G030xx $(USER_FLAGS) -g -ggdb3
CFLAGS = -Os -Wall -ffunction-sections -fdata-sections -ffreestanding -MMD -MP -I. -isystem ./inc
LDFLAGS = -nostartfiles -Wl,--gc-sections

SRC_DIR = src

SRCS = $(addprefix $(SRC_DIR)/, $(SRC_FILES))
OBJS = $(addprefix $(BUILD_DIR)/, $(notdir $(patsubst %,%.o,$(SRCS))))
DEPS = $(OBJS:.o=.d)
VPATH = $(SRC_DIR)

$(@info $(shell mkdir -p ./$(BUILD_DIR) &> /dev/null))

$(BUILD_DIR)/%.S.o: %.S
	$(CC) -x assembler-with-cpp $(FLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.c.o: %.c
	$(CC) $(FLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/$(EXE_NAME).elf: $(OBJS) $(LD_SCRIPT)
	$(CXX) $(FLAGS) $(LDFLAGS) -T$(LD_SCRIPT) $(OBJS) -o $@
	@$(OBJDUMP) -S -D $@ > $(BUILD_DIR)/$(EXE_NAME).lst

$(BUILD_DIR)/$(EXE_NAME).bin: $(BUILD_DIR)/$(EXE_NAME).elf
	$(OBJCOPY) -O binary --gap-fill 0xFF $< $@

print_size: $(BUILD_DIR)/$(EXE_NAME).elf
	@echo 'Size of modules:'
	@$(SIZE) -B -d -t --common $(OBJS)
	@echo 'Size of $(EXE_NAME):'
	@$(SIZE) -B -d $<

all: $(BUILD_DIR)/$(EXE_NAME).bin print_size

clean:
	@rm -rf ./$(BUILD_DIR)/*

.PHONY: all clean print_size

-include $(DEPS)
