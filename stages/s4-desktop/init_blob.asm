section .userblob
global init_blob, init_blob_end
init_blob:     incbin "build/init.elf"
init_blob_end:
