/* ============================================================
 * S2 内核公共头 —— 类型 / 各子系统接口 / 关键数据结构
 *
 * 无 libc（-nostdinc）：类型与基础函数自带。
 * 设计总览见 docs/20-kernel.md：
 *   mm.c    物理页位图 + 固定区堆（kmalloc/kfree）
 *   task.c  任务表 + 软件上下文切换 + 时间片轮转（PIT 100Hz）
 *   ramfs.c 内存盘（create/write/read/list）
 *   syscall int 0x80（write/getpid/exit/uptime/ls）+ ring3 演示
 *   （分页/虚存保护是 S3 的活：当前是平坦模型，ring3 无隔离）
 * ============================================================ */
#ifndef KERNEL_H
#define KERNEL_H

/* ---------- 基础类型 ---------- */
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef signed char        int8_t;
typedef short              int16_t;
typedef int                int32_t;
typedef unsigned long long uint64_t;
typedef uint32_t           uintptr_t;

#define NULL ((void *)0)

/* ---------- Multiboot 信息结构（只用 bit0/bit6，与 S1 相同） ---------- */
struct mb_info {
    uint32_t flags;
    uint32_t mem_lower, mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count, mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length, mmap_addr;
} __attribute__((packed));
struct mb_mmap {
    uint32_t size;
    uint64_t addr, len;
    uint32_t type;      /* 1=可用 */
} __attribute__((packed));

void *memset(void *d, int c, uint32_t n);
void *memcpy(void *d, const void *s, uint32_t n);

/* ---------- 端口 IO ---------- */
static inline void outb(uint16_t port, uint8_t v) {
    __asm__ volatile("outb %0,%1" :: "a"(v), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t r;
    __asm__ volatile("inb %1,%0" : "=a"(r) : "Nd"(port));
    return r;
}
static inline void cli(void)      { __asm__ volatile("cli"); }
static inline void sti(void)      { __asm__ volatile("sti"); }
static inline void hlt(void)      { __asm__ volatile("hlt"); }
static inline uint32_t read_cr2(void) {
    uint32_t v;
    __asm__ volatile("mov %%cr2,%0" : "=r"(v));
    return v;
}

/* ---------- console.c ---------- */
void console_init(void);
void print(const char *s);              /* 串口 + VGA 双路 */
void print_hex(uint32_t v);
void print_dec(uint32_t v);

/* ---------- gdt.c ---------- */
void gdt_init(void);
void tss_set_esp0(uint32_t esp0);

/* ---------- idt.c ---------- */
void idt_init(void);

/* ---------- irq.c（PIC + PIT + 键盘 + 中断分发） ---------- */
void pic_init(void);
void pit_init(uint32_t hz);
extern volatile uint32_t ticks;
int  kbd_pop(void);                     /* 有键返回 ASCII，无键返回 -1 */

/* ---------- mm.c ---------- */
#define HEAP_START 0x200000u            /* 2MiB */
#define HEAP_END   0x400000u            /* 4MiB */
#define USER_BASE  0x400000u            /* 用户程序加载处（紧随堆） */
#define USER_TOP   0x4F0000u            /* 用户栈顶（往下生长） */
void pmm_init(struct mb_info *mb);
uint32_t pmm_total_frames(void);
uint32_t pmm_alloc_frame(void);
void     pmm_free_frame(uint32_t addr);
void heap_init(void);
void *kmalloc(uint32_t n);
void  kfree(void *p);

/* ---------- ramfs.c ---------- */
void ramfs_init(void);
int  ramfs_write(const char *name, const void *buf, uint32_t len);
int  ramfs_read(const char *name, void *buf, uint32_t max);  /* 返回实际长度 */
void ramfs_list(void);                  /* 打印文件表 */

/* ---------- task.c ---------- */
typedef enum { T_EMPTY, T_EMBRYO, T_READY, T_RUNNING, T_DEAD } tstate_t;

/* switch_to 保存的内核上下文（与 cpu.asm 压栈顺序严格对应：
 * push ebx,esi,edi,ebp 后 ebp 位于最低地址，ret 取 eip） */
struct context {
    uint32_t ebp;
    uint32_t edi;
    uint32_t esi;
    uint32_t ebx;
    uint32_t eip;
};

typedef struct task {
    uint32_t     pid;
    tstate_t     state;
    uint32_t     kesp;                  /* 被换出时的内核栈指针（switch_to 保存/加载） */
    uint8_t     *kstack;                /* kmalloc 的 4KiB 内核栈 */
    uint32_t     kstack_top;
    int          is_user;               /* ring3 任务（iret 进入） */
    void       (*entry)(void);          /* 内核任务入口 */
} task_t;

extern task_t *current;
void tasks_init(void);
task_t *task_create(void (*entry)(void), int is_user);
void schedule(void);                    /* 时间片轮转（须在关中断下调用） */
void sched_loop(void) __attribute__((noreturn));
void task_yield(void);                  /* 内核任务主动让出 CPU */
void kbd_task(void);
void task_a(void);
void task_b(void);

/* ---------- 中断现场（与 isr.asm 压栈顺序严格对应，低→高） ---------- */
struct regs {
    uint32_t int_no;
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp_, ebx, edx, ecx, eax;
    uint32_t eip, cs, eflags, esp, ss;
};
void isr_dispatch(struct regs *r);
void syscall_dispatch(struct regs *r);
extern volatile uint32_t isr_err_code;  /* 异常错误码（isr.asm 塞进来） */

/* ---------- cpu.asm ---------- */
void gdt_flush(uint32_t gdtr);
void tss_flush(void);
/* old: 保存旧 esp 的槽位指针；new_esp: 新任务内核栈上的 context 地址 */
void switch_to(uint32_t *old_slot, uint32_t new_esp);
extern uint32_t utask_eip, utask_esp;   /* ring3 进入点（单用户任务假设，S3 改） */

#endif /* KERNEL_H */
