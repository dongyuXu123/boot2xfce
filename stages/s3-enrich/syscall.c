/* syscall.c —— int 0x80 系统调用（API 层）
 *
 * 约定：eax=调用号，ebx/ecx/edx=参数，eax=返回值。
 *   1 write(fd, buf, len)   fd=1 → 串口+VGA
 *   2 getpid()
 *   3 exit(code)            本任务标记 DEAD，不再被调度
 *   4 uptime()              返回 ticks
 *   5 ls()                  内核侧打印 ramfs 文件表
 * 用户指针直接用（无分页=同一地址空间）；S3 开分页后这里必须换成
 * "只翻一层"的拷贝语义，防用户传内核指针。
 */
#include "kernel.h"

void syscall_dispatch(struct regs *r) {
    switch (r->eax) {
    case 1: {                                   /* write */
        static char kbuf[256];                  /* 单 CPU，静态缓冲够用 */
        uint32_t n = r->edx;
        if (n > 255) n = 255;
        memcpy(kbuf, (void *)(uintptr_t)r->ecx, n);
        kbuf[n] = 0;
        print(kbuf);
        r->eax = n;
        break;
    }
    case 2:                                     /* getpid */
        r->eax = current->pid;
        break;
    case 3:                                     /* exit */
        current->state = T_DEAD;
        print("S2: [user] exit, pid=");
        print_dec(current->pid);
        print("\n");
        r->eax = 0;
        break;
    case 4:                                     /* uptime */
        r->eax = ticks;
        break;
    case 5:                                     /* ls */
        ramfs_list();
        r->eax = 0;
        break;
    case 7: {                                   /* fork()：父得 pid，子得 0 */
        task_t *ch = task_clone(current, r);
        r->eax = ch ? ch->pid : (uint32_t)-1;
        break;
    }
    case 8: {                                   /* wait()：收尸一个死子，阻塞 */
        for (;;) {
            task_t *z = 0;
            extern task_t *task_wait_scan(uint32_t pid);
            z = task_wait_scan(current->pid);
            if (z) { r->eax = z->pid; break; }
            schedule();                          /* 还没有就让出 CPU（无阻塞原语的教学实现） */
        }
        break;
    }
    default:
        r->eax = (uint32_t)-1;
        break;
    }
}
