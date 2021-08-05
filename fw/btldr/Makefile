CROSS=riscv64-unknown-elf-
CFLAGS=-mabi=ilp32 -march=rv32i -Wl,-Bstatic,--strip-debug -ffreestanding -nostdlib 

all: btldr.hex btldr.bin btldr.sv print_size

startup.o: startup.S
	$(CROSS)gcc -x assembler-with-cpp -c $(CFLAGS) startup.s -o startup.o

btldr.o: btldr.c
	$(CROSS)gcc -c $(CFLAGS) -o btldr.o btldr.c

btldr.elf: SC64_PicoRV32I.ld startup.o btldr.o
	$(CROSS)gcc $(CFLAGS) -TSC64_PicoRV32I.ld startup.o btldr.o -o btldr.elf

btldr.hex: btldr.elf
	$(CROSS)objcopy -O ihex btldr.elf btldr.hex

btldr.bin: btldr.elf
	$(CROSS)objcopy -O binary btldr.elf btldr.bin

btldr.sv: btldr.bin
	python3 bin2v.py

print_size:
	@echo 'Size of target .elf file:'
	$(CROSS)size -B btldr.elf

clean:
	rm -f btldr.sv btldr.bin btldr.hex btldr.elf btldr.o startup.o

.PHONY: clean
