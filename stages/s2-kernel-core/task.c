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

extern void utask_trampoline(void);     /* cpu.asm */

/* 内核任务蹦床：入口函数跑完 → 标记 DEAD → 永久休眠（栈留在那儿，S3 收尸） */
static void ktask_trampoline(void) {
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
        if (is_user) {                  /* ring3 进入点：当前只支持一个用户任务 */
            utask_eip = USER_BASE;
            utask_esp = USER_TOP;
        }
        return t;
    }
    return NULL;
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
    switch_to(&prev->kesp, next->kesp);
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
