/* main.c —— S3 总装：在 S2 四件套之上加分页与虚存
 *   M1 分页: paging_init（恒等映射内核）+ 按需零页
 *   后续里程碑: 每进程地址空间 / COW fork / ELF / VFS+fd / TTY+shell
 */
#include "kernel.h"

void kmain(uint32_t magic, struct mb_info *mb) {
    console_init();
    print("S3: booting, multiboot magic=");
    print_hex(magic);
    print("\n");

    gdt_init();
    idt_init();
    pic_init();
    pit_init(100);
    print("S3: GDT/IDT/PIC/PIT ready\n");

    pmm_init(mb);
    print("S3: pmm frames=");
    print_dec(pmm_total_frames());
    print("\n");
    heap_init();

    /* ---- M1：开分页（内核恒等映射），再做按需零页自检 ---- */
    paging_init();
    print("S3: paging ON\n");
    {
        uint32_t *probe = (uint32_t *)(USER_BASE + 0x800000);  /* 从未映射处 */
        *probe = 0x1234;               /* 触发缺页 → 分配零页 → 重试成功 */
        if (*probe == 0x1234 && probe[1] == 0)
            print("S3: demand page OK\n");
        else
            print("S3: demand BROKEN\n");
    }

    /* ---- 文件管理冒烟：写 hello.txt 再读回 ---- */
    ramfs_init();
    const char *msg = "hello,ramfs!\n";
    ramfs_write("hello.txt", msg, 13);
    char buf[64];
    int n = ramfs_read("hello.txt", buf, sizeof(buf) - 1);
    buf[n > 0 ? n : 0] = 0;
    if (n == 13 && buf[0] == 'h' && buf[12] == '\n')
        print("S3: ramfs read-back OK\n");
    else {
        print("S3: ramfs BROKEN\n");
        for (;;) hlt();
    }

    /* ---- 进程：轮转任务 + 键盘 + blob 用户任务（M2 起带独立页目录） ---- */
    tasks_init();
    extern uint8_t user_blob[], user_blob_end[];
    memcpy((void *)USER_BASE, user_blob, (uint32_t)(user_blob_end - user_blob));
    task_create(task_a, 0);
    task_create(task_b, 0);
    task_create(kbd_task, 0);
    {   /* 用户任务的代码页：先写物理帧，再映进它自己的页目录（用户可读） */
        task_t *u = task_create(NULL, 1);
        uint32_t pages = (user_blob_end - user_blob + 4095) / 4096;
        for (uint32_t i = 0; i < pages; i++)
            page_map(u->pd, USER_BASE + i * 4096, USER_BASE + i * 4096, 1, 1);
        /* 用户栈页 */
        for (uint32_t va = USER_TOP - 0x4000; va < USER_TOP; va += 4096) {
            uint32_t pa = pmm_alloc_frame();
            page_map(u->pd, va, pa, 1, 1);
        }
        print("S3: per-task pd OK\n");
    }

    print("S3: kernel core ready, starting scheduler\n");
    sched_loop();
}
