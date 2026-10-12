/* main.c —— S2 总装：从 GRUB 手里接过来，把四件套立起来
 *   内存: pmm_init/heap_init   进程: tasks_init/sched_loop
 *   文件: ramfs_init+读写回环  API:  int 0x80 + ring3 用户任务
 */
#include "kernel.h"

void kmain(uint32_t magic, struct mb_info *mb) {
    console_init();
    print("S2: booting, multiboot magic=");
    print_hex(magic);
    print("\n");

    gdt_init();
    idt_init();
    pic_init();
    pit_init(100);                      /* 100Hz 时钟片 */
    print("S2: GDT/IDT/PIC/PIT ready\n");

    pmm_init(mb);
    print("S2: pmm frames=");
    print_dec(pmm_total_frames());
    print("\n");
    heap_init();

    /* ---- 文件管理冒烟：写 hello.txt 再读回 ---- */
    ramfs_init();
    const char *msg = "hello,ramfs!\n";
    ramfs_write("hello.txt", msg, 13);
    char buf[64];
    int n = ramfs_read("hello.txt", buf, sizeof(buf) - 1);
    buf[n > 0 ? n : 0] = 0;
    if (n == 13 && buf[0] == 'h' && buf[12] == '\n')
        print("S2: ramfs read-back OK\n");
    else {
        print("S2: ramfs BROKEN\n");
        for (;;) hlt();
    }

    /* ---- 进程：两个轮转任务 + 键盘任务 + 一个 ring3 用户任务 ---- */
    tasks_init();
    extern uint8_t user_blob[], user_blob_end[];   /* blob.asm 经 incbin 嵌入 */
    memcpy((void *)USER_BASE, user_blob, (uint32_t)(user_blob_end - user_blob));
    task_create(task_a, 0);
    task_create(task_b, 0);
    task_create(kbd_task, 0);
    task_create(NULL, 1);               /* 用户程序已在 USER_BASE */

    print("S2: kernel core ready, starting scheduler\n");
    sched_loop();
}
