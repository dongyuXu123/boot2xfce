section .userblob
global clock_blob, clock_blob_end
clock_blob:      incbin "build/clock.elf"
clock_blob_end:
