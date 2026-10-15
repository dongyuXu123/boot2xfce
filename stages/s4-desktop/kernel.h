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

struct regs;                               /* 前置声明：task_clone 的参数用 */

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
static inline uint32_t read_eflags(void) {
    uint32_t v;
    __asm__ volatile("pushfl; popl %0" : "=r"(v));
    return v;
}
static inline uint32_t read_cr3(void) {
    uint32_t v;
    __asm__ volatile("mov %%cr3,%0" : "=r"(v));
    return v;
}
/* load_cr3 必须同步记录 active_pd：缺页处理全靠它找当前页目录。
 * （曾经的 bug：只动 CR3 不记变量，切任务后 COW/按需全写进幽灵表） */
extern uint32_t *active_pd;
static inline void load_cr3(uint32_t pa) {
    active_pd = (uint32_t *)(uintptr_t)pa;
    __asm__ volatile("mov %0,%%cr3" :: "r"(pa));
}
static inline void reload_cr3(void) {
    uint32_t cr3;
    __asm__ volatile("mov %%cr3,%0" : "=r"(cr3));
    __asm__ volatile("mov %0,%%cr3" :: "r"(cr3));
}
static inline void enable_paging(void) {
    uint32_t cr0;
    __asm__ volatile("mov %%cr0,%0" : "=r"(cr0));
    __asm__ volatile("mov %0,%%cr0" :: "r"(cr0 | 0x80000000u));
}

/* ---------- console.c（照 vt.c 的 VGA 控制台） ---------- */
void console_init(void);
void console_write(const char *s, uint32_t n);
void debug_ser(const char *s);           /* 直打串口（诊断用） */
void debug_hex(uint32_t v);
void console_clear(void);
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
uint32_t pmm_alloc_frame_low(void);     /* PDE0 可见区：内核要预处理的目标帧 */
void     pmm_free_frame(uint32_t addr);
void heap_init(void);
void *kmalloc(uint32_t n);
void  kfree(void *p);

/* ---------- page.c（S3 分页：恒等内核 + 每任务用户窗口） ---------- */
#define USER_SPAN 0x01000000u            /* 每任务用户区虚拟窗口 16MiB */
void paging_init(void);
uint32_t *page_dir_create(void);
void page_dir_destroy(uint32_t *pd);
void page_map(uint32_t *pd, uint32_t va, uint32_t pa, int user, int rw);
void page_unmap(uint32_t *pd, uint32_t va);
uint32_t page_va2pa(uint32_t *pd, uint32_t va);
uint32_t *current_pd(void);
uint32_t *page_kernel_pd(void);
void page_copy_space(uint32_t *dst, uint32_t *src);
uint32_t task_frame_unref(uint32_t pa);
void page_dir_clear_user(uint32_t *pd);
void page_fault_handler(uint32_t err, uint32_t va);

/* ---------- ramfs.c ---------- */
void ramfs_init(void);
int  ramfs_write(const char *path, const void *buf, uint32_t len);
int  ramfs_read(const char *path, void *buf, uint32_t max);
int  ramfs_mkdir(const char *path);
int  ramfs_rm(const char *path);
int  ramfs_chdir(const char *path);
int  ramfs_getcwd(char *buf, int max);
void ramfs_list(const char *path);      /* 打印目录内容（0=当前目录） */
struct vnode;
struct vnode *ramfs_lookup_vnode(const char *path);

/* ---------- file.c（S3-M5 VFS + fd 表） ---------- */
struct task;                                 /* 前置声明（task_t 在下文 typedef） */
#define FD_MAX 8
struct vnode;
struct file_ops {
    int (*read)(struct vnode *v, void *buf, uint32_t len, uint32_t off);
    int (*write)(struct vnode *v, const void *buf, uint32_t len, uint32_t off);
};
struct vnode { const struct file_ops *ops; void *data; };
struct file  { struct vnode *vn; uint32_t off; int used; };
int  fd_open(struct task *t, const char *kname);       /* 内核侧名字 */
int  fd_read(struct task *t, int fd, void *kbuf, uint32_t n);
int  fd_write(struct task *t, int fd, const void *kbuf, uint32_t n);
int  fd_close(struct task *t, int fd);
int  fd_lseek(struct task *t, int fd, int32_t off, int whence);
int  uptr_ok(uint32_t va, uint32_t n);            /* 用户指针范围校验 */

/* ---------- fb.c（S4 LFB 图元） ---------- */
void fb_init(void);
void fb_pixel(uint32_t x, uint32_t y, uint32_t rgb);
void fb_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t rgb);
void fb_blit(void);
void fb_char(uint32_t x, uint32_t y, char c, uint32_t rgb);
void fb_text(uint32_t x, uint32_t y, const char *s, uint32_t rgb);
extern uint32_t fb_addr, fb_pitch, fb_w, fb_h;

extern struct mb_info *g_mb;

/* ---------- mouse.c（S4 PS/2 鼠标） ---------- */
void mouse_init(void);
void mouse_intr(void);
int  mouse_pop(uint8_t *btn, int16_t *dx, int16_t *dy);

/* ---------- rtc.c（S4 实时钟） ---------- */
uint32_t rtc_time(void);

/* ---------- gfx/win（S4 桌面） ---------- */
void gfx_start(void);
void gfx_task(void);
int  gfx_active(void);
int  gfx_spawn_window(uint32_t pid, const char *title);
void gfx_on_exit(uint32_t pid);
void gfx_clear_focused(void);
int  tty_read_window(uint32_t pid, char *out, int max);
void tty_write_window(uint32_t pid, const char *s);
void gfx_console_write(uint32_t pid, const char *s, uint32_t n);
int  win_by_pid_ok(uint32_t pid);

/* 图形窗口（canvas 模式）：应用经 syscall 作画/收事件，见 win.c 顶部注释 */
struct wmev { int type; long a, b; };  /* 1=键(a=ASCII) 2=点击(a=x,b=y 窗内) */
int  gfx_mode_on(uint32_t pid);
int  gfx_rect(uint32_t pid, int x, int y, int w, int h, uint32_t color);
int  gfx_text(uint32_t pid, int x, int y, uint32_t color, const char *s);
int  gfx_line(uint32_t pid, int x0, int y0, int x1, int y1, uint32_t color);
int  gfx_circle(uint32_t pid, int cx, int cy, int r, uint32_t color);
int  gfx_ev_pop(uint32_t pid, struct wmev *out);

/* ---------- tty.c（S3-M6 行规程） ---------- */
void tty_init(void);
void tty_feed(char c);
void tty_poll(void);

/* ---------- task.c ---------- */
/* T_EMBRYO：构造期状态——内核栈/页目录/iret 帧还没就位的任务对调度器
 * 必须不可见。IRQ0 的 schedule() 是抢占点：曾因创建即 T_READY，半成品
 * 任务被 tick 调度，蹦床从 kmalloc 回收内存里弹垃圾 iret 帧 → 用户态
 * 取指内核地址（PAGE FAULT err=5，偶发）。spawn/fork 构造完才转 READY。 */
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
    int          if_on;                 /* 本任务应持有的 IF（switch_to 不携带 EFLAGS，
                                         * 协作切换发生在 int80 门内会漏 IF=0 给被切入者；
                                         * 切回点按此恢复——毒化/饿死的总根因） */
    uint32_t     kesp;                  /* 被换出时的内核栈指针（switch_to 保存/加载） */
    uint8_t     *kstack;                /* kmalloc 的 4KiB 内核栈 */
    uint32_t     kstack_top;
    int          is_user;               /* ring3 任务（iret 进入） */
    void       (*entry)(void);          /* 内核任务入口 */
    uint32_t    *pd;                    /* S3：本任务页目录（0=共享内核 boot_pd） */
    uint32_t     ppid;                  /* 父 pid（0=内核祖宗） */
    uint32_t     ueip, uesp;            /* ring3 进入点（创建/execve 时定） */
    struct file  fds[FD_MAX];           /* S3-M5：每任务打开文件表 */
} task_t;

extern task_t *current;
void tasks_init(void);
task_t *task_create(void (*entry)(void), int is_user);
task_t *task_clone(task_t *parent, struct regs *r);   /* fork：复制地址空间+现场 */
uint32_t task_wait_scan(uint32_t pid);
int  task_spawn(const char *path, uint32_t ppid, const char *title);
void task_reap_orphans(void);                /* 收尸一个死子，返回其 pid */
int  task_exec_image(task_t *t, const char *name);    /* ELF 换像（M4） */
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
