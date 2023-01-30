.section .assets.@sym@, "a", %progbits
.type assets_@sym@, %object
.global assets_@sym@
assets_@sym@:
    .incbin "@file@"
