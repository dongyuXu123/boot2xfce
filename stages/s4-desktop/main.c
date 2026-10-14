/* main.c —— S3 总装：在 S2 四件套之上加分页与虚存
 *   M1 分页: paging_init（恒等映射内核）+ 按需零页
 *   后续里程碑: 每进程地址空间 / COW fork / ELF / VFS+fd / TTY+shell
 */
#include "kernel.h"

struct mb_info *g_mb;

void kmain(uint32_t magic, struct mb_info *mb) {
    console_init();
    print("S4: booting, multiboot magic=");
    print_hex(magic);
    print("\n");

    gdt_init();
    idt_init();
    pic_init();
    pit_init(100);
    tty_init();
    mouse_init();
    print("S4: GDT/IDT/PIC/PIT ready\n");

    pmm_init(mb);
    print("S4: pmm frames=");
    print_dec(pmm_total_frames());
    print("\n");
    heap_init();

    /* ---- M1：开分页（内核恒等映射），再做按需零页自检 ---- */
    paging_init();
    print("S4: paging ON\n");
    {
        uint32_t *probe = (uint32_t *)(USER_BASE + 0x800000);  /* 从未映射处 */
        *probe = 0x1234;               /* 触发缺页 → 分配零页 → 重试成功 */
        if (*probe == 0x1234 && probe[1] == 0)
            print("S4: demand page OK\n");
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
        print("S4: ramfs read-back OK\n");
    {
        int ok = ramfs_mkdir("/home") == 0 && ramfs_mkdir("/home/user") == 0
              && ramfs_write("/home/user/t.txt", "x", 1) == 1
              && ramfs_chdir("/home/user") == 0 && ramfs_rm("t.txt") == 0
              && ramfs_chdir("/") == 0;
        print(ok ? "S4: fs tree OK\n" : "S4: fs tree BROKEN\n");
    }

    /* ---- 程序入库：/bin/init（自启）与 /bin/hello（execve 目标） ---- */
    extern uint8_t init_blob[], init_blob_end[], hello_blob[], hello_blob_end[];
    extern uint8_t sh_blob[], sh_blob_end[];
    extern uint8_t calc_blob[], calc_blob_end[];
    extern uint8_t clock_blob[], clock_blob_end[];
    extern uint8_t notepad_blob[], notepad_blob_end[];
    ramfs_write("/bin/init",  init_blob,  init_blob_end - init_blob);
    ramfs_write("/bin/hello", hello_blob, hello_blob_end - hello_blob);
    ramfs_write("/bin/sh",    sh_blob,    sh_blob_end - sh_blob);
    ramfs_write("/bin/calc",  calc_blob,  calc_blob_end - calc_blob);
    ramfs_write("/bin/clock", clock_blob, clock_blob_end - clock_blob);
    ramfs_write("/bin/notepad", notepad_blob, notepad_blob_end - notepad_blob);

    /* ---- 进程：轮转任务 + 键盘 + ELF 方式启动的 init ---- */
    tasks_init();
    task_create(kbd_task, 0);
    task_create(gfx_task, 0);
    {
        task_t *u = task_create(NULL, 1);
        if (task_exec_image(u, "/bin/init") == 0)
            print("S4: per-task pd + ELF image OK\n");
        else {
            print("S3: exec image BROKEN\n");
            for (;;) hlt();
        }
    }

    print("S4: kernel core ready, starting scheduler\n");
    sched_loop();
}
