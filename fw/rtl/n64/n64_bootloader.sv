module n64_bootloader (
    if_system.sys sys,
    if_n64_bus bus
);

    memory_flash memory_flash_inst (
        .sys(sys),
        .request(bus.request),
        .ack(bus.ack),
        .address(bus.address),
        .rdata(bus.rdata)
    );

endmodule
