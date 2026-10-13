/* task.c —— 任务表 / 创建 / 轮转调度 / 内核任务样例
 *
 * 模型（xv6 风格，也是现代 Linux 的做法）：
 *   - 每个任务一根 kmalloc 出来的 4KiB 内核栈；
 *   - 中断现场自然压在"被打断任务"的栈上；
 *   - schedule() 在中断处理里用 switch_to 换栈换上下文；
 *   - 新任务的 kctx.eip 指到蹦床：内核任务→ktask_trampoline，
 *     用户任务→utask_trampoline（iret 降级 ring3）。
 *   首次调度前 sched_loop 在主栈上空转，它自己不再被切回。
 */
#include "kernel.h"

#define MAX_TASKS 16
static task_t tasks[MAX_TASKS];
static task_t boot_task;                /* kmain 所在的"第 0 任务" */
task_t *current = &boot_task;
static uint32_t next_pid = 1;

extern void utask_trampoline(void);         /* cpu.asm */
extern void utask_trampoline_forked(void);  /* cpu.asm：fork 任务入场 */

/* 内核任务蹦床：入口函数跑完 → 标记 DEAD → 永久休眠（栈留在那儿，S3 收尸） */
static void ktask_trampoline(void) {
    /* switch_to 不保存 EFLAGS：第一次被切进来时 IF 还停在上一现场
     * （中断门把它清了）。这里 sti 重建"任务恒在 IF=1 下运行"的不变量，
     * 之后的 IF 由各任务的 iret 帧天然保存/恢复。
     * （此 bug 由 L18 的 sendkey 实测揪出：症状是 PIC 的 IRR 里 IRQ1
     *   永远 pending——整个系统只剩软触发中断在跑。） */
    sti();
    current->entry();
    current->state = T_DEAD;
    print("S2: task ");
    print_dec(current->pid);
    print(" exited\n");
    for (;;) hlt();
}

void tasks_init(void) {
    memset(tasks, 0, sizeof(tasks));
    memset(&boot_task, 0, sizeof(boot_task));
    boot_task.pid   = 0;
    boot_task.state = T_RUNNING;
    current = &boot_task;
}

task_t *task_create(void (*entry)(void), int is_user) {
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state != T_EMPTY) continue;
        task_t *t = &tasks[i];
        memset(t, 0, sizeof(*t));
        t->pid        = next_pid++;
        t->state      = T_READY;
        t->entry      = entry;
        t->is_user    = is_user;
        t->kstack     = kmalloc(4096);
        if (!t->kstack) return NULL;
        t->kstack_top = (uint32_t)(uintptr_t)t->kstack + 4096;
        /* 初始 context 布在【新任务自己的内核栈顶】（xv6 式）：
         * switch_to 加载 kesp 后，pops/ret 都发生在真实栈上，
         * ret 直接进蹦床。绝不能把任务结构体当栈用。 */
        struct context *ctx = (struct context *)(uintptr_t)
                              (t->kstack_top - sizeof(struct context));
        memset(ctx, 0, sizeof(*ctx));
        ctx->eip = (uint32_t)(uintptr_t)
                   (is_user ? (void *)utask_trampoline : (void *)ktask_trampoline);
        t->kesp = (uint32_t)(uintptr_t)ctx;
        if (is_user) {
            /* S3-M2：用户任务 = 自己的页目录 + 预制 iret 帧（帧地址
             * 经 ctx.ebx 交给 utask_trampoline，见 cpu.asm） */
            t->pd   = page_dir_create();
            t->ueip = USER_BASE;        /* M4 起 execve/ELF 会改写这两项 */
            t->uesp = USER_TOP;
            uint32_t *frame = (uint32_t *)(uintptr_t)
                              ((uint8_t *)ctx - 20);     /* ctx 正下方 */
            frame[0] = t->ueip;         /* eip */
            frame[1] = 0x1B;            /* cs（用户码，RPL=3） */
            frame[2] = 0x202;           /* eflags：IF=1 */
            frame[3] = t->uesp;         /* esp（用户栈顶） */
            frame[4] = 0x23;            /* ss（用户数据） */
            ctx->ebx = (uint32_t)(uintptr_t)frame;
        }
        return t;
    }
    return NULL;
}

/* fork：复制父的用户地址空间（COW）与 ring3 现场，子返回 0、父返回 pid */
task_t *task_clone(task_t *parent, struct regs *r) {
    task_t *t = 0;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == T_EMPTY) { t = &tasks[i]; break; }
    }
    if (!t) return 0;
    memset(t, 0, sizeof(*t));
    t->pid        = next_pid++;
    t->ppid       = parent->pid;
    t->state      = T_READY;
    t->is_user    = 1;
    t->kstack     = kmalloc(4096);
    if (!t->kstack) return 0;
    t->kstack_top = (uint32_t)(uintptr_t)t->kstack + 4096;

    /* 内核栈布局（自顶向下）：context | iret帧 | 段+寄存器影像 */
    struct context *ctx = (struct context *)(uintptr_t)
                          (t->kstack_top - sizeof(struct context));
    uint32_t *iret = (uint32_t *)(uintptr_t)((uint8_t *)ctx - 20);
    uint32_t *regs = (uint32_t *)(uintptr_t)((uint8_t *)iret - 48);

    regs[0] = 0x23; regs[1] = 0x23; regs[2] = 0x23; regs[3] = 0x23; /* gs fs es ds */
    regs[4] = r->edi; regs[5] = r->esi; regs[6] = r->ebp; regs[7] = 0;
    regs[8] = r->ebx; regs[9] = r->edx; regs[10] = r->ecx;
    regs[11] = 0;                            /* eax=0：子进程 fork() 返回 0 */
    iret[0] = r->eip;                        /* 从 int 0x80 的下一条用户指令继续 */
    iret[1] = r->cs;
    iret[2] = r->eflags;
    iret[3] = r->esp;
    iret[4] = r->ss;

    ctx->eip = (uint32_t)(uintptr_t)utask_trampoline_forked;
    ctx->ebx = (uint32_t)(uintptr_t)regs;
    t->kesp  = (uint32_t)(uintptr_t)ctx;
    t->ueip  = r->eip;
    t->uesp  = r->esp;

    t->pd = page_dir_create();
    page_copy_space(t->pd, parent->pd);
    return t;
}

/* 时间片轮转：从上次调度的下一个槽位开始找 READY */
void schedule(void) {
    static int last = -1;
    task_t *prev = current;
    task_t *next = NULL;
    for (int i = 1; i <= MAX_TASKS; i++) {
        task_t *t = &tasks[(last + i) % MAX_TASKS];
        if (t->state == T_READY) { next = t; last = (int)(t - tasks); break; }
    }
    if (!next) return;
    if (prev->state == T_RUNNING) prev->state = T_READY;
    current = next;
    next->state = T_RUNNING;
    if (next->is_user) tss_set_esp0(next->kstack_top);
    /* S3-M2：地址空间随任务切换——换任务必换 CR3（内核任务共享 boot_pd） */
    uint32_t *npd = next->pd ? next->pd : page_kernel_pd();
    if (npd != active_pd) load_cr3((uint32_t)npd);
    switch_to(&prev->kesp, next->kesp);
}

/* wait 的扫描半边：找到一个属于 pid 的死子并回收（栈/页目录/槽位） */
task_t *task_wait_scan(uint32_t pid) {
    for (int i = 0; i < MAX_TASKS; i++) {
        task_t *t = &tasks[i];
        if (t->state == T_DEAD && t->ppid == pid) {
            if (t->kstack) kfree(t->kstack);
            if (t->pd) page_dir_destroy(t->pd);
            memset(t, 0, sizeof(*t));            /* 槽位回 T_EMPTY */
            return t;                            /* 注意：pid 已被清零，调用方先存 */
        }
    }
    return 0;
}

void sched_loop(void) {                 /* 主 CPU 变成调度器，永不再返回 */
    for (;;) {
        sti();
        hlt();                          /* 等 tick */
        schedule();
    }
}

void task_yield(void) {                 /* 内核任务主动让出：软触发时钟中断 */
    __asm__ volatile("int $0x20");
}

/* ---------- 示例内核任务 ---------- */
void task_a(void) {
    int runs = 0;
    for (;;) {
        if (runs < 3) {
            print("taskA run ");
            print_dec((uint32_t)(runs + 1));
            print("\n");
        }
        runs++;
        task_yield();
    }
}

void task_b(void) {
    int runs = 0;
    for (;;) {
        if (runs < 3) {
            print("taskB run ");
            print_dec((uint32_t)(runs + 1));
            print("\n");
        }
        runs++;
        task_yield();
    }
}

void kbd_task(void) {                   /* 有键回显（无头测试时静默） */
    for (;;) {
        int c;
        while ((c = kbd_pop()) >= 0) {
            if (c >= 32 && c < 127) {
                char b[2] = { (char)c, 0 };
                print("kbd: ");
                print(b);
                print("\n");
            }
        }
        task_yield();
    }
}
