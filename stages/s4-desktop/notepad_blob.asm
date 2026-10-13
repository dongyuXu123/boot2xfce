section .userblob
global notepad_blob, notepad_blob_end
notepad_blob:      incbin "build/notepad.elf"
notepad_blob_end:
