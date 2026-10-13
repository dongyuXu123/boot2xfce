/* L14 桩 —— 任务表 / 创建 / 轮转调度（task_a/b/kbd_task 给定，L18 用） */
#include "kernel.h"

#define MAX_TASKS 16
static task_t tasks[MAX_TASKS];
static task_t boot_task;                /* kmain 所在的"第 0 任务" */
task_t *current = &boot_task;
static uint32_t next_pid = 1;

extern void utask_trampoline(void);     /* cpu.asm */

/* 内核任务蹦床：入口函数跑完 → 标记 DEAD → 永久休眠（栈留到 S3 收尸） */
static void ktask_trampoline(void) {
    /* TODO(你来写·L14): 三步——
     * 1) sti()   ← 别漏！switch_to 不保存 EFLAGS，第一次被切进来时 IF
     *              还是中断门清掉的 0，硬件中断从此全瞎（L18 会实测）
     * 2) 执行 current->entry()
     * 3) current->state 置 T_DEAD，然后 for(;;) hlt();
     * 想想为什么 entry() 之前 current 已经指向本任务（谁设置的？何时？） */
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
    /* TODO(你来写·L14):
     * 1) 找一个 T_EMPTY 槽位，memset 清零
     * 2) 填 pid(自增)/T_READY/entry/is_user
     * 3) kstack = kmalloc(4096)，kstack_top = kstack+4096
     * 4) 【本课灵魂】在"新任务自己的内核栈顶"布置初始 context：
     *      struct context *ctx = kstack_top - sizeof(struct context);
     *      清零后 ctx->eip = 蹦床地址（内核任务 ktask_trampoline，
     *      用户任务 utask_trampoline）；t->kesp = (uint32_t)ctx
     *    ⚠ 绝不能把 &tasks[i].kctx 之类的地址当 kesp——那等于把
     *      .bss 里的结构体当栈用，第一次切换就把任务表踩穿
     *      （全课程最毒的坑，欢迎踩完再来读这段注释）
     * 5) is_user 时填 utask_eip=USER_BASE, utask_esp=USER_TOP
     */
    (void)entry; (void)is_user;
    return NULL;
}

/* 时间片轮转：从上次调度的下一个槽位开始找 READY */
void schedule(void) {
    /* TODO(你来写·L14):
     * 1) static int last = -1; 从 last+1 起环形扫描 T_READY
     * 2) 没找到就直接 return（全死光了也别崩）
     * 3) prev->state: RUNNING→READY；current=next；next->state=RUNNING
     * 4) next->is_user 时 tss_set_esp0(next->kstack_top)
     * 5) switch_to(&prev->kesp, next->kesp)
     * 想想：为什么 prev 要先取出来？为什么 RUNNING(不是 READY)的任务跳过？ */
}

void sched_loop(void) {                 /* 主 CPU 变成调度器，永不再返回 */
    /* TODO(你来写·L14): for(;;) { sti(); hlt(); schedule(); }
     * 想想：sti 在 hlt 前还是后？为什么这样写 idle 也不丢 tick？ */
    for (;;) { cli(); hlt(); }
}

void task_yield(void) {                 /* 内核任务主动让出：软触发时钟中断 */
    /* TODO(你来写·L14): 一行内联汇编，软触发 vector 0x20（=IRQ0 的门） */
}
