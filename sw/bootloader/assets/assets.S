.section .assets.@sym@, "a", %progbits
.balign 8
.type assets_@sym@, %object
.global assets_@sym@
assets_@sym@:
    .incbin "@file@"
