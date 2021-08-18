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
	-Tbtldr.ld \
	-Wl,--gc-sections \
	-ffreestanding \
	-nostartfiles \
	-nostdlib

all: btldr.bin btldr.sv print_size

btldr.elf: btldr.ld btldr.c
	@$(CROSS)gcc $(FLAGS) btldr.c -o btldr.elf

btldr.bin: btldr.elf
	@$(CROSS)objcopy -O binary btldr.elf btldr.bin

btldr.sv: btldr.bin
	@python3 bin2sv.py btldr.bin btldr_template.sv btldr.sv

print_size:
	@echo 'Size of target .elf file:'
	$(CROSS)size -B btldr.elf

clean:
	@rm -f btldr.sv btldr.bin btldr.elf

.PHONY: clean
