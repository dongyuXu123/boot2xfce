section .userblob
global calc_blob, calc_blob_end
calc_blob:      incbin "build/calc.elf"
calc_blob_end:
