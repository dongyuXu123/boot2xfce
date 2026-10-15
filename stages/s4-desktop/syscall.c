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
    case 3: {                                   /* exit */
        current->state = T_DEAD;
        gfx_on_exit(current->pid);
        { extern void debug_ser(const char *);
          extern void debug_hex(uint32_t);
          debug_ser("[exit pid="); debug_hex(current->pid); debug_ser("]\n"); }
        print("S3: [user] exit, pid=");
        print_dec(current->pid);
        print("\n");
        r->eax = 0;
        /* 必须立刻切走且永不返回：死任务的内核栈随时会被 reap 的
         * kfree 收回，而 TSS esp0 还指着它——若死任务继续霸占 CPU
         * （哪怕挂在用户态死循环），每个中断都往"已释放的栈"里压帧，
         * 随机腐蚀新主人（context 弹出 eip=3、画布内容蒸发……）。
         * （旧代码标完 DEAD 就 break 返回，靠用户桩 .hang 死循环侥幸。） */
        schedule();
        for (;;) hlt();
        break;
    }
    case 4:                                     /* uptime */
        r->eax = ticks;
        break;
    case 5: {                                   /* ls([path]) */
        static char kp[64];
        const char *pp = ".";
        if (r->ebx) {
            for (int i = 0; i < 63; i++) { kp[i] = ((char *)(uintptr_t)r->ebx)[i]; if (!kp[i]) break; }
            kp[63] = 0;
            pp = kp;
        }
        ramfs_list(pp);
        r->eax = 0;
        break;
    }
    case 7: {                                   /* fork()：父得 pid，子得 0 */
        task_t *ch = task_clone(current, r);
        r->eax = ch ? ch->pid : (uint32_t)-1;
        break;
    }
    case 8: {                                   /* wait()：收尸一个死子，阻塞 */
        for (;;) {
            extern uint32_t task_wait_scan(uint32_t pid);
            uint32_t zpid = task_wait_scan(current->pid);
            if (zpid) { r->eax = zpid; break; }
            schedule();                          /* 还没有就让出 CPU（无阻塞原语的教学实现） */
        }
        break;
    }
    case 10: {                                  /* execve(name)：原位换像 */
        static char kname[16];                  /* 用户指针先拷进内核（铁律） */
        for (int i = 0; i < 15; i++) {
            kname[i] = ((char *)(uintptr_t)r->ebx)[i];
            if (!kname[i]) break;
        }
        kname[15] = 0;
        int rc = task_exec_image(current, kname);
        { extern void debug_ser(const char *);
          debug_ser(rc == 0 ? "[exec ok]\n" : "[exec fail]\n"); }
        if (rc == 0) {
            /* gfx 模式：为换像后的程序开自己的窗口（应用窗） */
            if (gfx_active()) {
                const char *base = kname;
                for (const char *q = kname; *q; q++) if (*q == '/') base = q + 1;
                gfx_spawn_window(current->pid, base);
            }
            /* 改写 int 0x80 的 iret 帧：iretd 一执行就落进新程序 */
            r->eip = current->ueip;
            r->esp = current->uesp;
            r->eax = 0;
        } else {
            r->eax = (uint32_t)-1;
        }
        break;
    }
    case 11: {                                  /* open(name) → fd */
        static char kn[16];
        for (int i = 0; i < 15; i++) {
            kn[i] = ((char *)(uintptr_t)r->ebx)[i];
            if (!kn[i]) break;
        }
        kn[15] = 0;
        r->eax = (uint32_t)fd_open(current, kn);
        break;
    }
    case 12: {                                  /* read(fd, ubuf, n) */
        uint32_t ub = r->ecx, n2 = r->edx;
        static char kb[512];
        if (!uptr_ok(ub, n2) || n2 > 512) { r->eax = (uint32_t)-1; break; }
        int rr = fd_read(current, (int)r->ebx, kb, n2);
        if (rr > 0) memcpy((void *)(uintptr_t)ub, kb, (uint32_t)rr);
        r->eax = (uint32_t)rr;
        break;
    }
    case 13: {                                  /* write(fd, ubuf, n) */
        uint32_t ub = r->ecx, n2 = r->edx;
        static char kb2[512];
        if (!uptr_ok(ub, n2) || n2 > 512) { r->eax = (uint32_t)-1; break; }
        memcpy(kb2, (void *)(uintptr_t)ub, n2);
        if ((int)r->ebx == 1) {                 /* fd1 = 控制台 */
            for (uint32_t i = 0; i < n2; i++) { char b[2] = { kb2[i], 0 }; print(b); }
            r->eax = n2;
        } else {
            r->eax = (uint32_t)fd_write(current, (int)r->ebx, kb2, n2);
        }
        break;
    }
    case 14:                                    /* close(fd) */
        r->eax = (uint32_t)fd_close(current, (int)r->ebx);
        break;
    case 15:                                    /* lseek(fd, off, whence) */
        r->eax = (uint32_t)fd_lseek(current, (int)r->ebx, (int32_t)r->ecx, (int)r->edx);
        break;

    case 22: {                                  /* spawn(path)：新任务+窗口 */
        static char ks[24];
        for (int i = 0; i < 23; i++) { ks[i] = ((char *)(uintptr_t)r->ebx)[i]; if (!ks[i]) break; }
        ks[23] = 0;
        const char *base = ks;
        for (const char *q = ks; *q; q++) if (*q == '/') base = q + 1;
        int pid = task_spawn(ks, current->pid, base);
        r->eax = (uint32_t)pid;
        break;
    }
    case 23:                                    /* time()：HHMMSS */
        r->eax = rtc_time();
        break;
    case 24:                                    /* clear_screen：文本台清屏或清聚焦窗 */
        if (gfx_active()) {
            extern void gfx_clear_focused(void);
            gfx_clear_focused();
        } else {
            console_clear();
        }
        r->eax = 0;
        break;
    case 25:                                    /* wgfx()：本窗转画布模式 */
        r->eax = (uint32_t)gfx_mode_on(current->pid);
        break;
    case 26: {                                  /* wrect(&{x,y,w,h,color}) */
        static struct { int x, y, w2, h2; uint32_t color; } rc;
        if (!uptr_ok(r->ebx, sizeof(rc))) { r->eax = (uint32_t)-1; break; }
        memcpy(&rc, (void *)(uintptr_t)r->ebx, sizeof(rc));
        r->eax = (uint32_t)gfx_rect(current->pid, rc.x, rc.y, rc.w2, rc.h2, rc.color);
        break;
    }
    case 27: {                                  /* wtext(&{x,y,color,s[48]}) */
        static struct { int x, y; uint32_t color; char s[48]; } tx;
        if (!uptr_ok(r->ebx, sizeof(tx))) { r->eax = (uint32_t)-1; break; }
        memcpy(&tx, (void *)(uintptr_t)r->ebx, sizeof(tx));
        tx.s[47] = 0;
        r->eax = (uint32_t)gfx_text(current->pid, tx.x, tx.y, tx.color, tx.s);
        break;
    }
    case 28: {                                  /* wevent(&ev) → 1 有事件 */
        if (!uptr_ok(r->ebx, sizeof(struct wmev))) { r->eax = (uint32_t)-1; break; }
        struct wmev ev;
        int got = gfx_ev_pop(current->pid, &ev);
        if (got) memcpy((void *)(uintptr_t)r->ebx, &ev, sizeof(ev));
        r->eax = (uint32_t)got;
        break;
    }
    case 30: {                                  /* winsize(&w, &h)：本窗尺寸 */
        if (!uptr_ok(r->ebx, 4) || !uptr_ok(r->ecx, 4)) { r->eax = (uint32_t)-1; break; }
        extern int win_size_of(uint32_t pid, uint32_t *w, uint32_t *h);
        uint32_t ww = 0, hh = 0;
        if (win_size_of(current->pid, &ww, &hh) != 0) { r->eax = (uint32_t)-1; break; }
        *(uint32_t *)(uintptr_t)r->ebx = ww;
        *(uint32_t *)(uintptr_t)r->ecx = hh;
        r->eax = 0;
        break;
    }
    case 29:                                    /* wyield()：协作让出 */
        task_yield();
        r->eax = 0;
        break;
    case 17:                                    /* gfx：进入图形演示 */
        gfx_start();
        r->eax = 0;
        break;
    case 18: {                                  /* mkdir(path) */
        static char kn[64];
        for (int i = 0; i < 63; i++) { kn[i] = ((char *)(uintptr_t)r->ebx)[i]; if (!kn[i]) break; }
        kn[63] = 0;
        r->eax = (uint32_t)ramfs_mkdir(kn);
        break;
    }
    case 19: {                                  /* rm(path) */
        static char kn2[64];
        for (int i = 0; i < 63; i++) { kn2[i] = ((char *)(uintptr_t)r->ebx)[i]; if (!kn2[i]) break; }
        kn2[63] = 0;
        r->eax = (uint32_t)ramfs_rm(kn2);
        break;
    }
    case 20: {                                  /* chdir(path) */
        static char kn3[64];
        for (int i = 0; i < 63; i++) { kn3[i] = ((char *)(uintptr_t)r->ebx)[i]; if (!kn3[i]) break; }
        kn3[63] = 0;
        r->eax = (uint32_t)ramfs_chdir(kn3);
        break;
    }
    case 21: {                                  /* getcwd(buf, max) */
        static char kc[128];
        int n2 = ramfs_getcwd(kc, 128);
        uint32_t ub = r->ebx, lim = r->edx;
        if (!uptr_ok(ub, lim) || lim > 127) { r->eax = (uint32_t)-1; break; }
        if ((uint32_t)n2 > lim) n2 = (int)lim;
        memcpy((void *)(uintptr_t)ub, kc, (uint32_t)n2);
        ((char *)(uintptr_t)ub)[n2] = 0;
        r->eax = (uint32_t)n2;
        break;
    }
    default:
        r->eax = (uint32_t)-1;
        break;
    }
}
