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
	-Wl,--gc-sections \
	-ffreestanding \
	-nostartfiles \
	-nostdlib

all: cntrllr.bin print_size

cntrllr.elf: cntrllr.ld main.c rtc.c startup.S
	$(CROSS)gcc $(FLAGS) -Tcntrllr.ld main.c rtc.c startup.S -o cntrllr.elf

cntrllr.bin: cntrllr.elf
	$(CROSS)objcopy -O binary --gap-fill 0xFF --pad-to 0x6000 cntrllr.elf cntrllr.bin

print_size:
	@echo 'Size of target .elf file:'
	$(CROSS)size -B cntrllr.elf

clean:
	rm -f cntrllr.bin cntrllr.elf

.PHONY: clean
