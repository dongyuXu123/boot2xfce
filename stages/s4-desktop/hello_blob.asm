section .userblob
global hello_blob, hello_blob_end
hello_blob:    incbin "build/hello.elf"
hello_blob_end:
