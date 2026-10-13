/* [课程模板 · S2] 进度门控 kmain —— 每课验收的"主考官"
 *
 * 本文件是给定的（不许改）：每过一课，它多启用一段自检代码，
 * 把 "S2: xxx OK" 打到串口；课程 Makefile 按 progress.h 的 LESSON
 * 值断言这些标记。桩写没写好，看标记就知道——不用口说无凭。
 * ============================================================ */
#include "kernel.h"
#include "progress.h"

void kmain(uint32_t magic, struct mb_info *mb) {
    /* ---- L08 console ---- */
    console_init();
    print("S2: console OK\n");

#if LESSON >= 9
    /* ---- L09 GDT：自检 gdtr.limit 应为 6*8-1 ---- */
    gdt_init();
    {
        struct { uint16_t limit; uint32_t base; } __attribute__((packed)) gdtr;
        __asm__ volatile("sgdt %0" : "=m"(gdtr));
        if (gdtr.limit == 47 && gdtr.base >= 0x100000u && gdtr.base < 0x400000u)
            print("S2: gdt OK\n");
        else
            print("S2: gdt LIMIT WRONG\n");
    }
#endif
#if LESSON >= 10
    /* ---- L10 IDT：自检 idtr.limit 应为 256*8-1，基址须在内核区 ---- */
    idt_init();
    {
        struct { uint16_t limit; uint32_t base; } __attribute__((packed)) idtr;
        __asm__ volatile("sidt %0" : "=m"(idtr));
        if (idtr.limit == 2047 && idtr.base >= 0x100000u && idtr.base < 0x400000u)
            print("S2: idt OK\n");
        else
            print("S2: idt WRONG\n");
    }
#endif
#if LESSON >= 11
    /* ---- L11 PIC+PIT：开中断收 3 个 tick 才算过 ---- */
    pic_init();
    pit_init(100);
    sti();
    while (ticks < 3) { __asm__ volatile("pause"); }
    cli();
    print("S2: pic+pit OK\n");
#endif
#if LESSON >= 12
    /* ---- L12 PMM：连发两页必须不同，释放后重发必须拿到同一页 ---- */
    pmm_init(mb);
    {
        uint32_t a = pmm_alloc_frame();
        uint32_t b = pmm_alloc_frame();
        int ok = a && b && a != b && (a & 0xFFF) == 0 && a >= 0x100000u;
        pmm_free_frame(a);
        uint32_t c = pmm_alloc_frame();
        if (ok && c == a) print("S2: pmm OK\n");
        else              print("S2: pmm WRONG\n");
    }
#endif
#if LESSON >= 13
    /* ---- L13 堆：分配/释放/复用同一地址 ---- */
    heap_init();
    {
        void *p1 = kmalloc(100);
        void *p2 = kmalloc(200);
        int ok = p1 && p2 && p1 != p2;
        kfree(p2);
        void *q = kmalloc(150);
        if (ok && q == p2) print("S2: heap OK\n");
        else               print("S2: heap WRONG\n");
    }
#endif
#if LESSON >= 15
    /* ---- L15 ramfs：写 13 字节读回须一致 ---- */
    ramfs_init();
    ramfs_write("hello.txt", "hello,ramfs!\n", 13);
    {
        char buf[64];
        int n = ramfs_read("hello.txt", buf, sizeof(buf) - 1);
        if (n == 13 && buf[0] == 'h' && buf[12] == '\n')
            print("S2: ramfs OK\n");
        else
            print("S2: ramfs WRONG\n");
    }
#endif
#if LESSON >= 16
    /* ---- L16 系统调用：ring0 里直接走 int 0x80 ---- */
    {
        const char *m = "S2: syscall OK\n";
        uint32_t r;
        __asm__ volatile("int $0x80" : "=a"(r) : "a"(1), "b"(1), "c"(m), "d"(15));
    }
#endif

#if LESSON >= 14
    /* ---- L14 任务与调度（之后每课都带着跑）---- */
    tasks_init();
    task_create(task_a, 0);
    task_create(task_b, 0);
#if LESSON >= 18
    task_create(kbd_task, 0);
#endif
#if LESSON >= 17
    extern uint8_t user_blob[], user_blob_end[];
    memcpy((void *)USER_BASE, user_blob, (uint32_t)(user_blob_end - user_blob));
    task_create(NULL, 1);
#endif
    print("S2: sched on\n");
    sched_loop();
#endif
    for (;;) __asm__ volatile("hlt");
}
