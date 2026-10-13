section .userblob
global sh_blob, sh_blob_end
sh_blob:       incbin "build/sh.elf"
sh_blob_end:
