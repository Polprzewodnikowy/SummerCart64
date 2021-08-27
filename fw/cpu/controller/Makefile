CROSS=riscv64-unknown-elf-
FLAGS=\
	-mabi=ilp32 \
	-march=rv32i \
	-std=c11 \
	-Os \
	-Wall \
	-Wstrict-prototypes \
	-fverbose-asm \
	-ffunction-sections \
	-fdata-sections \
	-Tprv32_rwx.ld \
	-Wl,--gc-sections \
	-ffreestanding \
	-nostartfiles \
	-nostdlib

all: controller.bin print_size

controller.elf: prv32_rwx.ld main.c init.c process.c
	@$(CROSS)gcc $(FLAGS) main.c init.c process.c -o controller.elf

controller.bin: controller.elf
	@$(CROSS)objcopy -O binary controller.elf tmp.bin
	@python3 bin2rom.py tmp.bin controller.bin
	@rm -f tmp.bin

print_size:
	@echo 'Size of target .elf file:'
	@$(CROSS)size -B controller.elf
	@echo $(shell $(CROSS)size -B controller.elf | awk 'NR==2 { printf "\nTotal memory used: %.2f%%\n",(100/(16*1024))*($$4) }')

clean:
	@rm -f controller.bin controller.elf

.PHONY: clean
