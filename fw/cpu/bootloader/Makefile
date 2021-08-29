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
	-Tprv32_rx.ld \
	-Wl,--gc-sections \
	-ffreestanding \
	-nostartfiles \
	-nostdlib

all: bootloader.bin cpu_bootloader.sv print_size

bootloader.elf: prv32_rx.ld main.c
	@$(CROSS)gcc $(FLAGS) main.c -o bootloader.elf

bootloader.bin: bootloader.elf
	@$(CROSS)objcopy -O binary bootloader.elf bootloader.bin

cpu_bootloader.sv: bootloader.bin
	@python3 bin2sv.py bootloader.bin cpu_bootloader_template.sv ../../rtl/cpu/cpu_bootloader.sv

print_size:
	@echo 'Size of target .elf file:'
	$(CROSS)size -B bootloader.elf

clean:
	@rm -f bootloader.bin bootloader.elf

.PHONY: clean
