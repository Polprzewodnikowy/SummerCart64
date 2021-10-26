#!/bin/bash

docker run \
    --mount type=bind,src="$(pwd)/..",target="/workdir" \
    ghcr.io/polprzewodnikowy/sc64env:v1.0 \
    /bin/bash -c " \
        cd fw && \
        quartus_sh --flow compile ./SummerCart64.qpf && \
        quartus_cpf -c ./SummerCart64.cof && \
        cp output_files/SC64_firmware.pof output_files/SC64_update.pof && \
        cat output_files/sc64_firmware_ufm_auto.rpd output_files/sc64_firmware_cfm0_auto.rpd > output_files/SC64_update_LE.bin && \
        riscv32-unknown-elf-objcopy -I binary -O binary --reverse-bytes=4 output_files/SC64_update_LE.bin output_files/SC64_update.bin
    "
